from rtlsdr import catalog, RTLSDR

if catalog ():
    radio = RTLSDR()
    help(radio)
