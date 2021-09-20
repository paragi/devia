
# Device interact
[![MIT license](http://img.shields.io/badge/license-MIT-brightgreen.svg)](http://opensource.org/licenses/MIT)
[![Issues](http://img.shields.io/github/issues/paragi/devia.svg)]( https://github.com/Paragi/devia/issues )
[![GitHub pull-requests](https://img.shields.io/github/issues-pr/paragi/devia.svg)](https://GitHub.com/paragi/devia/pull/)
![GitHub all releases](https://img.shields.io/github/downloads/paragi/devia/total)
![GitHub Sponsors](https://img.shields.io/github/sponsors/paragi)


* Command-line tool for easy and uniform access to attached devices.
* Interact with sensors, relays and other smart home devices.
* Easy integration to smart home systems and devices of all kinds.

**Note:** Early version. please wait for a stable version, if you want to include it to your project.


### Usages

    devia [<options>] [<device id> [<device attribute> [<action>]]]

See [documentation](doc/index.md) for a detailed and complete description.

**Example 1** - controle a relay: on hid/usb interface, relay controler defined by vendor id 0416:5020, set relay 1 on.

    > devia hidudb#0416:5020 1 on
    hidusb#0416:5020::Nuvoton#0002:0005:00 1 on

or ommit the action (last parameter) and read 

    > devia hidudb#0416:5020 1 
    hidusb#0416:5020::Nuvoton#0002:0005:00 1 on
  
**Example 2** - list attached devices

    > devia --list
    Nuvoton relay controler  id: hidusb#0416:5020::Nuvoton#0001:003b:00
    Nuvoton relay controler  id: hidusb#0416:5020::Nuvoton#0001:003a:00
    Nuvoton relay controler  id: hidusb#0416:5020::Nuvoton#0001:0039:00
    Nuvoton relay controler  id: hidusb#0416:5020::Nuvoton#0001:001a:00
    One-wire device  id: w1#28-000004d0db67 
    One-wire device  id: w1#28-011581cb99ff 
    One-wire device  id: w1#28-0000057eafe6 
    One-wire device  id: w1#28-0115818eebff 


**Example 3** - Continuosly read chages in temperatur

    > devia --monitor --changes w1#28-0000057eafe6 temperature
    w1#28-0000057eafe6 temperature 24750
    w1#28-0000057eafe6 temperature 24725
    w1#28-0000057eafe6 temperature 24700
    w1#28-0000057eafe6 temperature 24675
    ...

Devia also has a --info option, to help identify devices and problems, that provides a lot of extra information.

# Applications

* [IKEA buttons controling relays, with node-red on reapberry-pi](doc/ikea.md)
* [Zigbee setup](doc/zigbee-dongle.md)
* [Controlling relays from Python, PHP, JS node and other languages](doc/scripts)

# Supported devices

The following interfaceses are supported. 
Devices are listed under each interface.

* [HID USB](doc/hidusb.md)
* [one-wire](doc/w1)
* [SysFs](doc/sysfs.md)
* [GPIO](doc/gpio.md) (empty)
* [com](doc/com.md) (empty)


If you would like devia to have a particular device included, you can ask me to do so. 
Depending on complexity and gennerel interest, I will add devices on request, when i have the time available.
Your request should include that you send me a device for testing (I keep it) and as much information as posibble.

You may also contribute in other ways:
- write an interface for a device, keeping the coding style and using short clean code.
- repport bugs, with a reproduceable examples and descriptions
- correct any error you incounter and make a pull request.

Contributions are appriciated.


# Install

Currently there is no package. You must compile the program.

### Compile

  #### Install packages

    sudo apt install git libusb-1.0-dev libexplain-dev libhidapi-dev libftdi-dev libglib2.0-dev libsysfs-dev

  #### Retrieve source code

    git clone https://github/paragi/devia

  #### Compile and install

    cd devia
    make
    make install

