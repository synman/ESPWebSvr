#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <TelnetSpy.h>
#include "LittleFS.h"

#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>
#include "ESPWebDAV.h"

#define SD_CS		4
#define MISO		12
#define MOSI		13
#define SCLK		14
#define CS_SENSE	5
#define SPI_BLOCKOUT_PERIOD	20000UL

bool reboot = false;
char buffer[255];

bool weHaveBus = false;
volatile unsigned long spiBlockoutTime = 0;

void streamDirectory(WiFiClient client, File dir, int numTabs);
String printDirectory(File dir, int numTabs);
void IRAM_ATTR ISRoutine ();

// ------------------------
bool ESPWebDAV::init(int serverPort) {
// ------------------------
	// start the wifi server
	server = new WiFiServer(serverPort);
	server->begin();

	pinMode(CS_SENSE, INPUT);
	attachInterrupt(CS_SENSE, ISRoutine, FALLING);

	return true;
}

void ISRoutine() {
	if (!weHaveBus) {
	// 	unsigned long ms = millis();
	// 	spiBlockoutTime = ms + SPI_BLOCKOUT_PERIOD;

	// 	sprintf(buffer, "%lu / %lu", ms, spiBlockoutTime);
	// 	SER.println(buffer);
		// SER.println("CS_SENSE interupt triggered");
	}
}

// Sections are copied from ESP8266Webserver

// ------------------------
String ESPWebDAV::getMimeType(String path) {
// ------------------------
	if(path.endsWith(".html")) return "text/html";
	else if(path.endsWith(".htm")) return "text/html";
	else if(path.endsWith(".css")) return "text/css";
	else if(path.endsWith(".txt")) return "text/plain";
	else if(path.endsWith(".js")) return "application/javascript";
	else if(path.endsWith(".json")) return "application/json";
	else if(path.endsWith(".png")) return "image/png";
	else if(path.endsWith(".gif")) return "image/gif";
	else if(path.endsWith(".jpg")) return "image/jpeg";
	else if(path.endsWith(".ico")) return "image/x-icon";
	else if(path.endsWith(".svg")) return "image/svg+xml";
	else if(path.endsWith(".ttf")) return "application/x-font-ttf";
	else if(path.endsWith(".otf")) return "application/x-font-opentype";
	else if(path.endsWith(".woff")) return "application/font-woff";
	else if(path.endsWith(".woff2")) return "application/font-woff2";
	else if(path.endsWith(".eot")) return "application/vnd.ms-fontobject";
	else if(path.endsWith(".sfnt")) return "application/font-sfnt";
	else if(path.endsWith(".xml")) return "text/xml";
	else if(path.endsWith(".pdf")) return "application/pdf";
	else if(path.endsWith(".zip")) return "application/zip";
	else if(path.endsWith(".gz")) return "application/x-gzip";
	else if(path.endsWith(".appcache")) return "text/cache-manifest";

	return "application/octet-stream";
}

// ------------------------
String ESPWebDAV::urlDecode(const String& text)	{
// ------------------------
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len)	{
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len))	{
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);
			decodedChar = strtol(temp, NULL, 16);
		}
		else {
			if (encodedChar == '+')
				decodedChar = ' ';
			else
				decodedChar = encodedChar;  // normal ascii char
		}
		decoded += decodedChar;
	}
	return decoded;
}

// ------------------------
String ESPWebDAV::urlToUri(String url)	{
// ------------------------
	if(url.startsWith("http://"))	{
		int uriStart = url.indexOf('/', 7);
		return url.substring(uriStart);
	}
	else
		return url;
}

// ------------------------
bool ESPWebDAV::isClientWaiting() {
// ------------------------
	yield();
	return server->hasClient() && server->hasClientData();
}

// ------------------------
void ESPWebDAV::handleClient(String blank) {
// ------------------------
	processClient(&ESPWebDAV::handleRequest, blank);
}

