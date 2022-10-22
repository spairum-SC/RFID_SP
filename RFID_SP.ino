#include <SPI.h>
#include <MFRC522.h>
#include "env.h"
#include "EspMQTTClient.h"
#include <ESP8266WiFiMulti.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <Arduino_JSON.h>

Ticker blinkLed;
ESP8266WiFiMulti wifi_multi;

const uint16_t connectTimeOutPerAP = 3000; // Defines the TimeOut(ms) which will be used to try and connect with any specific Access Point

#define RST_PIN D1
#define SDA_PIN D2

const int Pompa = D4;
const unsigned int TRIG_PIN = D0;
const unsigned int ECHO_PIN = D3;
String ID_mesin = DEVICE_ID;
int run = 0;
boolean successfulRead = false;
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
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(Pompa, OUTPUT);
  digitalWrite(Pompa, HIGH); // OFF
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
}

void loop()
{
  client.loop();
  while (!client.isConnected())
  {
    client.loop();
    Serial.print("internet putus : ");
    Serial.println(client.getConnectionEstablishedCount()); // count internet lost
    Serial.print("Wifi terhubung : ");
    Serial.println(WiFi.SSID());
    if (!client.isWifiConnected())
    {
      Serial.print("Wifi Tidak terhubung : ");
      Serial.println(WiFi.SSID());
      Serial.println("WiFi Disconnected!!!");
      Serial.println("Establishing a connection with a nearby Wi-Fi...");
      if (wifi_multi.run(connectTimeOutPerAP) == WL_CONNECTED)
      {
        Serial.print("WiFi connected: ");
        Serial.print(WiFi.SSID());
        Serial.print(" ");
        Serial.println(WiFi.localIP());
      }
      else
      {
        Serial.println("WiFi not connected!");
      }
    }
    else if (!client.isMqttConnected())
    {
      Serial.println("Sedang Mengubungkan Spairum Server = " + String(client.getMqttServerIp()));
    }
    delay(1000);
  }
  // do
  // {
  //   successfulRead = getUID(); // Check if a card has been read successfully
  // } while (!successfulRead);
  while (successfulRead)
  {
    client.loop();
    Serial.println("RFID terbaca, Menungggu Mengisi");
    delay(1000);
  }
  successfulRead = getUID(); // Check if a card has been read successfully
}

void onConnectionEstablished()
{
  // client.subscribe("mesin/test/RFID" + ID_mesin, [](const String &payload)
  //                  {
  //                    Serial.println("RFID : ");
  //                    do {
  //   successfulRead = getUID();        // Check if a card has been read successfully
  // }
  // while (!successfulRead); });
  client.subscribe("mesin/fill/" + ID_mesin, [](const String &payload)
                   { Serial.println("FILL Water ");
                   successfulRead = false; });
  client.subscribe("mesin/rejection/" + ID_mesin, [](const String &payload)
                   { Serial.println(payload);
                   successfulRead = false; });
  client.publish("mesin/status/online", ID_mesin);
  JSONVar TX;
  TX["clientid"] = ID_mesin;
  TX["Wifi"] = WiFi.SSID();
  TX["IP"] = WiFi.localIP().toString();
  String jsonString = JSON.stringify(TX);
  client.publish("mesin/data/log", jsonString.c_str());
  client.subscribe("cleanUp", [](const String &payload)
                   {
    Serial.println(payload);
    StaticJsonDocument<96> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      //  Serial.println(error.f_str());
      return;
    }
    // String id_newIdmesin status;
    uint8_t state;
    uint8_t id_Pompa;
    String status = doc["status"];
    String id_newIdmesin = doc["id_mesin"];
    Serial.println(id_newIdmesin);

    if (id_newIdmesin == ID_mesin)
    {
      Serial.println("id_newIdmesin");
      id_Pompa = Pompa;
      if (status == "LOW")
      {
        // blinkLed.attach_ms(1000, tick);
        state = LOW; // ON
      }
      else
      {
        // blinkLed.detach();
        state = HIGH; // OFF
      }
      cleanOn(id_Pompa, state);
    } });

  client.subscribe("mesin/status/ping", [](const String &payload)
                   {
    long rssi = WiFi.RSSI();
    long valule = mersure();
    // long valule =  random(300);
    JSONVar TX;
    TX["clientid"] = ID_mesin;
    TX["RSSI"] = rssi;
    TX["vaule"] = valule;
    String jsonString = JSON.stringify(TX);
    client.publish("mesin/status/rssi", jsonString.c_str()); });

  client.subscribe("mesin/status/wifi/" + ID_mesin, [](const String &payload)
                   {
    JSONVar TX;
    TX["clientid"] = ID_mesin;
    TX["Wifi"] = WiFi.SSID();
    TX["IP"] =  WiFi.localIP().toString();
    String jsonString = JSON.stringify(TX);
    client.publish("mesin/data/log", jsonString.c_str()); });

  // blinkLed.attach_ms(2000, tick);

  client.subscribe("stop/" + ID_mesin, [](const String &payload)
                   {
  run = 1;
  Serial.println("BTN stop");
  return; });
}

boolean getUID()
{
  client.loop();
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
  {
    Serial.println("Read");
    delay(50);
    return false;
  }

  Serial.println("Read");
  Serial.print("UID tag :");
  String content = "";

  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    // Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    // Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  Serial.println();

  Serial.print("Pesan : ");
  content.toUpperCase();
  Serial.println(content);
  client.publish("mesin/uid/ID_MESIN" + ID_mesin, content);
  mfrc522.PICC_HaltA(); // Stop reading the card and tell it to switch off
  return true;
}

long mersure()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  const unsigned long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration / 29 / 2;
  return distance;
}

void cleanOn(uint8_t id_Pompa, uint8_t Status)
{
  digitalWrite(id_Pompa, Status); //  LOW == ON
  Serial.print("Clean Run ");
  Serial.print(id_Pompa);
}