from rtlsdr import RTLSDR
import numpy as np
from time import sleep

radio = RTLSDR ()
radio.freq = 88500000
radio.sample_rate = 2048000
radio.tuner_gain = radio.tuner_gains[-4]
radio.running = True

class Process:

    def __init__ (self):
        self.power = 0
        self.dlen = 0

    def __call__ (self, iq):
        self.dlen = len(iq)
        spec = 10.0 * np.log10 ( (iq * iq.conj()).real )
        self.power = np.average (spec)
        print (self.power)

callb = Process()

radio.onIQData = callb

sleep(1)

radio.running = False
