#include <SPI.h>
#include <MFRC522.h>
#include "env.h"
#include "EspMQTTClient.h"

#include <Ticker.h>
#include <ArduinoJson.h>
#include <Arduino_JSON.h>

Ticker blinkLed;
ESP8266WiFiMulti wifi_multi;

const uint16_t connectTimeOutPerAP = 3000; // Defines the TimeOut(ms) which will be used to try and connect with any specific Access Point

#define RST_PIN D1
#define SDA_PIN D2

String ID_mesin = DEVICE_ID;

MFRC522 mfrc522(SDA_PIN, RST_PIN);

EspMQTTClient client(
    MQTT_SERVER,   // MQTT Broker server ip
    MQTT_PORT,     // The MQTT port, default to 1883. this line can be omitted
    MQTT_USER,     // Can be omitted if not needed
    MQTT_PASS,     // Can be omitted if not needed
    MQTT_CLIENT_ID // Client name that uniquely identify your device
);

void setup()
{
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Spairum WiFi RFID");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  // Setting the AP Mode with SSID, Password, and Max Connection Limit
  // Adding the WiFi networks to the MultiWiFi instance
  wifi_multi.addAP(SECRET_SSID1, SECRET_PASS1);
  wifi_multi.addAP(SECRET_SSID2, SECRET_PASS2);

  client.enableMQTTPersistence();
  client.setMqttReconnectionAttemptDelay(2000);
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater();    // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA();               // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.setKeepAlive(4);           // keepalive interval.
  // client.enableLastWillMessage("mesin/status/offline", MQTT_CLIENT_ID); // You can activate the retain flag by setting the third parameter to true
  blinkLed.detach();
}

void loop()
{
  getUID();
  delay(1000);
}

void onConnectionEstablished()
{
  client.subscribe("mesin/status/wifi/" + ID_mesin, [](const String &payload)
                   {
    JSONVar TX;
    TX["clientid"] = ID_mesin;
    TX["Wifi"] = WiFi.SSID();
    TX["IP"] =  WiFi.localIP().toString();
    String jsonString = JSON.stringify(TX);
    client.publish("mesin/data/log", jsonString.c_str()); });

  blinkLed.attach_ms(2000, tick);

  client.subscribe("stop/" + ID_mesin, [](const String &payload)
                   {
  run = 1;
  Serial.println("BTN stop");
  return; });
  // mersure();
  // blinkLed.detach();
}
}
void getUID()
{
  Serial.println("Read");
  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }

  Serial.print("UID tag :");
  String content = "";
  byte letter;

  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  Serial.println();

  //  Serial.print("Pesan : ");
  content.toUpperCase();
  Serial.println(content);
}
