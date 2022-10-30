#include <SPI.h>
#include <MFRC522.h>
#include "env.h"
#include "EspMQTTClient.h"
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <Arduino_JSON.h>

ESP8266WiFiMulti wifi_multi;

const uint16_t connectTimeOutPerAP = 3000; // Defines the TimeOut(ms) which will be used to try and connect with any specific Access Point

#define RST_PIN D1 // MFRC522 Software Reset Pin
#define SDA_PIN D2 // MFRC522 Software SPI Pin (SDA)

const int Pompa = D4;             // Pin untuk pompa
const int Pompa2 = D8;            // Pin untuk pompa 2
const unsigned int TRIG_PIN = D0; // Pin untuk sensor jarak
const unsigned int ECHO_PIN = D3; // Pin untuk sensor jarak
String ID_mesin = DEVICE_ID;
int run = 0;
boolean successfulRead = false;
MFRC522 mfrc522(SDA_PIN, RST_PIN); // Create MFRC522 instance

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
  pinMode(Pompa2, OUTPUT);
  digitalWrite(Pompa, HIGH);  // OFF
  digitalWrite(Pompa2, HIGH); // OFF
  mfrc522.PCD_Init();
  // lcd.init();
  Serial.println("Spairum WiFi RFID");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  // Setting the AP Mode with SSID, Password, and Max Connection Limit
  // Adding the WiFi networks to the MultiWiFi instance
  wifi_multi.addAP(SECRET_SSID1, SECRET_PASS1);
  wifi_multi.addAP(SECRET_SSID2, SECRET_PASS2);

  client.enableMQTTPersistence();
  client.setMqttReconnectionAttemptDelay(2000);
  client.enableDebuggingMessages();                                     // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater();                                        // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA();                                                   // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.setKeepAlive(4);                                               // keepalive interval.
  client.enableLastWillMessage("mesin/status/offline", MQTT_CLIENT_ID); // You can activate the retain flag by setting the third parameter to true
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
  int cont = 0;
  while (successfulRead)
  {
    client.loop();
    Serial.println("RFID terbaca, mulai Mengisi");
    delay(1000);
    if (cont > 10)
    {
      successfulRead = false;
    }
    cont++;
  }
  successfulRead = getUID(); // Check if a card has been read successfully
}

void onConnectionEstablished()
{

  client.publish("mesin/status/online", ID_mesin);
  JSONVar TX;
  TX["clientid"] = ID_mesin;
  TX["Wifi"] = WiFi.SSID();
  TX["IP"] = WiFi.localIP().toString();
  String jsonString = JSON.stringify(TX);
  client.publish("mesin/data/log", jsonString.c_str());

  client.subscribe("mesin/fill/" + ID_mesin, [](const String &payload, uint8_t qos = 2)
                   { Serial.println("FILL Water");
                  StaticJsonDocument<96> doc;
                  DeserializationError error = deserializeJson(doc, payload);

                  if (error) {
                    Serial.print(F("deserializeJson() failed: "));
                    Serial.println(error.f_str());
                    return;
                  }
                  const char* ID_USER = doc["ID_USER"]; // "13CA5AA9"
                  int MAX_ML = doc["MAX_ML"]; // 1000
                  float FAKTOR_POMPA = doc["FAKTOR_POMPA"]; // 1.1
                  int sisa = refillCard(MAX_ML, FAKTOR_POMPA);
                  int vaule = sisa * FAKTOR_POMPA;
                    JSONVar TX;
                  TX["ID_USER"] = ID_USER;
                  TX["vaule"] = vaule;
                  String jsonString = JSON.stringify(TX);
                  client.publish("mesin/endRefill/" + ID_mesin, jsonString.c_str());
                   successfulRead = false; });
  client.subscribe("mesin/rejection/" + ID_mesin, [](const String &payload, uint8_t qos = 2)
                   { Serial.println(payload);
                   successfulRead = false; });
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

        state = LOW; // ON
      }
      else
      {
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

  client.subscribe("stop/" + ID_mesin, [](const String &payload)
                   {
  run = 1;
  Serial.println("BTN stop");
  return; });

  client.subscribe("start/" + ID_mesin, [](const String &payload)
                   {
    Serial.println(payload);
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      //  Serial.println(error.f_str());
      return;
    }
    const char *id = doc["id"];                 // "ProBali2"
    const char *akun = doc["akun"];             // "51E5EF69"
    const char *nama = doc["nama"];             // ""
    const char *mesinID = doc["mesinID"];       // ""
    int vaule = doc["vaule"];                   // "220"
    int faktor = doc["faktor"];                 // "1"
    int HargaTotal = doc["HargaTotal"];         // "4400"
    Serial.print(id);
    Serial.print(akun);
    Serial.println(vaule);
    Serial.println("Mulai Mengisi");
    uint8_t id_Pompa = Pompa;
    //  String ID_mesin = "ID_mesin";
    run = 0;
    int x = refill(vaule, akun, faktor, id_Pompa, ID_mesin);
    if (x == 1)
    {
      client.publish("nodeTrans", (payload));
      Serial.println("Selesai Mengisi");
      delay(2000);
      return;
    }
    else
    {
      int harga = (HargaTotal / vaule) * x;
      Serial.println(x);
      Serial.println(harga);
      String harga_string = String(harga);
      JSONVar nodeTr;
      nodeTr["id"] = id;
      nodeTr["mesinID"] = mesinID;
      nodeTr["nama"] = nama;
      nodeTr["akun"] = akun;
      nodeTr["vaule"] = x;
      nodeTr["faktor"] = faktor;
      nodeTr["HargaTotal"] = harga_string;
      String nodeTran = JSON.stringify(nodeTr);
      client.publish("nodeTrans", (nodeTran));
    } });
}