void ESPWebDAV::handleRequest(String blank)	{
	SER.print("handleRequest for: "); SER.println(uri);

	// default document
	if (method.equals("GET") && uri.equals("/")) {
		String index = "/index.html";
		File file = LittleFS.open(index, "r");

		String header;
		_prepareHeader(header, "200 OK", getMimeType(index).c_str(), file.size());
		client.write(header.c_str(), header.length());

		while(file.available()){
			client.write(file.read());
		}
		file.close();
		return;
	}

	// reboot 
	if (method.equals("GET") && uri.equalsIgnoreCase("/reboot")) {
		send("200 OK", "text/plain", "processing reboot request. . .");
		reboot = true;
		return;
	}

	// save all 
	if (method.equals("GET") && uri.startsWith("/save/")) {
		String pwd = uri.substring(uri.lastIndexOf("/") + 1);
		memset(config.pwd, '\0', WIFI_PASSWD_LEN);
		pwd.toCharArray(config.pwd, WIFI_PASSWD_LEN);

		String uri1 = uri.substring(0, uri.lastIndexOf("/"));
		String ssid = uri1.substring(uri1.lastIndexOf("/") + 1);
		memset(config.ssid, '\0', WIFI_SSID_LEN);
		ssid.toCharArray(config.ssid, WIFI_SSID_LEN);

		String uri2 = uri1.substring(0, uri1.lastIndexOf("/"));
		String host = uri2.substring(uri2.lastIndexOf("/") + 1);
		memset(config.hostname, '\0', HOSTNAME_LEN);
		host.toCharArray(config.hostname, HOSTNAME_LEN);

		config.hostname_flag = 9;
		config.ssid_flag = 9;
		config.pwd_flag = 9;

		EEPROM.begin(EEPROM_SIZE);
		uint8_t *p = (uint8_t*)(&config);
		for (uint8 i = 0; i < sizeof(config); i++) {
			EEPROM.write(i, *(p + i));
		}
		EEPROM.commit();    

		send("200 OK", "text/plain", "all settings saved");
		return;
	}

	// save hostname 
	if (method.equals("GET") && uri.startsWith("/hostname/")) {
		String hostname = uri.substring(uri.lastIndexOf("/") + 1);
		memset(config.hostname, '\0', HOSTNAME_LEN);
		hostname.toCharArray(config.hostname, HOSTNAME_LEN);
		config.hostname_flag = 9;

		EEPROM.begin(EEPROM_SIZE);
		uint8_t *p = (uint8_t*)(&config);
		for (uint8 i = 0; i < sizeof(config); i++) {
		EEPROM.write(i, *(p + i));
		}
		EEPROM.commit();    

		send("200 OK", "text/plain", "hostname saved");
		return;
	}

	// save ssid 
	if (method.equals("GET") && uri.startsWith("/ssid/")) {
		String ssid = uri.substring(uri.lastIndexOf("/") + 1);
		memset(config.ssid, '\0', WIFI_SSID_LEN);
		ssid.toCharArray(config.ssid, WIFI_SSID_LEN);
		config.ssid_flag = 9;

		EEPROM.begin(EEPROM_SIZE);
		uint8_t *p = (uint8_t*)(&config);
		for (uint8 i = 0; i < sizeof(config); i++) {
		EEPROM.write(i, *(p + i));
		}
		EEPROM.commit();    

		send("200 OK", "text/plain", "ssid saved");
		return;
	}

	// save pwd 
	if (method.equals("GET") && uri.startsWith("/password/")) {
		String pwd = uri.substring(uri.lastIndexOf("/") + 1);
		memset(config.pwd, '\0', WIFI_PASSWD_LEN);
		config.pwd_flag = 9;
		pwd.toCharArray(config.pwd, WIFI_PASSWD_LEN);

		EEPROM.begin(EEPROM_SIZE);
		uint8_t *p = (uint8_t*)(&config);
		for (uint8 i = 0; i < sizeof(config); i++) {
		EEPROM.write(i, *(p + i));
		}
		EEPROM.commit();    

		send("200 OK", "text/plain", "network password saved");
		return;
	}

	if (method.equals("GET") && (uri.equals("/fs") || uri.equals("/fs/"))) {
		String str = printDirectory(LittleFS.open("/", "r"), 0);
		String header;
		_prepareHeader(header, "200 OK", "text/plain", str.length());
		client.write(header.c_str(), header.length());
		client.write(str.c_str());
		char buf[255];
		sprintf(buf, "%s %d bytes", "littlefs dir", str.length());
		SER.println(buf);
		return;
	}

	if (method.equals("GET") && (uri.equals("/sd") || uri.equals("/sd/"))) {
		if (takeBusControl()) {
			String str = printDirectory(SD.open("/", "r"), 0);
			relenquishBusControl();
			String header;
			_prepareHeader(header, "200 OK", "text/plain", str.length());
			client.write(header.c_str(), header.length());
			client.write(str.c_str());
			char buf[255];
			sprintf(buf, "%s %d bytes", "sd dir", str.length());
			SER.println(buf);
		} else {
			send("404 Not Found", "text/plain", "SD CARD NOT READY");
			SER.println("SD CARD NOT READY");
		}
		return;
	}

	if (uri.startsWith("/sd") || uri.startsWith("/fs")) {
		String path = uri;
		if (uri.startsWith("/sd")) {
			path.replace("/sd/", "/");
			if (takeBusControl()) {
				File file = SD.open(path, FILE_READ);
				if (file) {
					char buf[255];
					sprintf(buf, "%s %d bytes", file.name(), file.size());
					SER.println(buf);

					String header;
					_prepareHeader(header, "200 OK", getMimeType(path).c_str(), file.size());
					client.write(header.c_str(), header.length());

					while(file.available()){
						client.write(file.read());
						delay(0);
					}
					file.close();
					relenquishBusControl();
					return;
				}
				relenquishBusControl();
			} else {
				send("409 Conflict", "text/plain", "SD CARD NOT READY");
				return;
			}
		} else {
			path.replace("/fs/", "/");
			File file = LittleFS.open(path, "r");
			if (file) {
				char buf[255];
				sprintf(buf, "%s %d bytes", file.name(), file.size());
				SER.println(buf);

				String header;
				_prepareHeader(header, "200 OK", getMimeType(path).c_str(), file.size());
				client.write(header.c_str(), header.length());

				while(file.available()){
					client.write(file.read());
				}
				file.close();
				return;
			}
		}
	}

	String message = "Not found\n";
	message += "URI: ";
	message += uri;
	message += " Method: ";
	message += method;
	message += "\n";

	send("404 Not Found", "text/plain", message);
}

