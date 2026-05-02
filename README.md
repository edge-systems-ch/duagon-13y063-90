# 13Y063-90 Linux wireless interface configuration tool for duagon F229, G239 and ME10

**Version 2.02 - 13Y063-90**  
**Version 2.00 - 13Y063-90_minimal**

> See the duagon website for up-to-date downloads and documentation:
www.duagon.com/software/13y063-90/

There are two variants of the 13Y063-90 tool:
* 13Y063-90
* 13Y063-90_minimal  
See chapter **Variants** below for more information.

## Compatibility 

**The 13Y063-90 tool starting v2.01 is backwards compatible to all firmware versions.**  

The 13Y063-90_minimal tool works only with specific firmware versions of ME10 and G239.  
The correct version needs to be selected accordingly:  

| Firmware version           | Tool version<br>13y063-90_minimal |
|----------------------------|-----------------------------------|
| >= 2.00                    | >= 1.04                           |
| < 2.00                     | <= 1.03                           |

> Starting with firmware version 2.0 the USB PIDs for ME10 and G239 have been changed to 0005 and 0006 respectively.  
> In order to detect those changed USB PIDs, tool version 1.04 or greater is necessary.  
> The tool version and the firmware version of the board are printed when the tool is executed. The tool version can also be found in the source code as `VERSION`.



# Build
## Prerequisites
After cloning the repository populate the submodules with:
```
git submodule update --init
```

Make sure required dependencies are installed:
```
sudo apt-get install pkg-config libusb-1.0-0-dev libudev-dev
```

Kernel must support `usbhid` and `hid-generic` (most probably the case), e.g.
```
CONFIG_USB_HID=m
CONFIG_HID_GENERIC=m
```

## Build 

* Makefile is placed in *linux/*  
* The built executable is placed in *linux/bin/*

With standart API:
```
$ cd linux
$ make
```

or with minimal API:
```
$ cd linux
$ make API=minimal
```

Executing the tool variant:
```
$ sudo ./bin/13Y063-90 -h
```

or 
```
$ sudo ./bin/13Y063-90_minimal -h
```

# Variants

## 13Y063-90

* This variant uses library `hidapi` (available in folder hidapi/).

* Since this tool variant uses the `libusb` backend of `libhidapi` you must run the tool's executable as root user.

    If you want to execute the tool as a normal user i.e. without root user rights:

    1. Create a rules file (e.g. 81-libusb.rules) under */etc/udev/rules*.d/ and add the following line:
        ```
        SUBSYSTEM=="usb", ATTR{idVendor}=="1b02", ATTR{idProduct}=="0004", MODE="0666"
        ```

    2. Save the file and execute:
        ```
        $ sudo udevadm control --reload-rules && udevadm trigger
        ```

    3. If you still cannot execute the tool as normal user reboot the system and try again.

## 13Y063-90_minimal

> Note:  
> Due to limitation of IOCTLs of the hidraw driver it is not possible for > 13Y063-90_minimal tool to read the firmware version of the board. To get the firmware version of the board, either use `13Y063-90` tool (see above), read firmware version, reboot and then switch back to 13Y063-90_minimal tool or contact duagon support.  
> Please also read the chapter "Known Limitations" in this readme.

* The advantage of 13Y063-90_minimal compared to 13Y063-90 is that no `libhidapi`, `libudev` or `libusb` is required.

* This variant uses a library developed by duagon that uses IOCTLs of the `hidraw` Linux kernel driver.  
  The library is called `hidapi_minimal_13y063-90`.  
  Please refer to the README in *linux/hidapi_minimal/* to learn
  more about this library.

# Known Limitations

1. If you are using 13Y063-90 variant and would like to switch to 13Y063-90_minimal variant (or vice versa) then please reboot the system.  
Users are recommended to not use both variants one after the other as they use different interfaces: 13Y063-90 uses `libusb` interface of `hidapi`, whereas 13Y063-90_minimal uses IOCTLs of `hidraw` Linux driver.  
It has been observed that if you execute 13Y063-90_minimal and then execute 13Y063-90, the hidraw interface of the board disappears and 13Y063-90_minimal cannot find that interface. In that case a reboot is required.

2. The 13Y063-90_minimal tool cannot read the firmware version, geographical address and serial number of the board.  
This is because the IOCTLs of hidraw Linux driver do not provide these
functionalities.
