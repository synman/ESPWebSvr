#include <ESP8266WiFi.h>
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

// constants for WebServer
#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)
#define HTTP_MAX_POST_WAIT 		5000 

extern bool reboot;
extern CONFIG_TYPE config;
extern TelnetSpy SerialAndTelnet;

#ifndef FUNCTIONS_H_INCLUDED
  #define FUNCTIONS_H_INCLUDED
  void blink();
  void errorBlink();
  boolean takeBusControl();
  void relenquishBusControl();
#endif

class ESPWebDAV	{
public:
	bool init(int serverPort);
	bool isClientWaiting();
	void handleClient(String blank = "");
	
protected:
	typedef void (ESPWebDAV::*THandlerFunction)(String);
	
	void processClient(THandlerFunction handler, String message);
	void handleRequest(String blank);

	// Sections are copied from ESP8266Webserver
	String getMimeType(String path);
	String urlDecode(const String& text);
	String urlToUri(String url);
	bool parseRequest();
	void sendHeader(const String& name, const String& value, bool first = false);
	void send(String code, const char* content_type, const String& content);
	void _prepareHeader(String& response, String code, const char* content_type, size_t contentLength);
	void sendContent(const String& content);
	void sendContent_P(PGM_P content);
	void setContentLength(size_t len);
	size_t readBytesWithTimeout(uint8_t *buf, size_t bufSize);
	size_t readBytesWithTimeout(uint8_t *buf, size_t bufSize, size_t numToRead);

	// variables pertaining to current most HTTP request being serviced
	WiFiServer *server;
	WiFiClient 	client;
	String 		method;
	String 		uri;
	String 		contentLengthHeader;
	String 		depthHeader;
	String 		hostHeader;
	String		destinationHeader;

	String 		_responseHeaders;
	bool		_chunked;
	int			_contentLength;
};






