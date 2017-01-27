# USB-controlled switched-mode power supply

### Characteristics
* forward topology
* voltage range 0-36 V
* adjustable current limit 0-2 A
* alphanumeric LCD display shows set and measured values
* possibility to disconnect the output from the powered device using the internal relay using the button or via USB
* possibility to set voltage and current values per a value pair or using a CSV file with consecutive values via USB
* possibility to read measured values via USB
* the device is an early prototype but is no longer being developed

### Hardware
Hardware has been designed and made by KBS.

Notable parts:

* ATmega8 - microcontroller
* TL494 - PWM control circuit, the chip is commonly used in SMPS's
* MCP4802 / MCP4812 / MCP4822 - 8/10/12-bit DACs with internal voltage reference
* a cool rotary encoder instead of a cheesy potentiometer

### Software

##### Device
Toolchain used - Atmel Studio with the integrated toolset.

The device registers itself as a HID device with VID=0x16c0 and PID=0x05df.

Third-party libraries:

* [V-USB](https://www.obdev.at/products/vusb/index.html) - software USB 1.1 stack

##### Host
Tiny CLI program to test basic functionality of the SMPS. It should work on both Windows and Linux.

Toolchain used - Eclipse + gcc.

Third-party libraries:

* [HID API](http://www.signal11.us/oss/hidapi/) - multi-platform library for USB-HID communication

### Known issues
Controlling the SMPS via USB is impossible for higher output voltages because of lack of error detection mechanism in the software USB stack used in the project.

### Other information
Date of project completion - October 2014
