# hidapi_minimal_13y063-90 (Library for communicating with HID devices of duagon like F229)

## Overview
This library uses IOCTLs of hidraw driver of Linux kernel.

Makefile in this directory builds library hidapi_minimal_13y063-90 that can be used to
develop an application to communicate with F229 HID raw device.

## Prerequisites

* Requires that hidraw driver is available on Linux system. Kernel shall be built
with CONFIG_HIDRAW=y.

* build-essential: gcc, make

## How to build

`make`

This builds static and shared library named as hidapi_minimal_13y063-90.

## Generating doxygen documentation

`cd doc`

`doxygen doxygen.cfg`

## License

The source code files of this library are delivered under MIT license.

## Version history
1. v1.0 - First version of library with all APIs needed to communicate with F229
2. v2.0 - Renamed library name and source files

## Limitations

* Function to get firmware version of the board (HID device) could not be
implemented. libudev is required to read system attribute bcdDevice.
Possibility to find device and its bcdDevice attribute from sys path shall
be researched.

* Function to get geographical address of board (HID device) is not
available due to limitation of hidraw IOCTLs. IOTCL is not available
to get string descriptor at specific index. Libusb does have such
function. Possibility to get string descriptor at index shall be researched.

