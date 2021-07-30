# Intuitus interface
![GitHub](https://img.shields.io/github/license/LukiBa/Intuitus-intf)

Python to linux device driver interface wrapper for Intuitus CNN hardware accelerator. 
Creates high level interface for CNN model design similar to well known Keras wrapper. 

## Features

- [x] v4l2 camera wrapper 
- [x] framebruffer wrapper 
- [x] conv2d (kernel sizes: [1x1,3x3,5x5]; strides: [1,2])
- [x] inplace maxpool2d (stride 2 only)
- [x] maxpool2d
- [x] upsample
- [x] concat
- [x] split
- [ ] inplace yolo layer  
- [ ] fully connected
- [ ] inverse bottleneck 
- [ ] residual 

## Installation
````sh
cd Intuitus-intf
pip install -e .
````

## Build Requirements
#### Compiler:
- swig >= 3.0.10
#### C++ Libs:
- opencv
- v4l2
#### Python:
- python >= 3.6.10
- numpy >= 1.19.5
- pathlib
- wget
- setuptools

## Usage 
- For an example how to build a network using Intuitus Interface see Yolov3-tiny example application for Zybo-Z7-20 board 
- To convert a torch or keras model into commands which can be interpreted by the Intuitus Interface see Intuitus Model Converter 

## Usage Requirements
- Intuitus device driver kernel module: intuitus.ko 
- Programmed FPGA including Intuitus IP 
- Device Tree note for Intuitus

## Related Projects: 
| Project | link |
| ------ | ------ |
| Intuitus Model Converter | <https://github.com/LukiBa/Intuitus-converter.git> |
| Yolov3-tiny example application for Zybo-Z7-20 board | <https://github.com/LukiBa/zybo_yolo.git> |
| Intuitus device driver | link comming soon (contact author for sources) |
| Vivado Example project| https://github.com/LukiBa/zybo_yolo_vivado.git |
| Intuitus FPGA IP | encrypted trial version comming soon (contact author) |

## Author
Lukas Baischer 
lukas_baischer@gmx.at


