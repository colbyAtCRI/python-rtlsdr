# python-rtlsdr
A pybind11 interface to librtlsdr

# building and requirements

clone this repo into a local file. 

```
git clone https://github.com/colbyAtCRI/python-rtlsdr.git

cd python-rtlsdr

```

Typing make will generate a python module called rtllsdr
which will be installed to your active python environment
as a link to the python-rtlsdr directory. 

The above will fail unless all requirements are met.
One must install or have

- a working c++ build environment
- a working python3 (3.11 or greater)
- librtlsdr  (brew install or equivalent)
- pybind11   (pip)
- numpy      (pip)
- setuptools (pip)
- wheel      (pip)

# Using it

Once installed, the package rtlsdr supplies two things, catalog and RTLSDR.
The function catalog returns a list of rtl devices pluged into USB ports. 
 
```
from rtlsdr import catalog, RTLSDR

if catalog():
    radio = RTLSDR ()
    help(radio)
```
