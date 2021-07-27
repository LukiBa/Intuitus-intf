# Based on setup.py by Benjamin Kulnik: https://github.com/marbleton/FPGA_MNIST
# Autor: Lukas Baischer

# System imports
from distutils.core import *
from distutils      import sysconfig
import glob
import pathlib 

# Third-party modules - we depend on numpy for everything
import numpy
import wget
from setuptools import Extension
from setuptools import setup, find_packages
from setuptools.command.build_ext import build_ext

# Obtain the numpy include directory.  This logic works across numpy versions.
try:
    numpy_include = numpy.get_include()
except AttributeError:
    numpy_include = numpy.get_numpy_include()

def readme():
    with open('./README.md') as f:
        return f.read()


def download_numpy_interface(path):
    """
    Downloads numpy.i
    :return: None
    """
    print("Download Numpy SWIG Interface")
    np_version = re.compile(r'(?P<MAJOR>[0-9]+)\.'
                            '(?P<MINOR>[0-9]+)') \
        .search(numpy.__version__)
    np_version_string = np_version.group()
    np_version_info = {key: int(value)
                       for key, value in np_version.groupdict().items()}

    np_file_name = 'numpy.i'
    np_file_url = 'https://raw.githubusercontent.com/numpy/numpy/maintenance/' + \
                  np_version_string + '.x/tools/swig/' + np_file_name
    if np_version_info['MAJOR'] == 1 and np_version_info['MINOR'] < 9:
        np_file_url = np_file_url.replace('tools', 'doc')

    wget.download(np_file_url, path)

    return

file_root = pathlib.Path(__file__).absolute().parent
src_dir = file_root / "intuitus_nn/src"
pkg_dir = file_root / "intuitus_nn"

# Download numpy.i if needed
if not os.path.exists(str(pkg_dir / 'numpy.i')):
    print('Downloading numpy.i')
    project_dir = os.path.dirname(os.path.abspath(__file__))
    download_numpy_interface(path=str(file_root))


print(str(src_dir))
# gather up all the source files
srcFiles = [str(pkg_dir / 'intuitus.i'),str(src_dir / 'intuitus.cpp'),str(src_dir / 'fb' / 'framebuffer.cpp'),str(src_dir / 'cam' / 'v4l_camera.cpp')]
includeDirs = [numpy_include]
#srcDir = os.path.abspath('driver-intf')
#for root, dirnames, filenames in os.walk(srcDir):
#  for dirname in dirnames:
#    absPath = os.path.join(root, dirname)
#    print('adding dir to path: %s' % absPath)
#    globStr = "%s/*.c*" % absPath
#    files = glob.glob(globStr)
#    print(files)
#    includeDirs.append(absPath)
#    srcFiles += files
includeDirs.append(str(src_dir))
includeDirs.append(str(src_dir/'fb'))
includeDirs.append(str(src_dir/'cam'))

print("************************ Include dirs *************************")
print(includeDirs)
print("************************ source files *************************")
print(srcFiles)

# set the compiler flags so it'll build on different platforms (feel free
# to file a  pull request with a fix if it doesn't work on yours)
if sys.platform == 'darwin':
    # default to clang++ as this is most likely to have c++11 support on OSX
    if "CC" not in os.environ or os.environ["CC"] == "":
        os.environ["CC"] = "clang++"
        # we need to set the min os x version for clang to be okay with
        # letting us use c++11; also, we don't use dynamic_cast<>, so
        # we can compile without RTTI to avoid its overhead
        extra_args = ["-stdlib=libc++",
          "-mmacosx-version-min=10.7","-fno-rtti",
          "-std=c++0x"]  # c++11
else: # only tested on travis ci linux servers
    os.environ["CC"] = "g++" # force compiling c as c++
    extra_args = ['-std=c++0x','-fno-rtti']

# inplace extension module
_intuitus_nn = Extension("_intuitus_nn",
                   sources=srcFiles,
                   include_dirs=includeDirs,
                   swig_opts=['-c++'],
                   extra_compile_args=extra_args,
                   extra_link_args=[],
                   depends=['numpy'],
                   optional=True)

# NumyTypemapTests setup
setup(  name        = "intuitus_nn",
        description = "Intuitus CNN accelerator driver interface.",
        author      = "Lukas Baischer",
        author_email= "lukas_baischer@gmx.at",
        version     = "0.1",
        packages    = find_packages(),
        package_dir = {"": "intuitus_nn"},
        package_data={
          '': ['*.txt', '*.md', '*.i', '*.c', '*.h', '*.py', '*.cpp', '*.hpp'],
        },
        url         = 'https://github.com/LukiBa/Intuitus-lnx',
        license     = "MIT",
        ext_modules = [_intuitus_nn],
        install_requires=['numpy', 'pathlib', 'elevate']
        )