// Full blown HTTP server implementation for Wifi-SD card

#include <Arduino.h>
#include <ArduinoOTA.h>
#include "LittleFS.h"

#include <DNSServer.h> 
#include <EEPROM.h>

#include "WebServer.h"

// LED is connected to GPIO2 on this board
#define INIT_LED			{pinMode(2, OUTPUT);}

uint32 sys_time = system_get_time();
CONFIG_TYPE config;

const char *hostname 	= "espwebsvr";
const char *ssid 		= "";
const char *pwd 		= "";
WiFiMode_t wifimode     = WIFI_AP;

DNSServer dnsServer;
const byte DNS_PORT = 53;

WebServer webserver;
char buf[255];

TelnetSpy SerialAndTelnet;

void waitForConnection() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    SER.print(".");
  }
  SER.println(F(" Connected!"));
}

void waitForDisconnection() {
  while (WiFi.status() == WL_CONNECTED) {
    delay(500);
    SER.print(".");
  }
  SER.println(F(" Disconnected!"));
}

// ------------------------
void setup() {
  	// Serial.begin(115200);
	SerialAndTelnet.setWelcomeMsg(F("espwebsvr diagnostics\r\n\n"));
	SER.begin(115200);
	delay(100); // Wait for serial port

	// disable sleep
	wifi_fpm_set_sleep_type(NONE_SLEEP_T);

	SER.println("\nESPWebSvr setup\n");
	
	EEPROM.begin(EEPROM_SIZE);
	uint8_t *p = (uint8_t*)(&config);
	for (uint8 i = 0; i < sizeof(config); i++)
	{
		*(p + i) = EEPROM.read(i);
	}
	EEPROM.commit();
	EEPROM.end();

	if (config.hostname_flag == 9) {
		sprintf(buf, "config host: %s", config.hostname);
		SER.println(buf);
		hostname = config.hostname;
	} else {
		strcpy(config.hostname, hostname);
	}

	if (config.ssid_flag == 9) {
		sprintf(buf, "config ssid: %s", config.ssid);
		SER.println(buf);
		ssid = config.ssid;
		wifimode = WIFI_STA;
	} 
	if (config.pwd_flag == 9) {
		sprintf(buf, "config  pwd: %s", config.pwd);
		SER.println(buf);
		pwd = config.pwd;
	} 

	INIT_LED;
	blink();
	
	WiFi.hostname(hostname);
	WiFi.mode(wifimode);
	WiFi.setAutoConnect(false);
	WiFi.setPhyMode(WIFI_PHY_MODE_11N);

	if (wifimode != WIFI_AP) {
		WiFi.begin(ssid, pwd);
		// Wait for connection
		SER.print("\nConnecting to WiFi .");
		for (uint8 x = 0 ; x < 60 && WiFi.status() != WL_CONNECTED; x++) {
			blink();
			SER.print(".");
		}
	}
	
	if (WiFi.status() != WL_CONNECTED) {
		wifimode = WIFI_AP;
		WiFi.mode(wifimode);
		WiFi.softAP("espwebsvr");
		dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
		SER.println("\n\nSoftAP [espwebsvr] started");
	}

	SER.println("\n");
  	SER.print("    Hostname: "); SER.println(hostname);
	SER.print("Connected to: "); SER.println(ssid);
	SER.print("  IP address: "); SER.println(WiFi.localIP().toString());
	SER.print("        RSSI: "); SER.println(WiFi.RSSI());
	SER.print("        Mode: "); SER.println(WiFi.getPhyMode());
	
	if(!LittleFS.begin()){
		SER.println("An Error has occurred while mounting LittleFS");
	} else {
		updateIndexTemplate(hostname, ssid, pwd);
	}

	// start the web server
	webserver.init();
	blink();
	SER.println("\nWeb server started");

	// enable mDNS via espota
	ArduinoOTA.setHostname(hostname);
   	ArduinoOTA.begin(true);
}

// ------------------------
void loop() {
// ------------------------
	// reboot every 6 hours
	if (sys_time / 1000 / 1000 + 21600 <= system_get_time() / 1000 / 1000)  {
		SER.print("Requesting scheduled reboot ref="); SER.print(sys_time / 1000 / 1000 + 21600); SER.print(" cur="); SER.println(system_get_time() / 1000 / 1000);
		reboot = true;    
	}

	if (reboot) {
		reboot = false;
		SER.println("REBOOTING");
		errorBlink();
		ESP.restart();
		while (1) {}  // timeout!
	}

	SerialAndTelnet.handle();

	if (SER.available() > 0) {
		char c = SER.read();
		switch (c) {
		case '\r':
			SER.println();
			break;
		case '\n':
			break;
		case 'C':
			SER.print(F("\r\nConnecting "));
			WiFi.begin(ssid, pwd);
			waitForConnection();
			break;
		case 'D':
			SER.print(F("\r\nDisconnecting ..."));
			WiFi.disconnect();
			waitForDisconnection();
			break;
		case 'R':
			SER.print(F("\r\nReconnecting "));
			WiFi.reconnect();
			waitForDisconnection();
			waitForConnection();
			break;
		case 'X':
			SER.println(F("\r\nClosing telnet session..."));
			SerialAndTelnet.disconnectClient();
			break;
		case 'B':
			SER.println(F("\r\nsubmitting reboot request..."));
			reboot = true;
			break;
		default:
			SER.print("\n\nCommands:\n\nC = WiFi Connect\nD = WiFi Disconnect\nR = WiFi Reconnect\nX = Close Session\nB = Reboot ESP\n\n");
			break;
		}
		SER.flush();
	}

	if (wifimode == WIFI_AP) {
		dnsServer.processNextRequest();
	}

	// check for OTA
	ArduinoOTA.handle();

	// if a request is pending, process it
	webserver.handleClient();
}

void updateIndexTemplate(const char* hostname, const char* ssid, const char* pwd) {
	// create new index based on active config
	File _template = LittleFS.open("/index.template.html", "r");

	if (_template) {
		String html = _template.readString();
		_template.close();

		while (html.indexOf("{host}", 0) != -1) {
			html.replace("{host}", hostname);
		}

		html.replace("{ssid}", ssid);
		html.replace("{pass}", pwd);

		if (LittleFS.exists("/index.html")) LittleFS.remove("/index.html");
		File _index = LittleFS.open("/index.html", "w");
		_index.write(html.c_str());
		_index.close();
	}
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