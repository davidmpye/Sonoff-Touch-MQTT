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

#define BUTTON_PIN 0
#define RELAY_PIN 12
#define LED_PIN 13

volatile int desiredRelayState = 0;
volatile int relayState = 0;
volatile unsigned long millisSinceChange = 0;

unsigned long lastMQTTCheck = -5000; //This will force an immediate check on init.

WiFiClient espClient;
PubSubClient client(espClient);
bool printedWifiToSerial = false;

void initWifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
#ifdef STATIC_IP
  WiFi.config(ip, gateway, subnet);
#endif

  WiFi.begin(SSID, PASS);
}

void checkMQTTConnection() {
  Serial.print("Checking MQTT connection: ");
  if (client.connected()) Serial.println("OK");
  else {
    if (WiFi.status() == WL_CONNECTED) {
      //Wifi connected, attempt to connect to server
      Serial.print("new connection: ");
      if (client.connect(NAME)) {
        Serial.println("connected");
        client.subscribe(cmndTopic1);
        client.subscribe(cmndTopic2);
      } else {
        Serial.print("failed, rc=");
        Serial.println(client.state());
      }
    }
    else {
      //Wifi isn't connected, so no point in trying now.
      Serial.println(" Not connected to WiFI AP, abandoned connect.");
    }
  }
  //Set the status LED to ON if we are connected to the MQTT server
  if (client.connected()) digitalWrite(LED_PIN, LOW);
    else digitalWrite(LED_PIN, HIGH);
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  if (!strcmp(topic, cmndTopic1) || !strcmp(topic, cmndTopic2)) {
    if ((char)payload[0] == '1' || ! strncasecmp_P((char *)payload, "on", length)) {
        desiredRelayState = 1;
    }
    else if ((char)payload[0] == '0' || ! strncasecmp_P((char *)payload, "off", length)) {
      desiredRelayState = 0;
    }
    else if ( ! strncasecmp_P((char *)payload, "toggle", length)) {
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
  Serial.begin(115200);
  Serial.println("Initialising");
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH); //LED off.

  Serial.begin(115200);
  initWifi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(MQTTcallback);

  //OTA setup
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(NAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();
  //Enable interrupt for button press

  Serial.println("Enabling touch switch interrupt");
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonChangeCallback, CHANGE);
}

void loop() {
  //If we haven't printed WiFi details to Serial port yet, and WiFi now connected,
  //do so now. (just the once)
  if (!printedWifiToSerial && WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    printedWifiToSerial = true;
  }

  if (millis() - lastMQTTCheck >= 5000) {
    checkMQTTConnection();
    lastMQTTCheck = millis();
  }

  //Handle any pending MQTT messages
  client.loop();

  //Handle any pending OTA SW updates
  ArduinoOTA.handle();

  //Relay state is updated via the interrupt *OR* the MQTT callback.
  if (relayState != desiredRelayState) {
      Serial.print("Changing state to ");
      Serial.println(desiredRelayState);

      digitalWrite(RELAY_PIN, desiredRelayState);
      relayState = desiredRelayState;

      Serial.print("Sending MQTT status update ");
      Serial.print(relayState);
      Serial.print(" to ");
      Serial.println(statusTopic);

      client.publish(statusTopic, relayState == 0 ? "0" : "1");
  }
  delay(50);
}
