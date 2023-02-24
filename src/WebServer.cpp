#include "LittleFS.h"
#include <EEPROM.h>
#include <SD.h>

#include "WebServer.h"

#define SERVER_PORT		80

#define SD_CS		4
#define MISO		12
#define MOSI		13
#define SCLK		14
#define CS_SENSE	5
#define SPI_BLOCKOUT_PERIOD	20000UL

bool reboot = false;
char buffer[255];
File uploadFile;

bool weHaveBus = false;
volatile unsigned long spiBlockoutTime = 0;
void IRAM_ATTR ISRoutine ();

ESP8266WebServer server(SERVER_PORT);
String printDirectory(FileSystem fs, File dir, int numTabs);

// ------------------------
void WebServer::init() {
// ------------------------
	// attach to the SD card sense pin (we don't do anything with it though)
	pinMode(CS_SENSE, INPUT);
	attachInterrupt(CS_SENSE, ISRoutine, FALLING);

	// logging hook
	server.addHook([this](const String& method, const String& url, WiFiClient* client, ESP8266WebServer::ContentTypeFunction contentType) {
		struct tm timeinfo;
		if(!getLocalTime(&timeinfo)) {
			SER.println("Failed to obtain time");
		}
		sprintf(buffer, "\n%4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
		SER.println(buffer);

		sprintf(buffer, "%.4f %s %s %s %s %d", (float) millis() / 1000, method.c_str(), url.c_str(), getMimeType(url).c_str(), client->remoteIP().toString().c_str(), client->getKeepAliveCount());
		SER.println(buffer);
		return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
	});

	server.serveStatic("/", LittleFS, "/index.html");
	server.serveStatic("/index.html", LittleFS, "/index.html");
	server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");

 	server.on("/uploadfs", HTTP_POST, []() { server.send(200); }, [this]() {handleFileUpload(LFS); });
 	server.on("/uploadsd", HTTP_POST, []() { server.send(200); }, [this]() {handleFileUpload(SDCARD); });

	server.onNotFound([this]() { handleRequest(""); });
	server.begin();

	return;
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
String WebServer::getMimeType(String path) {
// ------------------------
	if(path.endsWith(".html")) return "text/html";
	else if(path.endsWith(".htm")) return "text/html";
	else if(path.endsWith(".css")) return "text/css";
	else if(path.endsWith(".txt")) return "text/plain";
	else if(path.endsWith(".gcode")) return "text/plain";
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
String WebServer::urlDecode(const String& text)	{
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
String WebServer::urlToUri(String url)	{
// ------------------------
	if(url.startsWith("http://"))	{
		int uriStart = url.indexOf('/', 7);
		return url.substring(uriStart);
	}
	else
		return url;
}

// ------------------------
void WebServer::handleClient(String blank) {
// ------------------------
	server.handleClient();
}

void WebServer::handleRequest(String blank)	{
	uri = urlDecode(server.uri());
	method = server.method();
	client = server.client();

	// default document
	if (method == HTTP_GET && uri.equals("/")) {
		String index = "/index.html";
		File file = LittleFS.open(index, "r");
		server.streamFile(file, getMimeType(index));
		file.close();
		return;
	}

	// reboot 
	if (method == HTTP_GET  && uri.equalsIgnoreCase("/reboot")) {
		server.sendHeader("Location","/index.html?reboot");
		server.send(303);
		reboot = true;
		return;
	}

	// save all 
	if (method == HTTP_GET  && uri.startsWith("/save/")) {
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
		EEPROM.end();

		updateIndexTemplate(config.hostname, config.ssid, config.pwd);

		server.sendHeader("Location", "/index.html?saved");
		server.send(303);

		SER.println("config saved");
		return;
	}

	// littlefs directory listing
	if (method == HTTP_GET  && (uri.equals("/fs") || uri.equals("/fs/"))) {
		String str = printDirectory(FileSystem::LFS, LittleFS.open("/", "r"), 0);
		server.send(200, "text/html", str);
		return;
	}

	// SD card directory listing
	if (method == HTTP_GET  && (uri.equals("/sd") || uri.equals("/sd/"))) {
		if (takeBusControl()) {
			String str = printDirectory(FileSystem::SDCARD, SD.open("/", "r"), 0);
			relenquishBusControl();
			server.send(200, "text/html", str);
		} else {
			server.send(404, "text/plain", "SD CARD NOT READY");
			SER.println("SD CARD NOT READY");
		}
		return;
	}

	// download file
	if (uri.startsWith("/sd") || uri.startsWith("/fs")) {
		String path = uri;
		if (uri.startsWith("/sd")) {
			path.replace("/sd/", "/");
			if (takeBusControl()) {
				File file = SD.open(path, FILE_READ);
				if (file) {
					server.streamFile(file, getMimeType(path));
					file.close();
					relenquishBusControl();
					return;
				}
				relenquishBusControl();
			} else {
				server.send(404, "text/plain", "SD CARD NOT READY");
				return;
			}
		} else {
			path.replace("/fs/", "/");
			File file = LittleFS.open(path, "r");
			if (file) {
				server.streamFile(file, getMimeType(path));
				file.close();
				return;
			}
		}
	}

	// delete file
	if (uri.startsWith("/DELETESD/") || uri.startsWith("/DELETEFS/")) {
		String path = uri;
		boolean res = false;

		if (uri.startsWith("/DELETESD/")) {
			path.replace("/DELETESD/", "/");
			takeBusControl();
			File file = SD.open(path, "r");	
			if (file.isDirectory()) {
				file.close();
				res = SD.rmdir(path);
			} else {
				file.close();
				res = SD.remove(path);
			}
			String str = printDirectory(FileSystem::SDCARD, SD.open("/", "r"), 0);
			relenquishBusControl();
			server.send(200, "text/html", str);
		} else {
			path.replace("/DELETEFS/", "/");
			File file = LittleFS.open(path, "r");
			if (file.isDirectory()) {
				file.close();
				res = LittleFS.rmdir(path);
			} else {
				file.close();
				res = LittleFS.remove(path);
			}
			String str = printDirectory(FileSystem::LFS, LittleFS.open("/", "r"), 0);
			server.send(200, "text/html", str);
		}
		SER.print("file deleted="); SER.println(res ? "true" : "false");
		return;
	}

	// 404
	String message = "Not found\n";
	message += "URI: ";
	message += uri;
	message += " Method: ";
	message += method;
	message += "\n";

	server.send(404, "text/plain", message);
	SER.println("404");
}

void WebServer::handleFileUpload(FileSystem fs) { 
	HTTPUpload& upload = server.upload();

	if (upload.status == UPLOAD_FILE_START) {
		String path = server.arg("path");
		if (!path.endsWith("/")) path = path + "/";

		if (fs == SDCARD) takeBusControl();
		if (fs == SDCARD ? !SD.exists(path) : !LittleFS.exists(path)) {
			sprintf(buffer, "creating directory [%s]", path.c_str());
			SER.println(buffer);
			if (fs == SDCARD) 
				SD.mkdir(path);
			else
				LittleFS.mkdir(path);
		}
		if (fs == SDCARD)
			uploadFile = SD.open(path + upload.filename, "w"); 
		else 
			uploadFile = LittleFS.open(path + upload.filename, "w"); 

		sprintf(buffer, "uploading [%s] to [%s] . . .", upload.filename.c_str(), fs == SDCARD ? "SD" : "LittleFS");
		SER.println(buffer);
		return;
	}

	if (upload.status == UPLOAD_FILE_WRITE && uploadFile) {
		uploadFile.write(upload.buf, upload.currentSize);
	}

	if (upload.status == UPLOAD_FILE_END) {
		if(uploadFile) {           
			uploadFile.close();    
			server.sendHeader("Location","/index.html?upload_complete");
			server.send(303);
			sprintf(buffer, "completed [%s]", upload.filename.c_str());
			SER.println(buffer);
		} else {
			sprintf(buffer, "upload failed for [%s]", upload.filename.c_str());
			SER.println(buffer);
			server.sendHeader("Location","/index.html?upload_failed");
			server.send(303);
		}
		if (fs == SDCARD) relenquishBusControl();
 	}
}

String printDirectory(FileSystem fs, File dir, int numTabs) {
  	String str;	
	if (numTabs == 0) {
		str = "<b>ESPWebSvr ";
		str = str + (fs == SDCARD ? "SD Card" : "LittleFS"); 
		str = str + " File List</b><pre>";
	}

	while (true) {
		File entry =  dir.openNextFile();
		if (! entry) {
		// no more files
		break;
		}

		String entryname = String(entry.name());

		if (!entryname.startsWith(".")) {
			if (!entry.isDirectory()) 
				str = str + "<a href='/" + (fs == SDCARD ? "sd" : "fs") + "/" + entry.fullName() + "'>DOWNLOAD</a>\t";

			for (uint8_t i = 0; i < numTabs; i++) {
				str = str + '\t';
			}

			char nameBuf[34];
			if (entry.isDirectory()) {
				char dirname[strlen(entry.name() + 1)];
				strcpy(dirname, entry.name());
				sprintf(nameBuf, "\t\t%-31.31s", strcat(dirname, "/"));
			} else {
				sprintf(nameBuf, "%-30.30s", entry.name());
			}

			str = str + nameBuf;

			if (entry.isDirectory()) {
				str = str + "\t<a href='/DELETE" + (fs == SDCARD ? "SD" : "FS") + "/" + entry.fullName() + "'>DELETE</a>\n" + printDirectory(fs, entry, numTabs + 1);
			} else {
				// files have sizes, directories do not - captain obvious
				str = str + "\t";
				char buf[11];
				sprintf(buf, "%10d", entry.size());
				str = str + buf;
				time_t cr = entry.getCreationTime();
				time_t lw = entry.getLastWrite();
				struct tm * tmstruct = localtime(&cr);
				char buf2[100];
				sprintf(buf2, "\tCREATION: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
				str = str + buf2;
				tmstruct = localtime(&lw);
				char buf3[100];
				sprintf(buf3, "\tLAST WRITE: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
				str = str + buf3;
				str = str + "\t<a href='/DELETE" + (fs == SDCARD ? "SD" : "FS") + "/" + entry.fullName() + "'>DELETE</a>\n";
				yield();
			}
		}
		entry.close();
	}

	if (numTabs == 0) str = str + "</pre><br><a href='/index.html'>Go Back</a>";
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