// ------------------------
void ESPWebDAV::processClient(THandlerFunction handler, String message) {
// ------------------------
	// Check if a client has connected
	client = server->accept();
	if(!client)
		return;

	// Wait until the client sends some data
	while(!client.available())
		delay(1);
	
	// reset all variables
	_chunked = false;
	_responseHeaders = String();
	_contentLength = CONTENT_LENGTH_NOT_SET;
	method = String();
	uri = String();
	contentLengthHeader = String();
	depthHeader = String();
	hostHeader = String();
	destinationHeader = String();

	// extract uri, headers etc
	if(parseRequest())
		// invoke the handler
		(this->*handler)(message);
		
	// finalize the response
	if(_chunked)
		sendContent("");

	// send all data before closing connection
	client.flush();
	// // close the connection
	// client.stop();
}

// ------------------------
bool ESPWebDAV::parseRequest() {
// ------------------------
	// Read the first line of HTTP request
	String req = client.readStringUntil('\r');
	client.readStringUntil('\n');
	
	// First line of HTTP request looks like "GET /path HTTP/1.1"
	// Retrieve the "/path" part by finding the spaces
	int addr_start = req.indexOf(' ');
	int addr_end = req.indexOf(' ', addr_start + 1);
	if (addr_start == -1 || addr_end == -1) {
		return false;
	}

	method = req.substring(0, addr_start);
	uri = urlDecode(req.substring(addr_start + 1, addr_end));
	// SER.print("method: "); SER.print(method); SER.print(" url: "); SER.println(uri);
	
	// parse and finish all headers
	String headerName;
	String headerValue;
	
	while(1) {
		req = client.readStringUntil('\r');
		client.readStringUntil('\n');
		if(req == "") 
			// no more headers
			break;
			
		int headerDiv = req.indexOf(':');
		if (headerDiv == -1)
			break;
		
		headerName = req.substring(0, headerDiv);
		headerValue = req.substring(headerDiv + 2);
		// SER.print("\t"); SER.print(headerName); SER.print(": "); SER.println(headerValue);
		
		if(headerName.equalsIgnoreCase("Host"))
			hostHeader = headerValue;
		else if(headerName.equalsIgnoreCase("Depth"))
			depthHeader = headerValue;
		else if(headerName.equalsIgnoreCase("Content-Length"))
			contentLengthHeader = headerValue;
		else if(headerName.equalsIgnoreCase("Destination"))
			destinationHeader = headerValue;
	}
	
	return true;
}

// ------------------------
void ESPWebDAV::sendHeader(const String& name, const String& value, bool first) {
// ------------------------
	String headerLine = name + ": " + value + "\r\n";

	if (first)
		_responseHeaders = headerLine + _responseHeaders;
	else
		_responseHeaders += headerLine;
}

// ------------------------
void ESPWebDAV::send(String code, const char* content_type, const String& content) {
// ------------------------
	char buf[255];
	sprintf(buf, "send %s", code.c_str());
	SER.println(buf);

	String header;
	_prepareHeader(header, code, content_type, content.length());

	client.write(header.c_str(), header.length());
	if(content.length())
		sendContent(content);
}

