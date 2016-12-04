/*
   Simpleton Sonoff Touch firmware with MQTT support
   Supports OTA update
   David Pye (C) 2016 GNU GPL v3
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define NAME "lswitch0"
#define SSID "SSID"
#define PASS "PASSWORD"

//Defaults to DHCP, if you want a static IP, uncomment and 
//configure below
//#define STATIC_IP
#ifdef STATIC_IP
IPAddress ip(192, 168, 0, 50);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
#endif

#define MQTT_SERVER "192.168.0.1"
#define MQTT_PORT 1883

#define OTA_PASS "UPDATE_PW"
#define OTA_PORT 8266

const char *cmndTopic1 = "cmnd/" NAME "/light";
const char *cmndTopic2 = "cmnd/group/lights";
const char *statusTopic = "status/" NAME "/light";

volatile int desiredRelayState = 0;
volatile int relayState = 0;
volatile unsigned long millisSinceChange = 0;

#define BUTTON_PIN 0
#define RELAY_PIN 12
#define LED_PIN 13

WiFiClient espClient;
PubSubClient client(espClient);

void initWifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
#ifdef STATIC_IP
  WiFi.config(ip, gateway, subnet);
#endif

  WiFi.begin(SSID, PASS);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else Serial.println("WiFi not connected...");
}

void checkMQTTConnection() {
  Serial.print("Establishing MQTT connection: ");
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    if (client.connect(NAME)) {
      Serial.println("connected");
      client.subscribe(cmndTopic1);
      client.subscribe(cmndTopic2);
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
  }
  if (client.connected()) {
    //LED to on.
    digitalWrite(LED_PIN, LOW);
    return;
  }
  else {
    digitalWrite(LED_PIN, HIGH);
  }
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  if (!strcmp(topic, cmndTopic1) || !strcmp(topic, cmndTopic2)) {
    if ((char)payload[0] == '1' || ! strcasecmp((char *)payload, "on")) {
        desiredRelayState = 1;
    }
    else if ((char)payload[0] == '0' || ! strcasecmp((char *)payload, "off")) {
      desiredRelayState = 0;
    }
    else if ( ! strcasecmp((char *)payload, "toggle")) {
      desiredRelayState = !desiredRelayState;
    }
  }
}

void shortPress() {
  desiredRelayState = !desiredRelayState; //Toggle relay state.
}

void longPress() {
  desiredRelayState = !desiredRelayState; //Toggle relay state.
}

void buttonChangeCallback() {
  if (digitalRead(0) == 1) {
    //Button has been released, trigger one of the two possible options.
    if (millis() - millisSinceChange > 500) {
      longPress();
    }
    else if (millis() - millisSinceChange > 100){
      //Short press
      shortPress();
    }
    else {
      //Too short to register as a press
    }
  }
  else {
    //Just been pressed - do nothing until released.
  }
  millisSinceChange = millis();
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH); //LED off.

  Serial.begin(115200);
  Serial.println("Init");
  initWifi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(MQTTcallback);

  //OTA setup
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(NAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();
  //Enable interrupt for button press

  Serial.println("Enabling switch interrupt");
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonChangeCallback, CHANGE);

  //Connect to MQTT server.
  checkMQTTConnection();
  lastMQTTCheck = millis();
}

unsigned long lastMQTTCheck = 0;
void loop() {

  if (millis() - lastMQTTCheck >= 5000) {
    checkMQTTConnection();
    lastMQTTCheck = millis();
  }

  client.loop(); //Process MQTT client events

  //Handler for over-the-air SW updates.
  ArduinoOTA.handle();

  //Relay state is updated via the interrupt above *OR* the MQTT callback.
  if (relayState != desiredRelayState) {
      digitalWrite(RELAY_PIN, desiredRelayState);
      relayState = desiredRelayState;
      client.publish(statusTopic, relayState == 0 ? "0" : "1");
  }
  delay(50);
}
