#define ARDUINOJSON_USE_LONG_LONG 1
#define FASTLED_ESP8266_DMA

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <stdlib.h>
#include "config.h"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

#define RING_LEDS 16
#define RINGS     4
#define NUM_LEDS (RING_LEDS*RINGS)
#define DATA_PIN 9

// Define the array of leds
CRGB leds[NUM_LEDS];
byte state = 0x2;
byte master_state = 0x1; //Off=0, Pattern=1, Color=2
#include "LED_control.h"

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
#include "MFRC522_func.h"

MFRC522::MIFARE_Key key;
//MFRC522::StatusCode status;
byte sector         = 1;
byte blockAddr      = 4;
byte dataBlock[]    = {
        0x01, 0x02, 0x03, 0x04, //  1,  2,   3,  4,
        0x05, 0x06, 0x07, 0x08, //  5,  6,   7,  8,
        0x09, 0x0a, 0xff, 0x0b, //  9, 10, 255, 11,
        0x0c, 0x0d, 0x0e, 0x0f  // 12, 13, 14, 15
    };
byte trailerBlock   = 7;
byte buffer[18];
byte size = sizeof(buffer);
bool read_success, write_success, auth_success;
byte PICC_version;
unsigned int readCard[4];

bool send_chip_data = true;


void connectToMqtt() {
  Serial.println("[MQTT] Connecting to MQTT...");
  mqttClient.setClientId(WIFI_CLIENT_ID);
  mqttClient.setKeepAlive(5);
  mqttClient.setWill(MQTT_TOPIC_MONITOR,MQTT_TOPIC_MONITOR_QoS,true,"{\"alive\": false}");
  mqttClient.connect();
}

void connectToWifi() {
  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);

  WiFi.hostname(WIFI_CLIENT_ID);
  WiFi.mode(WIFI_STA);
  if (WIFI_STATIC_IP != 0) {
    WiFi.config(WIFI_CLIENT_IP, WIFI_GATEWAY_IP, WIFI_SUBNET_IP, WIFI_DNS_IP);
  }

  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  delay(10000);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("[WiFi] timeout, disconnecting WiFi");
    ESP.restart();
  }
  else {
    Serial.println(WiFi.localIP());
    wifiReconnectTimer.detach();
    connectToMqtt();
  }
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.print("[WiFi] Connected, IP address: ");
  Serial.println(WiFi.localIP());
  wifiReconnectTimer.detach();
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("[WiFi] Disconnected from Wi-Fi!");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(WIFI_RECONNECT_TIME, connectToWifi);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("[MQTT] Connected to MQTT!");

  mqttReconnectTimer.detach();

  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  uint16_t packetIdSub = mqttClient.subscribe(MQTT_TOPIC_LEDS, MQTT_TOPIC_LEDS_QoS);
  Serial.print("Subscribing to ");
  Serial.println(MQTT_TOPIC_LEDS);
  Serial.println("Sending alive message");
  mqttClient.publish(MQTT_TOPIC_MONITOR, MQTT_TOPIC_MONITOR_QoS, true, "{\"alive\": true}");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("[MQTT] Disconnected from MQTT!");

  if (WiFi.isConnected()) {
    Serial.println("[MQTT] Trying to reconnect...");
    mqttReconnectTimer.once(MQTT_RECONNECT_TIME, connectToMqtt);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (!strcmp(topic, MQTT_TOPIC_LEDS)) {
    Serial.print("Leds message: ");
    Serial.print(len);
    Serial.print(" [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < len; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();

    DynamicJsonDocument doc(50);
    deserializeJson(doc, payload);
    state = doc["state"];
    master_state = doc["master_state"];
    Serial.print("state: ");
    Serial.println(state);
    Serial.print("master_state: ");
    Serial.println(master_state);
    FastLED.clear();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Startup!");
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card
  mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
  // Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
  // Prepare the key (used both as key A and as key B)
  // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(64);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  if (MQTT_USER != "") {
    mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  }

  connectToWifi();
}

/**
 * Main loop.
 */
void loop() {
  set_leds(state, master_state);
  FastLED.show();
  FastLED.delay(20);

  // read the RFID reader version to send over the heartbeat as data
  PICC_version = 0;
  PICC_version = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);

  // START RFID HANDLING
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return;

  // Dump debug info about the card; PICC_HaltA() is automatically called
  // mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

  // get card uid
  Serial.print("found tag ID: ");
  for (int i = 0; i < mfrc522.uid.size; i++) {  // for size of uid.size write uid.uidByte to readCard
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println();

  // get PICC card type
  // Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  // Serial.println(mfrc522.PICC_GetTypeName(piccType));

  // Check for compatibility
  if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
      &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
      &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
      Serial.println(F("Not a MIFARE Classic card."));
      return;
  }

  // perform authentication to open communication
  auth_success = authenticate(trailerBlock, key);
  if (!auth_success) {
    //Serial.println(F("Authentication failed"));
    return;
  }

  // read the tag to get coded information
  read_success = read_block(blockAddr, buffer, size);
  if (!read_success) {
    //Serial.println(F("Initial read failed, closing connection"));
    // Halt PICC
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();
    return;
  }

  //char chip_data[22] = "Null for now";
  if (mqttClient.connected() && send_chip_data) {
    StaticJsonDocument<128> chip_data;
    //JsonArray UID = chip_data.to<JsonArray>();
    //JsonArray block_data = chip_data.to<JsonArray>();
    String UID = String(readCard[0],HEX) + String(readCard[1],HEX) + String(readCard[2],HEX) + String(readCard[3],HEX);
    //copyArray(readCard, UID);
    //copyArray(buffer, block_data);
    chip_data["UID"] = UID;
    chip_data["color"] = buffer[1] & 0x0F; // remove valid and win bits from color byte
    chip_data["old_chip"] = (buffer[0] != 0xFF); // first byte not 0xFF means old chip
    char chip_data_buffer[100];
    serializeJson(chip_data, chip_data_buffer);
    // send_chip_data = false;
    mqttClient.publish(MQTT_TOPIC_CHIP, MQTT_TOPIC_CHIP_QoS, false, chip_data_buffer);
    Serial.print("Sending chip data: ");
    serializeJson(chip_data, Serial);
  }

  // Dump the sector data, good for debug
  // Serial.println(F("Current data in sector:"));
  // mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
  // Serial.println();

  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();
}
