from setuptools import setup, Extension
from sys import platform
import pybind11 as pb

if platform == 'darwin':
    lib_path = ['/opt/homebrew/lib','/usr/local/lib']
    inc_path = [pb.get_include(), '/opt/homebrew/include', '/usr/local/include']
    link_lib = ['rtlsdr']
elif platform == 'linux':
    lib_path = ['/usr/local/lib']
    inc_path = [pb.get_include(), '/usr/local/include']
    link_lib = ['rtlsdr']

rtlsdr = Extension ( 'rtlsdr',
                    define_macros = [('MAJOR_VERSION',1), ('MINOR_VERSION',0)],
                    include_dirs = inc_path,
                    libraries = link_lib,
                    library_dirs = lib_path,
                    runtime_library_dirs = lib_path,
                    extra_compile_args = ['-std=c++11'],
                    sources = ['wrapper.cpp'])

setup ( name = 'rtlsdr',
        version = '1.0',
        description = 'SDRPlay support',
        author = 'Paul Colby',
        author_email = 'paulccolby@earthlink.net',
        long_description = '''
Support for RTLSDR
''',
        ext_modules = [rtlsdr])
