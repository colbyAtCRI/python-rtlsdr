// Use pybind11 to build an RTLSDR device python interface

#include <rtl-sdr.h>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/cast.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <complex>
#include <map>
#include <initializer_list>

#define BUFFERSIZE (16 *  512)
using namespace pybind11::literals;
namespace py = pybind11;

py::list catalog (void) 
{
    py::list cat;
    int ndev = rtlsdr_get_device_count ();
    for (int indx = 0; indx < ndev; indx++) {
        std::string name = rtlsdr_get_device_name (indx);
        cat.append (name);
    }
    return cat;
}

std::map<rtlsdr_tuner,std::string> tuners = {
    {RTLSDR_TUNER_UNKNOWN,"UNKNOWN"},
    {RTLSDR_TUNER_E4000,  "E4000"},
    {RTLSDR_TUNER_FC0012, "FC0012"},
    {RTLSDR_TUNER_FC0013, "FC0013"},
    {RTLSDR_TUNER_FC2580, "FC2580"},
    {RTLSDR_TUNER_R820T,  "R820T"},
    {RTLSDR_TUNER_R828D,  "R828D"}};

void sdr_callback (void *device);

class RTLSDR
{
public:
    unsigned char    *mBuffer;
    py::object        mNp = py::module_::import ("numpy");
    py::object        mComplex64_t = mNp.attr("dtype")("complex64");
    py::object        mArray = mNp.attr("array");
 
    rtlsdr_dev_t     *mDev;
    py::object        mIQData;
    std::atomic<bool> mRunning;
    std::thread      *mProcess;
    std::vector<int>  mGains;
    std::string       mTuner;
    std::string       mName;
    std::string       mSerialNumber;
    bool              mAGC;

    RTLSDR (int indx) {
        char man[256];
        char prd[256];
        char ser[256];
        mIQData = py::none();
        rtlsdr_get_device_usb_strings (indx,man,prd,ser);
        mName = man;
        mSerialNumber = ser;
        rtlsdr_open (&mDev, indx);
        mRunning.store(false);
        mProcess = nullptr;
        mGains = get_tuner_gains ();
        mTuner = tuners[rtlsdr_get_tuner_type (mDev)];
        mBuffer = new unsigned char[BUFFERSIZE];
        set_agc (false);
        set_center_freq (100000000);
        set_sample_rate (2048000);
    }

   ~RTLSDR (void) {
        delete mBuffer;
        rtlsdr_close (mDev);
    }

    void set_center_freq (int freq) {
        if (freq < 2880000)
            rtlsdr_set_direct_sampling (mDev,1);
        else
            rtlsdr_set_direct_sampling (mDev,0);
        rtlsdr_set_center_freq (mDev,freq); 
    }

    int get_center_freq (void) {
        int freq = (int)rtlsdr_get_center_freq (mDev);
        return freq;
    }

    int get_ppm_corection (void) {
        return rtlsdr_get_freq_correction (mDev);
    }

    void set_ppm_correction (int ppm) {
        rtlsdr_set_freq_correction (mDev, ppm);
    }

    bool get_running (void) {
        return mProcess != nullptr;
    }

    void set_running (bool val) {
        if (val and mProcess) {
            mRunning.store(false);
            mProcess->join();
            mProcess = nullptr;
        }
        else {
            mRunning.store(true);
            mProcess = new std::thread (sdr_callback,this);
        }
    }    

    int get_sample_rate (void) {
        return (int) rtlsdr_get_sample_rate (mDev);
    }

    void set_sample_rate (int rate) {
        rtlsdr_set_sample_rate (mDev, rate);
    }

    std::vector<int> get_tuner_gains (void) {
        int ng = rtlsdr_get_tuner_gains (mDev,nullptr);
        std::vector<int> gains(ng);
        rtlsdr_get_tuner_gains (mDev,&gains.at(0));
        return gains;
    }

    int get_tuner_gain (void) {
        return rtlsdr_get_tuner_gain (mDev);
    }

    int set_tuner_gain (int gain) {
        rtlsdr_set_tuner_gain (mDev, gain);
    }

    void set_agc (bool val) {
        mAGC = val;
        if (val) 
            rtlsdr_set_agc_mode (mDev, 1);
        else 
            rtlsdr_set_agc_mode (mDev, 0);
    }

    bool get_agc (void) {
        return mAGC;
    }
};


void sdr_callback (void *device) 
{
    std::cout << "thread started" << std::endl;
    int nr(0);
    std::vector<std::complex<float>> ret;
    RTLSDR *radio = (RTLSDR*)device;
    int8_t *buffer = (int8_t*)radio->mBuffer;
    rtlsdr_reset_buffer (radio->mDev);
    while (radio->mRunning.load()) {
        if (rtlsdr_read_sync (radio->mDev,radio->mBuffer,BUFFERSIZE,&nr)) {
            radio->mRunning.store (false);
            break;
        }
        ret.clear();
        for (int n = 0; n < nr; n += 2) {
           ret.push_back(std::complex<float>(buffer[n]/127.0f,buffer[n+1]/127.0f));
        }
        py::gil_scoped_acquire scope;
            if ( !radio->mIQData.is(py::none()) ) {
                radio->mIQData (radio->mArray(ret,radio->mComplex64_t));
            }
        py::gil_scoped_release release;
    }
    std::cout << "thread stopped" << std::endl;
}

const char *initDoc = R"""(
    The RTLSDR constructor takes a device index which defaults to 0 for the first
radio. Most often only one device is available in which case,

    radio = RTLSDR ()

will suffice to open it. See the catalog function which returns a list of devices.
)""";

