#include <ESP8266WebServer.h>
#include <TelnetSpy.h>

#define LED_ON				{digitalWrite(2, LOW);}
#define LED_OFF				{digitalWrite(2, HIGH);}

#define HOSTNAME_LEN 32
#define WIFI_SSID_LEN 32
#define WIFI_PASSWD_LEN 64

//#define SER  Serial
#define SER  SerialAndTelnet

#define EEPROM_SIZE 512

typedef struct config_type {
  unsigned char hostname_flag;
  char hostname[HOSTNAME_LEN];
  unsigned char ssid_flag;
  char ssid[WIFI_SSID_LEN];
  unsigned char pwd_flag;
  char pwd[WIFI_PASSWD_LEN];
} CONFIG_TYPE;

enum FileSystem { LFS, SDCARD };

// constants for WebServer
#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)
#define HTTP_MAX_POST_WAIT 		5000 

extern bool reboot;
extern CONFIG_TYPE config;
extern TelnetSpy SerialAndTelnet;

#ifndef FUNCTIONS_H_INCLUDED
  #define FUNCTIONS_H_INCLUDED
  void updateIndexTemplate(const char* hostname, const char* ssid, const char* pwd);
  void blink();
  void errorBlink();
  boolean takeBusControl();
  void relenquishBusControl();
#endif

class WebServer	{
public:
	void init();
	void handleClient(String blank = "");
	
protected:
	void handleRequest(String blank);
	void handleFileUpload(FileSystem fs);

	// Sections are copied from ESP8266Webserver
	String getMimeType(String path);
	String urlDecode(const String& text);
	String urlToUri(String url);

	// variables pertaining to current most HTTP request being serviced
	WiFiClient 	client;   // we should avoid using this
	HTTPMethod 	method;
	String 		uri;
};