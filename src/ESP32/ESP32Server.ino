/*
 1.PC connect the wifi : Controller
              password : 123456789
 2.Browser open: http://192.168.4.1/

 3.click the on or off button
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <WiFiMulti.h>

#define USE_SERIAL Serial

static const char ssid[] = "Controller";
static const char password[] = "123456789";
MDNSResponder mdns;

static void writeLED(bool);

WiFiMulti wifiMulti;

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0,minimum-scale=1.0,maximum-scale=1.0,user-scalable=no">
        <meta http-equiv="X-UA-Compatible" content="ie=edge">
        <title>Controller</title>
        <script>
            function disableTouch() {
                var lastTouchEnd = 0;
                document.addEventListener('touchstart', function(event) {
                    if (event.touches.length > 1) {
                        event.preventDefault();
                    }
                });
                document.addEventListener('touchend', function(event) {
                    var now = (new Date()).getTime();
                    if (now - lastTouchEnd <= 300) {
                        event.preventDefault();
                    }
                    lastTouchEnd = now;
                }, false);
                document.addEventListener('gesturestart', function(event) {
                    event.preventDefault();
                });
                document.addEventListener('dblclick', function(event) {
                    event.preventDefault();
                });
                document.addEventListener("touchmove", function(e) {
                    passive: false;
                }, false);
            }
            var websocket;
            var _reconnect;
            function init() {
                websocket = new WebSocket('ws://' + window.location.hostname + ':81/');
                websocket.onopen = function(evt) {
                    clearInterval(_reconnect);
                    console.log("CONNECTED");
                }
                websocket.onclose = function(evt) {
                    console.log("DISCONNECTED");
                }
                websocket.onmessage = function(evt) {
                    document.getElementById('chkLight').checked = evt.data == "ledon";
                }
                websocket.onerror = function(evt) {
                    console.log('ERROR:' + evt.data);
                }
            }
            function reconnect() {
                _reconnect = setInterval(function() {
                    if (websocket.readyState != WebSocket.OPEN)
                        init();
                }, 10000);
            }
            function checkClick(e) {
                websocket.send(e.checked ? "ledon" : "ledoff");
            }
        </script>
        <style>
            * {
                -webkit-touch-callout: none;
                -webkit-user-select: none;
                -khtml-user-select: none;
                -moz-user-select: none;
                -ms-user-select: none;
                user-select: none;
            }

            html,body {
                position: fixed;
                width: 100%;
                height: 100%;
                margin: 0px;
            }

            input {
                position: absolute;
                left: -9999px;
            }

            input:checked + .slider::before {
                box-shadow: 0 0.08em 0.15em -0.1em rgba(0, 0, 0, 0.5) inset, 0 0.05em 0.08em -0.01em rgba(255, 255, 255, 0.7), 3em 0 0 0 rgba(68, 204, 102, 0.7) inset;
            }

            input:checked + .slider::after {
                left: 3em;
            }

            .slider {
                -webkit-tap-highlight-color: rgba(255,0,0,0);
                position: relative;
                display: block;
                width: 5.5em;
                height: 3em;
                cursor: pointer;
                border-radius: 1.5em;
                transition: 350ms;
                background: linear-gradient(rgba(0, 0, 0, 0.07), rgba(255, 255, 255, 0)), #ddd;
                box-shadow: 0 0.07em 0.1em -0.1em rgba(0, 0, 0, 0.4) inset, 0 0.05em 0.08em -0.01em rgba(255, 255, 255, 0.7);
            }

            .slider::before {
                position: absolute;
                content: '';
                width: 4em;
                height: 1.5em;
                top: 0.75em;
                left: 0.75em;
                border-radius: 0.75em;
                transition: 250ms ease-in-out;
                background: linear-gradient(rgba(0, 0, 0, 0.07), rgba(255, 255, 255, 0.1)), #d0d0d0;
                box-shadow: 0 0.08em 0.15em -0.1em rgba(0, 0, 0, 0.5) inset, 0 0.05em 0.08em -0.01em rgba(255, 255, 255, 0.7), 0 0 0 0 rgba(68, 204, 102, 0.7) inset;
            }

            .slider::after {
                position: absolute;
                content: '';
                width: 2em;
                height: 2em;
                top: 0.5em;
                left: 0.5em;
                border-radius: 50%;
                transition: 250ms ease-in-out;
                background: linear-gradient(#f5f5f5 10%, #eeeeee);
                box-shadow: 0 0.1em 0.15em -0.05em rgba(255, 255, 255, 0.9) inset, 0 0.2em 0.2em -0.12em rgba(0, 0, 0, 0.5);
            }
        </style>
    </head>
    <body onload="disableTouch();init();reconnect();">
        <div id="txtLog">LEDStatus</div>
        <div style="position: fixed;top: 10%;width: 100%;display: flex;justify-content: center;align-items: center;">
            <span style="float:left;margin:21px">开关
        </span>
            <input type="checkbox" id="chkLight" onclick="checkClick(this)">
            <label style="float:left" class="slider" for="chkLight"></label>
        </div>
    </body>
</html>
)rawliteral";

void hexdump(const void* mem, uint32_t len, uint8_t cols = 16) {
	const uint8_t* src = (const uint8_t*)mem;
	USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for (uint32_t i = 0; i < len; i++) {
		if (i % cols == 0) {
			USE_SERIAL.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		USE_SERIAL.printf("%02X ", *src);
		src++;
	}
	USE_SERIAL.printf("\n");
}

// GPIO#0 is for Adafruit ESP8266 HUZZAH board. Your board LED might be on 13.
const int LEDPIN = 2;
// Current LED status
bool LEDStatus;

// Commands sent through Web Socket
const char LEDON[] = "ledon";
const char LEDOFF[] = "ledoff";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
{
	USE_SERIAL.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
	switch (type) {
	case WStype_DISCONNECTED:
		USE_SERIAL.printf("[%u] Disconnected!\r\n", num);
		break;
	case WStype_CONNECTED:
	{
		IPAddress ip = webSocket.remoteIP(num);
		USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
		// Send the current LED status
		if (LEDStatus) {
			webSocket.sendTXT(num, LEDON, strlen(LEDON));
		}
		else {
			webSocket.sendTXT(num, LEDOFF, strlen(LEDOFF));
		}
	}
	break;
	case WStype_TEXT:
		USE_SERIAL.printf("[%u] get Text: %s\r\n", num, payload);

		if (strcmp(LEDON, (const char*)payload) == 0) {
			writeLED(true);
		}
		else if (strcmp(LEDOFF, (const char*)payload) == 0) {
			writeLED(false);
		}
		else {
			USE_SERIAL.println("Unknown command");
		}
		// send data to all connected clients
		webSocket.broadcastTXT(payload, length);
		break;
	case WStype_BIN:
		USE_SERIAL.printf("[%u] get binary length: %u\r\n", num, length);
		hexdump(payload, length);

		// echo data back to browser
		webSocket.sendBIN(num, payload, length);
		break;
	default:
		USE_SERIAL.printf("Invalid WStype [%d]\r\n", type);
		break;
	}
}

void handleRoot()
{
	server.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound()
{
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";
	for (uint8_t i = 0; i < server.args(); i++) {
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}
	server.send(404, "text/plain", message);
}

static void writeLED(bool LEDon)
{
	LEDStatus = LEDon;
	// Note inverted logic for Adafruit HUZZAH board
	if (LEDon) {
		digitalWrite(LEDPIN, 1);
	}
	else {
		digitalWrite(LEDPIN, 0);
	}
}

void setup()
{
	pinMode(LEDPIN, OUTPUT);
	writeLED(false);

	USE_SERIAL.begin(115200);

	//Serial.setDebugOutput(true);

	USE_SERIAL.println();
	USE_SERIAL.println();
	USE_SERIAL.println();

	for (uint8_t t = 4; t > 0; t--) {
		USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\r\n", t);
		USE_SERIAL.flush();
		delay(1000);
	}

	//  wifiMulti.addAP(ssid, password);
	//
	//  while (wifiMulti.run() != WL_CONNECTED) {
	//    Serial.print(".");
	//    delay(100);
	//  }

	WiFi.softAP(ssid, password);
	IPAddress myIP = WiFi.softAPIP();
	USE_SERIAL.print("AP IP address: ");
	USE_SERIAL.println(myIP);

	USE_SERIAL.println("");
	USE_SERIAL.print("Connected to ");
	USE_SERIAL.println(ssid);
	USE_SERIAL.print("IP address: ");
	USE_SERIAL.println(WiFi.localIP());

	if (mdns.begin("esp32")) {
		USE_SERIAL.println("MDNS responder started");
		mdns.addService("http", "tcp", 80);
		mdns.addService("ws", "tcp", 81);
	}
	else {
		USE_SERIAL.println("MDNS.begin failed");
	}
	USE_SERIAL.print("Connect to http://espWebSock.local or http://");
	USE_SERIAL.println(WiFi.localIP());

	server.on("/", handleRoot);
	server.onNotFound(handleNotFound);

	server.begin();

	webSocket.begin();
	webSocket.onEvent(webSocketEvent);
}

void loop()
{
	webSocket.loop();
	server.handleClient();
}