const char *nameDoc = R"""(
    device name as returned by usb strings library call.
)""";

const char *serialDoc = R"""(
    device serial number as returned by usb library call. Typically a useless string
shared by all devices even across manufacturers. It is often possible to set this 
value by writing to eeprom. This feature is not supported. You get what you get.
)""";

const char *onIQDataDoc = R"""(
Set to a call back function which expects a numpy array of type complex64 and returns 
None. When running property is set to True, a thread is started which calls,

    onIQData (iq)

where iq is a numpy array containing IQ samples. Clearly one may also set onIQData
to a class which overides the __call__ call. Using a class instead of a function
call has many advantages since the entire class namespace is available inside the 
callback. As an example, lets say our goal in life is to maintain a running average
of the IQ values (which are complex) and their square magnitude.

-------- Example Code -----------
from rtlsdr import RTLSDR
import numpy as np

radio = RTLSDR ()

class Average:

    def __init__(self):
        self.iq_aver = 0
        self.power_aver = 0

    def __call__(self, iq):
        self.iq_aver = 0.9 * self.iq_aver + 0.1 * np.average (iq)
        pwr = np.sum ( 10.0 * np.log10 (( iq * iq.conj()).real))
        self.power_aver = 0.9 * self.power_aver + 0.1 * pwr

average = Average()
radio.onIQData = average

radio.running = True

for n in 10:
    print (f'average IQ: {average.iq_aver} power: {average.power_aver}')
--------- End Example Code --------
)""";

const char *tunerDoc = R"""(
Tuner device name such as 'RT20T' or 'E4000'.  
)""";

const char *tunerGainsDoc = R"""(
A read only list of supported gains. These are used to set the tuner_gain 
property.
)""";

const char *agcDoc = R"""(
Automatic Gain Control enable/disable. Use radio.agc to read the agc state,
radio.agc = True to enable, radio.agc = False to disable the hardware AGC.
)""";

const char *ppmDoc = R"""(
Rtl-sdr dongles are dirt cheap. Their internal frequency sense is at the 
mercy of cheap aging crystal oscilators. The ppm_correction allows the 
user to calibrate the radios center frequency. Since all frequencies are
proportional to the crystal oscillator, adjusting its assumed value will
modify where signals show up by parts per million. 

So, let's say WWV is at exactly 10MHz but your sleazy dongle has it comming
in at 9.997MHz, or 3000Hz low. This means your radio crystal is running too
fast. Now 1ppm at 10MHz is 10Hz. We need to tell the software that the crystal
is 300 ppm faster. By settings the ppm_correction to 300 WWV should then show 
up much closer to 10MHz.
)""";

const char *tunerGainDoc = R"""(
Set to one of the values found in tuner_gains which is in acending order. A
gain of 0 is the lowest possible gain. Usually no signals are above the noise
at this setting so one should set it higher or enable AGC.
)""";

const char *runningDoc = R"""(
The boolean running property starts and stops the IQ data reading thread 
provided by this library. When first instantiated, running is False and
the radio is not collecting IQs. Setting running to True starts a radio
reading thread in which onIQData is called if set. Now, a happy thing is
that onIQData may be set or read independent of running. I've yet to 
encounter time sharing issues because python's Global Iterpreter Lock is 
used to ensure thread safty. This is really one of the best features of
this particular interface.  
)""";

const char *sampleRate = R"""(
The IQ sample rate may be changed or read with this property. The default
value is 204800 Hz. 
)""";

const char *freqDoc = R"""(
Used to set the center frequency of the radio. It's initial value is set
to 100000000 Hz or 100 Mhz just to avoid being starting out at  0. The 
whole RTL faimily work from 28.8Mhz to 1.7Ghz which, for just a few bucks,
isn't shabby. Below 28.8 Mhz, the tuner chip fails to work. Someone along 
the line determined that these tuner chips could be bypassed enabling the
DSP chip to directly sample the rf signal at 25 Mhz or so. To run in this 
mode, one must enable direct sampling. This is boring. Setting freq below 
28.8 Mhz will automatically enable direct sampling mode. Above 28.8 it's
disabled.  
)""";

PYBIND11_MODULE (rtlsdr, m)
{
    m.def ("catalog", &catalog);

    py::class_<RTLSDR> (m,"RTLSDR")
        .def (py::init<int>(), py::arg("index")=0,initDoc)
        .def_readwrite ("onIQData", &RTLSDR::mIQData,onIQDataDoc)
        .def_readonly ("tuner_gains", &RTLSDR::mGains,tunerGainsDoc)
        .def_readonly ("tuner", &RTLSDR::mTuner,tunerDoc)
        .def_readonly ("name", &RTLSDR::mName,nameDoc)
        .def_readonly ("serial_number", &RTLSDR::mSerialNumber,serialDoc)
        .def_property ("agc", &RTLSDR::get_agc, &RTLSDR::set_agc,agcDoc)
        .def_property ("ppm_correction", &RTLSDR::get_ppm_corection, &RTLSDR::set_ppm_correction,ppmDoc)
        .def_property ("tuner_gain", &RTLSDR::get_tuner_gain, &RTLSDR::set_tuner_gain,tunerGainDoc)
        .def_property ("running", &RTLSDR::get_running, &RTLSDR::set_running,runningDoc)
        .def_property ("sample_rate", &RTLSDR::get_sample_rate, &RTLSDR::set_sample_rate)
        .def_property ("freq", &RTLSDR::get_center_freq, &RTLSDR::set_center_freq,freqDoc);
}

