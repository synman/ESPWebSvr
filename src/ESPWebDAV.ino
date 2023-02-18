// Using the WebDAV server with Rigidbot 3D printer.
// Printer controller is a variation of Rambo running Marlin firmware

#include <Arduino.h>
#include <ArduinoOTA.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <user_interface.h>

#include "ESPWebDAV.h"

#define BM_WDT_SOFTWARE 0
#define BM_WDT_HARDWARE 1
#define BM_ESP_RESTART 2
#define BM_ESP_RESET 3

#define BOOT_MODE BM_WDT_SOFTWARE

// LED is connected to GPIO2 on this board
#define INIT_LED			{pinMode(2, OUTPUT);}

#define SD_CS		4
#define MISO		12
#define MOSI		13
#define SCLK		14
#define CS_SENSE	5

#define SERVER_PORT		80
#define SPI_BLOCKOUT_PERIOD	20000UL

uint32 sys_time = system_get_time();
CONFIG_TYPE config;

const char *hostname 	= "espwebdav";
const char *ssid 		= "xxxx";
const char *pwd 		= "xxxx";

ESPWebDAV dav;
String statusMessage;
bool initFailed = false;

volatile unsigned long spiBlockoutTime = 0;
bool weHaveBus = false;

// ------------------------
void setup() {
  	DBG_INIT(115200);
	DBG_PRINTLN("\nESPWebDAV setup");
		
	EEPROM.begin(EEPROM_SIZE);
	uint8_t *p = (uint8_t*)(&config);
	for (uint8 i = 0; i < sizeof(config); i++)
	{
		*(p + i) = EEPROM.read(i);
	}
	EEPROM.commit();

	if (config.hostname_flag == 9) {
		DBG_PRINT("config host="); DBG_PRINTLN(config.hostname);
		hostname = config.hostname;
	} 
	if (config.ssid_flag == 9) {
		DBG_PRINT("config ssid="); DBG_PRINTLN(config.ssid);
		ssid = config.ssid;
	} 
	if (config.pwd_flag == 9) {
		DBG_PRINT("config pwd="); DBG_PRINTLN(config.pwd);
		pwd = config.pwd;
	} 

	INIT_LED;
	blink();
	
	// ----- WIFI -------
	// Set hostname first
	WiFi.hostname(hostname);
	// Reduce startup surge current
	WiFi.setAutoConnect(false);
	WiFi.mode(WIFI_STA);
	WiFi.setPhyMode(WIFI_PHY_MODE_11N);
	WiFi.begin(ssid, pwd);

	// Wait for connection
	while(WiFi.status() != WL_CONNECTED) {
		blink();
		DBG_PRINT(".");
	}

	if (!MDNS.begin(hostname)) { 
		DBG_PRINTLN("\nError setting up mDNS responder!");
	} else {
		DBG_PRINTLN("\nmDNS successfully activated");
		MDNS.update();
	}
  
   	ArduinoOTA.begin();

	DBG_PRINTLN("");
  	DBG_PRINT("    Hostname: "); DBG_PRINTLN(hostname);
	DBG_PRINT("Connected to: "); DBG_PRINTLN(ssid);
	DBG_PRINT("  IP address: "); DBG_PRINTLN(WiFi.localIP());
	DBG_PRINT("        RSSI: "); DBG_PRINTLN(WiFi.RSSI());
	DBG_PRINT("        Mode: "); DBG_PRINTLN(WiFi.getPhyMode());

	// Detect when other master uses SPI bus
	pinMode(CS_SENSE, INPUT);
	attachInterrupt(CS_SENSE, []() {
		if(!weHaveBus)
			spiBlockoutTime = millis() + SPI_BLOCKOUT_PERIOD;
	}, FALLING);

	// wait for other master to assert SPI bus first
	delay(SPI_BLOCKOUT_PERIOD);
  
	// sleep for an additional 30 seconds
	// we don't want to be touching the SD card 
	// if we are connected to a printer and its
	// bootloader is trying to install a firmware bin
	delay(30000);

	// ----- SD Card and Server -------
	// Check to see if other master is using the SPI bus
	while(millis() < spiBlockoutTime)
		blink();
	
	takeBusControl();
	
	// start the SD DAV server
	if(!dav.init(SD_CS, SERVER_PORT)) {
		statusMessage = "Failed to initialize SD Card";
		DBG_PRINT("\nERROR: "); DBG_PRINTLN(statusMessage);
		// indicate error on LED
		errorBlink();
		initFailed = true;
	} else {
		blink();
		DBG_PRINTLN("\nWebDAV server started");
  	}

	relenquishBusControl();
}



// ------------------------
void loop() {
// ------------------------
	MDNS.update();
	ArduinoOTA.handle();

	if(millis() < spiBlockoutTime)
		blink();

	// do it only if there is a need to read FS
	if(dav.isClientWaiting())	{
		if(initFailed)
			return dav.rejectClient(statusMessage);
		
		// has other master been using the bus in last few seconds
		if(millis() < spiBlockoutTime)
			return dav.rejectClient("Host is reading from SD card");
		
		// a client is waiting and FS is ready and other SPI master is not using the bus
		takeBusControl();
		dav.handleClient();
		relenquishBusControl();
	}

	// reboot every 6 hours
	if (sys_time / 1000 / 1000 + 21600 <= system_get_time() / 1000 / 1000)  {
		DBG_PRINT("Requesting scheduled reboot ref="); DBG_PRINT(sys_time / 1000 / 1000 + 21600); DBG_PRINT(" cur="); DBG_PRINTLN(system_get_time() / 1000 / 1000);
		reboot = true;    
	}

	if (reboot || initFailed) {
		initFailed = false;
		reboot = false;
		DBG_PRINTLN("REBOOTING");
		errorBlink();
		#if BOOT_MODE == BM_ESP_RESTART
		Serial.println("\n\nRebooting with ESP.restart()");
		ESP.restart();
		#elif BOOT_MODE == BM_ESP_RESET
		Serial.println("\n\nRebooting with ESP.reset()");
		ESP.reset();
		#else 
		#if BOOT_MODE == BM_WDT_HARDWARE      
			Serial.println("\n\nRebooting with hardware watchdog timeout reset");
			ESP.wdtDisable();
		#else
			Serial.println("\n\nRebooting with sofware watchdog timeout reset");
		#endif  
		while (1) {}  // timeout!
		#endif 
	}
}

// ------------------------
void takeBusControl()	{
// ------------------------
	weHaveBus = true;
	LED_ON;
	pinMode(MISO, SPECIAL);	
	pinMode(MOSI, SPECIAL);	
	pinMode(SCLK, SPECIAL);	
	pinMode(SD_CS, OUTPUT);
}

// ------------------------
void relenquishBusControl()	{
// ------------------------
	pinMode(MISO, INPUT);	
	pinMode(MOSI, INPUT);	
	pinMode(SCLK, INPUT);	
	pinMode(SD_CS, INPUT);
	LED_OFF;
	weHaveBus = false;
}

// ------------------------
void blink()	{
// ------------------------
	LED_ON; 
	delay(100); 
	LED_OFF; 
	delay(400);
}

// ------------------------
void errorBlink()	{
// ------------------------
	for(int i = 0; i < 100; i++)	{
		LED_ON; 
		delay(50); 
		LED_OFF; 
		delay(50);
	}
}