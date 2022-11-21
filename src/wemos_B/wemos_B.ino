#include "env.h"
#include "EspMQTTClient.h"
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <Arduino_JSON.h>

#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
ESP8266WiFiMulti wifi_multi;
const long utcOffsetInSeconds = 3600 * 7;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "1.id.pool.ntp.org", utcOffsetInSeconds);

const uint16_t connectTimeOutPerAP = 3000; // Defines the TimeOut(ms) which will be used to try and connect with any specific Access Point

// const int Pompa = D8;             // Pin untuk pompa
// const int Pompa2 = D4;            // Pin untuk pompa 2
const unsigned int TRIG_PIN = D5; // Pin untuk sensor jarak //D5
const unsigned int ECHO_PIN = D6; // Pin untuk sensor jarak //D6
String ID_mesin = DEVICE_ID;
LiquidCrystal_I2C lcd(0x27, 20, 4); // atau 0x3F
EspMQTTClient client(
    MQTT_SERVER,   // MQTT Broker server ip
    MQTT_PORT,     // The MQTT port, default to 1883. this line can be omitted
    MQTT_USER,     // Can be omitted if not needed
    MQTT_PASS,     // Can be omitted if not needed
    MQTT_CLIENT_ID // Client name that uniquely identify your device
);

void setup()
{
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Spairum Setup   ");
  lcd.setCursor(0, 1);
  // lcd.scrollDisplayLeft();
  lcd.print("Tunggu ya       ");
  delay(1000);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("Spairum WiFi RFID");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  // Setting the AP Mode with SSID, Password, and Max Connection Limit
  // Adding the WiFi networks to the MultiWiFi instance
  wifi_multi.addAP(SECRET_SSID1, SECRET_PASS1);
  wifi_multi.addAP(SECRET_SSID2, SECRET_PASS2);

  client.enableMQTTPersistence();
  client.setMqttReconnectionAttemptDelay(2000);
  client.enableDebuggingMessages();                                      // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater();                                         // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  client.enableOTA();                                                    // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  client.setKeepAlive(4);                                                // keepalive interval.
  client.enableLastWillMessage("mesin/status/offline/ESP_B", DEVICE_ID); // You can activate the retain flag by setting the third parameter to true
}

void loop()
{
  client.loop();
  timeClient.update();
  long Menit = timeClient.getMinutes();
  lcd.setCursor(0, 0);
  lcd.print("Spairum Siap       ");
  lcd.setCursor(0, 1);
  lcd.print(timeClient.getFormattedTime());
  lcd.print("                    ");
  while (!client.isConnected())
  {
    lcd.home();
    client.loop();
    Serial.print("internet putus : ");
    Serial.println(client.getConnectionEstablishedCount()); // count internet lost
    Serial.print("Wifi terhubung : ");
    Serial.println(WiFi.SSID());
    lcd.setCursor(0, 0);
    lcd.print("Spairium Offline     ");
    lcd.setCursor(0, 1);
    lcd.print("Connecting to a wifi");
    lcd.setCursor(0, 2);
    lcd.print(WiFi.SSID());
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
      Serial.println("Sedang Mengubungkan Spairum Server = " + String(client.getMqttServerIp()));
      lcd.setCursor(0, 0);
      lcd.print("MQTT to Server : ");
      lcd.setCursor(0, 1);
      lcd.print(String(client.getMqttServerIp()));
      lcd.print("                    ");
    }
    delay(1000);
    lcd.setCursor(0, 2);
    lcd.print("                    ");
    lcd.setCursor(0, 3);
    lcd.print("                    ");
  }
}

void onConnectionEstablished()
{

  client.publish("mesin/status/online/ESP_B", ID_mesin);
  JSONVar TX;
  TX["clientid"] = ID_mesin;
  TX["Wifi"] = WiFi.SSID();
  TX["IP"] = WiFi.localIP().toString();
  String jsonString = JSON.stringify(TX);
  client.publish("mesin/data/log", jsonString.c_str());

  client.subscribe("mesin/status/ping", [](const String &payload)
                   {
    long rssi = WiFi.RSSI();
    long valule = mersure();
    // long valule =  random(300);
     Serial.println(valule);
    JSONVar TX;
    TX["clientid"] = ID_mesin;
    TX["RSSI"] = rssi;
    TX["vaule"] = valule;
    String jsonString = JSON.stringify(TX);
    client.publish("mesin/status/rssi", jsonString.c_str()); });

  // client.subscribe("mesin/status/wifi/" + ID_mesin, [](const String &payload)
  //                  {
  //   JSONVar TX;
  //   TX["clientid"] = ID_mesin;
  //   TX["Wifi"] = WiFi.SSID();
  //   TX["IP"] =  WiFi.localIP().toString();
  //   String jsonString = JSON.stringify(TX);
  //   client.publish("mesin/data/log", jsonString.c_str()); });
  client.subscribe("mesin/RFID/LCD1/" + ID_mesin, [](const String &payload)
                   {
    lcd.setCursor(0, 2);
    lcd.print(payload); });
  client.subscribe("mesin/RFID/LCD2/" + ID_mesin, [](const String &payload)
                   {
    lcd.setCursor(0, 3);
    lcd.print(payload); });
  client.subscribe("mesin/RFID/LCD/" + ID_mesin, [](const String &payload)
                   {
    lcd.setCursor(0, 3);
    lcd.print(payload); });
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
