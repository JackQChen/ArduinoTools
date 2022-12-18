#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <WebSocketsClient.h>

#include <PS4Controller.h>

WiFiMulti wifiMulti;
WebSocketsClient webSocket;

void setup() {

	Serial.begin(115200);
	while (!Serial);
	Serial.print("Starting...");

	PS4.begin();

	wifiMulti.addAP("Controller", "Res112358");

	while (wifiMulti.run() != WL_CONNECTED) {
		delay(100);
	}

	webSocket.begin("192.168.4.1", 81, "/");
	webSocket.setReconnectInterval(5000);

	Serial.println("Ready.");
}

unsigned long currentMillis, previousMillis, period = 50;
uint8_t arr[6] = { 3, 0, 0, 0, 0, 0 };

void loop() {
	webSocket.loop();

	if (!webSocket.isConnected())
		return;

	currentMillis = millis();
	if (currentMillis >= previousMillis && currentMillis - previousMillis < period)
		return;
	previousMillis = currentMillis;

	int left = PS4.LStickY() * 20;
	int right = PS4.RStickY() * 20;

	arr[1] = (0xff00 & left) >> 8;
	arr[2] = 0xff & left;

	arr[3] = (0xff00 & right) >> 8;
	arr[4] = 0xff & right;

	arr[5] = ~(arr[0] + arr[1] + arr[2] + arr[3] + arr[4]);

	webSocket.sendBIN(arr, 6);
}