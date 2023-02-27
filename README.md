# ESP Web Server / Network Shared SD Card
This project was originally based on the work here:  https://github.com/ardyesp/ESPWebDAV

## Features
- HTTP based Network Configuration (SoftAP failover)
- Captive Portal in SoftAP mode via DNS
- Onboard LittleFS partition support
- SD FAT16/FAT32 partition support
- HTTP file management (upload / download / delete)
 
### 3D Printer
I am using this setup as a networked drive for 3D Printer running Marlin. Following circuit with ESP8266 and a MicroSD adapter is fabricated on a PCB. A full size SD card adapter is glued to one end and provides access to all SPI data lines from printer. ESP8266 code avoids accessing micro SD card, when Marlin (printer's firmware) is reading/writing to it (detected using Chip Select line).

GCode can be directly uploaded from the slicer (Cura) to this remote drive, thereby simplifying the workflow. 

#### DIY
![Printer Hookup Diagram](./PrinterHookup2.jpg)

#### Prebuilt

![s-l1600](https://user-images.githubusercontent.com/1299716/221260555-80218804-13b5-4fb6-b807-f3526bfa82da.jpg)


## Dependencies:
1. Visual Studio Code
2. PlatformIO 
3. ESP8266 Arduino Framework

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

There is extra flash storage on this module and the project's `platformio.ini` file has been configured to flash the atached ESP8266 with a 2m1m partition scheme, essentially providing a 1mb LittleFS partition that can be used to host and serve static content.

## Initial Setup
After uploading the sketch to your WiFi-SD board, reset the adapter and wait approximately 60 seconds.  You can monitor its progress via the 
PlatformIO Serial Monitor.  Once it indicates the WebDav server is running, connect to its Wi-Fi AP (espwebdav1).

Open up a browser and visit http://192.168.4.1 or really any address.  The captive portal should direct you to the Web Admin form.

<img width="201" alt="Screenshot 2023-02-19 at 11 56 41 AM" src="https://user-images.githubusercontent.com/1299716/219963213-3390b434-43d0-4752-9688-e7708b868d0a.png">

Fill in the hostname, ssid, and password fields with values that meet your needs and click the `Save` button. Verify the values you entered meet your requirements.  Click the `Reboot` button to activate the changes.

After aproximiately 30 seconds, the Web Server server will be active on the configured wireless network and you can access it as described below, using the hostname you provided if your DHCP / DNS server support it.  Otherwise, the adapter will be available via its mDNS name `{hostname}`.`local`

You can revist the settings page at any time to reboot the adapter or update its settings.

## Use:

Upload files and create directories via the `File Upload Form`.

<img width="361" alt="Screenshot 2023-02-24 at 12 37 00 PM" src="https://user-images.githubusercontent.com/1299716/221257223-fb68ee1a-c307-4513-8b82-d0c8cf95555a.png">

View, download, and delete files via the respective File List pages.

<img width="1121" alt="Screenshot 2023-02-24 at 1 01 10 PM" src="https://user-images.githubusercontent.com/1299716/221257532-4af895fb-1559-4217-8b6e-d3dee7459851.png">

<img width="1064" alt="Screenshot 2023-02-24 at 1 02 15 PM" src="https://user-images.githubusercontent.com/1299716/221257555-43f11a62-c668-4344-9719-69d900e3f08f.png">

## References
Marlin Firmware - [http://marlinfw.org/](http://marlinfw.org/)   

Cura Slicer - [https://ultimaker.com/en/products/ultimaker-cura-software](https://ultimaker.com/en/products/ultimaker-cura-software)   

3D Printer LCD and SD Card Interface - [http://reprap.org/wiki/RepRapDiscount_Full_Graphic_Smart_Controller](http://reprap.org/wiki/RepRapDiscount_Full_Graphic_Smart_Controller)   

LCD Schematics - [http://reprap.org/mediawiki/images/7/79/LCD_connect_SCHDOC.pdf](http://reprap.org/mediawiki/images/7/79/LCD_connect_SCHDOC.pdf) 

Cheap Prebuilt ESP8266 WiFi SD Cards - https://www.aliexpress.us/item/2255800805111104.html and https://www.ebay.com/itm/203949035388



