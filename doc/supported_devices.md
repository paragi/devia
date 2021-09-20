# Supported devices

## HID/UDB interface

**Nuvoton relay controler** ![](image/relay-controller-nuvoton.png) ![](image/relay-controller-nuvoton16.png) 

USB HID relay controller board, from Nuvoton - Winbond Electronics Corp. 
This relay USB controller is almost identical to the Sainsmart 16-channel controller, except that the state of the relays are in bit-order from 0 to 16, LSB first.

It's a Chinese product with precious little and useless [documentation](https://www.cafago.com/en/p-e1812-1.html) and support.

Identifying devices:
The Nuvoton device has no serial number. The only way to uniquely identify multiple devices connected, is to use the physical port paths.
The HIDAPI enumeration returns a device node path. As the device node changes, when reconnected, Its unreliable as identification.

The Nuvoton HID relay controler can only be destinquised from the Saintsmart relay controler, by the manufacturer_string.

The device identification looks like this: **hidusb#0416:5020::Nuvoton#** 

Example get relay state:

    > devia hidudb#0416:5020 1
    hidusb#0416:5020::Nuvoton#0002:0005:00 1 on

Example set all relays off

    > devia hidudb#0416:5020 all off 
    hidusb#0416:5020::Nuvoton#0002:0005:00 all 0000000000000000

action can be both 0/1 anf off/on
Attribute can be 1-16, all or 0

