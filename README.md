# WebDAV Server and a 3D Printer
This project was originally based on the work here:  https://github.com/ardyesp/ESPWebDAV

~~WiFi WebDAV server using ESP8266 SoC. It maintains the filesystem on an SD card.~~

So much of what is mentioned below is no longer valid in this distribution.   It is safe to call it a self standing project,
not bound by any prior upstreams / code, as it has reached a point of divergence where I feel comfortable declaring it unique.

**This document is grossly out of date at this point**

Supports the basic WebDav operations - *PROPFIND*, *GET*, *PUT*, *DELETE*, *MKCOL*, *MOVE* etc.

Once the WebDAV server is running on the ESP8266, a WebDAV client like Windows can access the filesystem on the SD card just like a cloud drive. The drive can also be mounted like a networked drive, and allows copying/pasting/deleting files on SD card remotely.

### 3D Printer
I am using this setup as a networked drive for 3D Printer running Marlin. Following circuit with ESP8266 and a MicroSD adapter is fabricated on a PCB. A full size SD card adapter is glued to one end and provides access to all SPI data lines from printer. ESP8266 code avoids accessing micro SD card, when Marlin (printer's firmware) is reading/writing to it (detected using Chip Select line).

GCode can be directly uploaded from the slicer (Cura) to this remote drive, thereby simplifying the workflow. 

![Printer Hookup Diagram](./PrinterHookup2.jpg)

## Dependencies:
1. [ESP8266 Arduino Core version 2.4](https://github.com/esp8266/Arduino)
2. [SdFat library](https://github.com/greiman/SdFat)

## Build And Deploy:
Connect the SPI bus lines to SD card.

ESP Module|SD Card
---|---
GPIO13|MOSI   
GPIO12|MISO   
GPIO14|SCK    
GPIO4|CS   
GPIO5|CS Sense   

The card should be formatted for Fat16 or Fat32

Compile and upload the program to an ESP8266 module. 
```
Detecting chip type... ESP8266
Chip is ESP8285H16
Features: WiFi, Embedded Flash
Crystal is 26MHz
MAC: xxxx
Uploading stub...
Running stub...
Stub running...
Manufacturer: a1
Device: 4015
Detected flash size: 2MB
```

This project is fully compatible with PlatformIO. Additionally, OTA is fully supported for subsequent flashes of the sketch over Wi-Fi.

Be sure to reset your adapter using its reset button after uploading a new firmware over USB.  The ESP8266 series modules will not properly reset / reboot
after a USB flash if not reset via their reset button first.  This limitation does not apply to OTA updates.

There is plenty of extra flash storage on this module and work is underway to split the rom with a 1mb spiifs partition (too many conflicts with sdFat at the moment but the spiffs FS is there if you `pio run -t uploadFs`)

## Initial Setup
After uploading the sketch to your WiFi-SD board, reset the adapter and wait approximately 60 seconds.  You can monitor its progress via the 
PlatformIO Serial Monitor.  Once it indicates the WebDav server is running, connect to its Wi-Fi AP (espwebdav1).

Open up a browser and visit http://192.168.4.1

<img width="201" alt="Screenshot 2023-02-19 at 11 56 41 AM" src="https://user-images.githubusercontent.com/1299716/219963213-3390b434-43d0-4752-9688-e7708b868d0a.png">

Fill in the hostname, ssid, and password fields with values that meet your needs and click the `Save` button.  Press the back button in your 
browser, refresh the page, and verify the values you entered meet your requirements.  Click the `Reboot` button to activate the changes.

After aproximiately 30 seconds, the WebDav server will be active and you can access it as described below, using the hostname you provided 
if your DHCP / DNS server support it.  Otherwise, the adapter will be available via its mDNS name `{hostname}`.`local`

You can revist the settings page at any time to reboot the adapter or update its settings.

## Use:
To access the drive from Windows, type ```\\esp_hostname_or_ip\DavWWWRoot``` at the Run prompt, or use Map Network Drive menu in Windows Explorer.

`cadaver` is an excellent command line tool for accessing webdav devices if you are having problems using Windows/MacOS built-in options.

## References
Marlin Firmware - [http://marlinfw.org/](http://marlinfw.org/)   

Cura Slicer - [https://ultimaker.com/en/products/ultimaker-cura-software](https://ultimaker.com/en/products/ultimaker-cura-software)   

3D Printer LCD and SD Card Interface - [http://reprap.org/wiki/RepRapDiscount_Full_Graphic_Smart_Controller](http://reprap.org/wiki/RepRapDiscount_Full_Graphic_Smart_Controller)   

LCD Schematics - [http://reprap.org/mediawiki/images/7/79/LCD_connect_SCHDOC.pdf](http://reprap.org/mediawiki/images/7/79/LCD_connect_SCHDOC.pdf)   



