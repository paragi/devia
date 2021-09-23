# Supported devices

## HID/UDB interface
|  |  |
| ------------------------------------------------------------------------------------- | --------- |
| **Nuvoton relay controller** <br> USB HID relay controller board, from Nuvoton - Winbond Electronics Corp. <br> This relay USB controller is almost identical to the Sainsmart 16-channel controller, except that the state of the relays are in bit-order from 0 to 16, LSB first. <br> It's a Chinese product with precious little and useless [documentation](https://www.cafago.com/en/p-e1812-1.html) and support. <br><br> Identifying devices:<br>The Nuvoton device has no serial number. The only way to uniquely identify multiple devices connected, is to use the physical port paths.<br>The HIDAPI enumeration returns a device node path. As the device node changes, when reconnected, Its unreliable as identification.<br><br>The Nuvoton HID relay controller can only be distinguished from the Saintsmart relay controller, by the manufacturer_string. <br><br>The device identification looks like this: **hidusb#0416:5020::Nuvoton#**<br><br> Example get relay state:<br>    \> devia hidudb#0416:5020 1<br> hidusb#0416:5020::Nuvoton#0002:0005:00 1 on<br><br>Example set all relays off<br><br> \> devia hidudb#0416:5020 all off:<br> hidusb#0416:5020::Nuvoton#0002:0005:00 all 0000000000000000<br><br> action can be both 0/1 and off/on <br> Attribute can be 1-16, all or 0 | ![](image/relay-controller-nuvoton.png) ![](image/relay-controller-nuvoton16.png) |


## One-wire interface
|  |  |
| ------------------------------------------------------------------------------------- | --------- |
| **DS18B20 Temperature sensor**<br> The DS18B20 Temperature Sensor, is a widely used chip. Its easy to use, and has a unique 64-bit ID, that makes it possible to have more than 200 devices mounted on the same two or tree wire bus ( signal, gnd and optional vcc)<br> Temperature range: -55°C to +125°C ±0.5 with a programmable Resolution from 9 Bits to 12 Bits<br> <br> The device identification looks like this: **w1#28-011581cb99ff** <br> <br> Example get temperature:<br> \> devia w1#28-011581cb99ff temperature<br> w1#28-011581cb99ff temperature 24725<br><br> Example set resolution to 9 bits:<br>\> devia w1#28-011581cb99ff resolution 9 <br>w1#28-011581cb99ff resolution 9 | ![](image/ds18s20.png) ![](image/ds18b20-waterproof.png) 