int refillCard(int MAX_ML, float FAKTOR_POMPA)
{
  unsigned long nilai = (MAX_ML / FAKTOR_POMPA);
  float runvar = 0;
  run = 0;
  for (int x = 0; x < nilai;)
  {
    client.loop();
    x += 10;
    runvar = x;
    digitalWrite(Pompa, LOW);

    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    {
      Serial.println("Reax");
      delay(50);
    }
    else
    {
      digitalWrite(Pompa, HIGH);
      Serial.println("RFID Stop Mengisi");
      return runvar;
    }
    delay(100);
  }
  digitalWrite(Pompa, HIGH);
  return runvar;
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
  client.publish("mesin/uid/" + ID_mesin, content, 2);
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

int refill(int vaule, const char *akun, int faktor, uint8_t id_Pompa, String ID_mesin)
{
  run = 0;
  client.loop();
  Serial.print("Mulai ");
  Serial.println(vaule);
  Serial.println(akun);
  JSONVar update;
  update["vaule"] = vaule;
  update["Status"] = ("Megisi");
  update["user"] = akun;
  String dataupdate = JSON.stringify(update);
  client.publish("update", dataupdate.c_str()); // update status undifined
  delay(2500);
  int nilai = vaule * 100 / faktor;
  unsigned long time_now = millis();
  unsigned long finish = time_now + nilai;
  while (time_now < finish)
  {
    client.loop();
    if (run == 1)
    {
      digitalWrite(id_Pompa, HIGH);
      long x = finish - time_now;
      Serial.print("Force Stop ");
      Serial.println(x);
      int sisa = (x * faktor) / 100;
      Serial.print("Sisa ");
      Serial.print(sisa);
      Serial.println(" ml");
      // time_now = finish;
      JSONVar stop;
      stop["Status"] = ("Selesai");
      stop["user"] = akun;
      String datastop = JSON.stringify(stop);
      client.publish("current", datastop.c_str());
      return sisa;
    }
    digitalWrite(id_Pompa, LOW);
    time_now = millis();
  }
  Serial.println("selesai sempurna");
  JSONVar stop;
  stop["vaule"] = vaule;
  stop["Status"] = ("Selesai");
  stop["user"] = akun;
  String datastop = JSON.stringify(stop);
  client.publish("current", datastop.c_str());
  digitalWrite(id_Pompa, HIGH);
  Serial.print("Selesai ");
  return 1;
}