// ------------------------
void ESPWebDAV::_prepareHeader(String& response, String code, const char* content_type, size_t contentLength) {
// ------------------------
	response = "HTTP/1.1 " + code + "\r\n";

	if(content_type)
		sendHeader("Content-Type", content_type, true);
	
	if(((unsigned) _contentLength) == CONTENT_LENGTH_NOT_SET)
		sendHeader("Content-Length", String(contentLength));
	else if(((unsigned) _contentLength) != CONTENT_LENGTH_UNKNOWN)
		sendHeader("Content-Length", String(_contentLength));
	else if(((unsigned) _contentLength) == CONTENT_LENGTH_UNKNOWN) {
		_chunked = true;
		sendHeader("Accept-Ranges","none");
		sendHeader("Transfer-Encoding","chunked");
	}
	sendHeader("Connection", "close");

	response += _responseHeaders;
	response += "\r\n";
}

// ------------------------
void ESPWebDAV::sendContent(const String& content) {
// ------------------------
	const char * footer = "\r\n";
	size_t size = content.length();
	
	if(_chunked) {
		char * chunkSize = (char *) malloc(11);
		if(chunkSize) {
			sprintf(chunkSize, "%x%s", size, footer);
			client.write(chunkSize, strlen(chunkSize));
			free(chunkSize);
		}
	}
	
	client.write(content.c_str(), size);
	
	if(_chunked) {
		client.write(footer, 2);
		if (size == 0) {
			_chunked = false;
		}
	}
}

// ------------------------
void ESPWebDAV::sendContent_P(PGM_P content) {
// ------------------------
	const char * footer = "\r\n";
	size_t size = strlen_P(content);
	
	if(_chunked) {
		char * chunkSize = (char *) malloc(11);
		if(chunkSize) {
			sprintf(chunkSize, "%x%s", size, footer);
			client.write(chunkSize, strlen(chunkSize));
			free(chunkSize);
		}
	}
	
	client.write_P(content, size);
	
	if(_chunked) {
		client.write(footer, 2);
		if (size == 0) {
			_chunked = false;
		}
	}
}

// ------------------------
void ESPWebDAV::setContentLength(size_t len)	{
// ------------------------
	_contentLength = len;
}

// ------------------------
size_t ESPWebDAV::readBytesWithTimeout(uint8_t *buf, size_t bufSize) {
// ------------------------
	int timeout_ms = HTTP_MAX_POST_WAIT;
	size_t numAvailable = 0;
	while(!(numAvailable = client.available()) && client.connected() && timeout_ms--) 
		delay(1);

	if(!numAvailable)
		return 0;

	return client.read(buf, bufSize);
}

// ------------------------
size_t ESPWebDAV::readBytesWithTimeout(uint8_t *buf, size_t bufSize, size_t numToRead) {
// ------------------------
	int timeout_ms = HTTP_MAX_POST_WAIT;
	size_t numAvailable = 0;
	
	while(((numAvailable = client.available()) < numToRead) && client.connected() && timeout_ms--) 
		delay(1);

	if(!numAvailable)
		return 0;

	return client.read(buf, bufSize);
}

String printDirectory(File dir, int numTabs) {
  String str;	
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }

	String entryname = String(entry.name());

	if (!entryname.startsWith(".")) {
		for (uint8_t i = 0; i < numTabs; i++) {
			str = str + '\t';
		}

		str = str + entry.name();

		if (entry.isDirectory()) {
			str = str + "/\n" + printDirectory(entry, numTabs + 1);
		} else {
			// files have sizes, directories do not
			str = str + "\t\t";
			char buf[10];
			sprintf(buf, "%d", entry.size());
			str = str + buf;
			time_t cr = entry.getCreationTime();
			time_t lw = entry.getLastWrite();
			struct tm * tmstruct = localtime(&cr);
			char buf2[100];
			sprintf(buf2, "\tCREATION: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
			str = str + buf2;
			tmstruct = localtime(&lw);
			char buf3[100];
			sprintf(buf3, "\tLAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
			str = str + buf3;
			yield();
		}
	}
    entry.close();
  }
  return str;
}

// ------------------------
boolean takeBusControl()	{
// ------------------------
	// while(millis() < spiBlockoutTime) {
	// 	blink();
	// 	ArduinoOTA.handle();
	// 	SER.handle();
	// }

	weHaveBus = true;
	LED_ON;
	pinMode(MISO, SPECIAL);	
	pinMode(MOSI, SPECIAL);	
	pinMode(SCLK, SPECIAL);	
	pinMode(SD_CS, OUTPUT);

	boolean res = SD.begin(SD_CS);
	if (!res) relenquishBusControl();
	return res;
}

// ------------------------
void relenquishBusControl()	{
// ------------------------

	SD.end();

	pinMode(MISO, INPUT);	
	pinMode(MOSI, INPUT);	
	pinMode(SCLK, INPUT);	
	pinMode(SD_CS, INPUT);
	LED_OFF;
	weHaveBus = false;
}

