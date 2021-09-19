Device interact 

Command line tool, to interact with various devices.

Often used for Smart home or office solutions, to interact with relays, sensors and other equitment.
made for linux compatible platform, including debian, Raspberry-PI, Ubuntu and the likes.


## Supported devices:

* [[HID USB]]
* [[One-wire]]
* [[SysFS]] generic sysfs 





## Usages

devia [<options>] [<device identifier> [<attribute> [<action]]]

**device identifier**:  Is a device specific concatanated key, used to identify the device which is a key consisting of <interface>#<device identifier>#<port>#<device path>
Each part is separated with a '&'. and can be empty or the end omitted
If the identifier is ambiguous, multiple matching devices are affected.
  **interface**: Is the type of interfaces used for the device. ex. usb, gpio, serial, hidusb.
  **Device identifier**: is specific to the interface type. ex: hidusb#<vendor id>#<product id>#<serial number>#<manufacturer string>
  **port**: Is a string that describe the port/bus, the device is attached to - as the kernel sees it. (sysfs)
  **device path**: Is the path to the device as a kernel file.

**attribute**  is the attribute of the device that should be interacted with. It is device specific. ex. a relay number, address etc.

**action**  Is device specific. it describes what is to be done to it. Ex: on, off, toggle, or other values. It is what is done to the attribute of the device.

**options** : 
  -l --list:  List attached devices
  -i --info:  Print additional information 
  -s --supported_devices: List supported devices
  -m --monitor [<milliseconds>]: monitor or repeat action every <milliseconds> if specified, or when ever suitable. 
  -c --changes: Print only changed states.








[Installation and compilation]

 




