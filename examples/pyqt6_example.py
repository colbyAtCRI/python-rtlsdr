import pyqtgraph as pg
from pyqtgraph import QtWidgets, QtCore, QtGui
from PyQt6.QtCore import Qt
from rtlsdr import RTLSDR
import numpy as np
from numpy.fft import fft, fftfreq, fftshift

pg.mkQApp()

class Radio (QtCore.QObject):

    onData = QtCore.pyqtSignal (tuple)

    def __init__(self):
        super().__init__()
        #self.rate = 600000
        self.rate = 2048000
        self.radio = RTLSDR()
        ## San Jose airport control tower at 120100000
        self.center_freq = 120100000
        ## NPR fm station
        self.center_freq = 88500000 
        self.radio.sample_rate = 2048000
        self.agc = True
        self.radio.onIQData = self
        #self.radio.tuner_gain = self.radio.tuner_gains[-4]
        self.pwr = np.zeros (2048,np.float32)
        self.iq = []
        self.offset = 0
        self.count = 0

    def __call__ (self, iq):
        ## DC correction
        self.offset = 0.99 * self.offset + 0.01 * np.average(iq)
        iq = iq - self.offset
        self.iq = np.concatenate ((self.iq,iq))
        while len(self.iq) > 4096:
            spec = fftshift(fft (self.iq[0:4096]))/4096
            spec = spec[1024:-1024]
            pwr = 10.0 * np.log10 ((spec * spec.conj()).real)
            ## Keep a running power average using a one tap filter
            self.pwr = 0.9 * self.pwr + 0.1 * pwr
            self.iq = self.iq[4096:]
            self.count = self.count + 1
            ## cut down the display refresh to make it less fernetic
            if self.count > 20:
                self.count = 0
                self.onData.emit ((self.freqs,self.pwr))

    @property
    def center_freq (self):
        return self.freq

    @center_freq.setter
    def center_freq (self, freq):
        self.freq = freq
        self.freqs = fftshift(fftfreq(4096) * self.rate + freq) / 1000000
        self.freqs = self.freqs[1024:-1024]
        self.radio.freq = int(freq)

class FrequencyDisplay (QtWidgets.QLineEdit):

    def __init__(self):
        super().__init__()
        self.setInputMask ('9,999,999,999')
        self.setFont (QtGui.QFont ('Arial',25))
        self.setAlignment (Qt.AlignmentFlag.AlignHCenter)
        self.textChanged.connect (self.onChange)
        self.editingFinished.connect (self.onComplete)
        policy = self.sizePolicy()
        policy.setHorizontalPolicy (QtWidgets.QSizePolicy.Policy.Fixed)
        self.setSizePolicy (policy)
        self.style_done = 'color: green'
        self.style_edit = 'color: darkred'

    def keyPressEvent (self, key):
        ## Filter out delete
        if key.key() == 16777219 or key.key() == 16777223:
            return
        if key.key() == 16777220 or key.text() == '' or key.text().isdigit():
            super().keyPressEvent (key)

    def onChange (self):
        self.setStyleSheet (self.style_edit)

    def onComplete (self):
        self.setStyleSheet (self.style_done)

    def setFreq (self, freq):
        text = str(freq)
        text = '0' * (10-len(text)) + text
        self.setText (text)
        self.onComplete ()

    def getFreq (self):
        return int(self.text().replace(',',''))

class Spectrum (pg.PlotWidget):

    def __init__ (self,radio):
        super().__init__()
        self.radio = radio
        self.pen = pg.mkPen (color=(0,255,0))
        self.dataLine = self.plot()
        self.viewport().setAttribute (Qt.WidgetAttribute.WA_AcceptTouchEvents,False)
        self.setDefaultPadding (0)
        self.setYRange (0,-80)
        self.showGrid (x=True, y=True, alpha=0.7)
        self.setMouseEnabled (x=False,y=False)
        self.hideButtons ()
        self.radio.onData.connect (self.redraw)

    def redraw (self, data):
        self.setXRange (data[0][0],data[0][-1])
        self.dataLine.setData (data[0],data[1],pen=self.pen)

class MainWindow (QtWidgets.QWidget):

    def __init__(self,radio):
        super().__init__()
        self.radio = radio
        self.setWindowTitle (f'{radio.radio.name}:{radio.radio.serial_number}')
        self.setLayout (QtWidgets.QVBoxLayout())
        layout = QtWidgets.QHBoxLayout()
        self.spec = Spectrum (radio)
        layout.addWidget(self.spec)
        self.layout().addLayout (layout)
        layout = QtWidgets.QHBoxLayout ()
        self.freq = FrequencyDisplay ()
        layout.addWidget (QtWidgets.QWidget())
        layout.addWidget (self.freq)
        layout.addWidget (QtWidgets.QWidget())
        self.layout().addLayout (layout)
        self.freq.setFreq (self.radio.center_freq)
        self.freq.editingFinished.connect (self.changeFreq)

    def changeFreq (self):
        self.radio.center_freq = self.freq.getFreq()
 
if __name__ == '__main__':
    radio = Radio ()
    display = MainWindow (radio)
    display.show()
    radio.radio.running = True
    pg.exec ()
