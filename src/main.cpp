/*
  Shelfstrip

  Mega controller of three RGBW strips
  Using a Node CU as pass through WiFi
  MQTT status and control of the strip program to be run
  Not referenced here: ISY controller sends MQTT messages
  ELClient runs on the Node CU
 */

/*
  Includes
*/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>
#include <avr/wdt.h>
#include <stdlib.h>

/*
  Pin definition
*/
#define S1_WHITEPIN 2
#define S1_BLUEPIN 3
#define S1_REDPIN 4
#define S1_GREENPIN 5

#define S2_WHITEPIN 6
#define S2_BLUEPIN 7
#define S2_REDPIN 8
#define S2_GREENPIN 9

#define S3_WHITEPIN 10
#define S3_BLUEPIN 11
#define S3_REDPIN 12
#define S3_GREENPIN 13

#define FADESPEED 50 // make this higher to slow down

void pinsetup() {
  pinMode(S1_REDPIN, OUTPUT);
  pinMode(S1_GREENPIN, OUTPUT);
  pinMode(S1_BLUEPIN, OUTPUT);
  pinMode(S1_WHITEPIN, OUTPUT);

  pinMode(S2_REDPIN, OUTPUT);
  pinMode(S2_GREENPIN, OUTPUT);
  pinMode(S2_BLUEPIN, OUTPUT);
  pinMode(S2_WHITEPIN, OUTPUT);

  pinMode(S3_REDPIN, OUTPUT);
  pinMode(S3_GREENPIN, OUTPUT);
  pinMode(S3_BLUEPIN, OUTPUT);
  pinMode(S3_WHITEPIN, OUTPUT);
}

/*
  MQTT
*/
#define toptopic "sej" // main topic
#define clientId "shelfstrip" // client ID for this unit
#define concat(first, second, third, fourth)                                   \
  first second third fourth // macro for concatenation

const char *topic_control =
    concat(toptopic, "/", clientId, "/control"); // reset position
const char *topic_status =
    concat(toptopic, "/", clientId, "/status"); // program status

/*
  Global vars
*/
boolean state_recd = false;  // start with off
boolean state_state = false; // start with off
int brightness_recd = 255;   // start with full 100% bright
int brightness_state = 255;  // start with full 100% bright
int red_recd = 0;            // red led
int red_state = 0;           // red led
int green_recd = 0;          // green led
int green_state = 0;         // green led
int blue_recd = 0;           // blue led
int blue_state = 0;          // blue led
int white_recd = 0;          // white led
int white_state = 0;         // white led
int program_recd = 0;        // start with off
int program_state = 0;       // start with off

boolean s1 = true; // start with strip 1 enabled
boolean s2 = true; // start with strip 2 enabled
boolean s3 = true; // start with strip 3 enabled

char buf[200]; // spare char buffer
boolean change = false;
boolean inprocess = false;

/*
  Initialize ELClient & MQTT
*/
// Initialize a connection to esp-link using the normal hardware serial port
// both for SLIP and for debug messages.
ELClient esp(&Serial, &Serial);

// Initialize CMD client (for GetTime)
ELClientCmd cmd(&esp);

// Initialize the MQTT client
ELClientMqtt mqtt(&esp);

// Callback made from esp-link to notify of wifi status changes
// Here we just print something out for grins
void wifiCb(void *response) {
  ELClientResponse *res = (ELClientResponse *)response;
  if (res->argc() == 1) {
    uint8_t status;
    res->popArg(&status, 1);

    if (status == STATION_GOT_IP) {
      Serial.println("WIFI CONNECTED");
    } else {
      Serial.print("WIFI NOT READY: ");
      Serial.println(status);
    }
  }
}

bool connected;

// Callback when MQTT is connected
void mqttConnected(void *response) {
  Serial.println("MQTT connected!");
  mqtt.publish(toptopic, concat("connected ", toptopic, clientId, "."), true);
  mqtt.subscribe(topic_control);
  connected = true;
}

// Callback when MQTT is disconnected
void mqttDisconnected(void *response) {
  Serial.println("MQTT disconnected");
  connected = false;
}

// Callback when an MQTT message arrives for one of our subscriptions
void mqttData(void *response) {
  ELClientResponse *res = (ELClientResponse *)response;

  Serial.print("Received: topic=");
  String topic = res->popString();
  Serial.println(topic);

  Serial.print("data=");
  res->popChar(buf);
  Serial.println(buf);

  // topic control
  if (strcmp(topic.c_str(), topic_control) == 0) {
    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, buf);
    if (error) {
      Serial.print(("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      state_recd = (doc["state"] == "100");
      brightness_recd = doc["brightness"];
      red_recd = doc["color"]["r"];
      green_recd = doc["color"]["g"];
      blue_recd = doc["color"]["b"];
      white_recd = doc["color"]["w"];
      program_recd = doc["program"];
      change = true;
      Serial.println(state_recd);
      Serial.println(brightness_recd);
      Serial.println("LED Color");
      Serial.println(red_recd);
      Serial.println(green_recd);
      Serial.println(blue_recd);
      Serial.println(white_recd);
      Serial.println(program_recd);
    }
  }
}

void mqttPublished(void *response) { Serial.println("MQTT published"); }

/*
  build and publish the json state
*/
void jsonBuildPublish() {
  DynamicJsonDocument doc(200);
  if (state_state) {
    doc["state"] = 100;
  } else {
    doc["state"] = 0;
  }
  doc["brightness"] = brightness_state;
  doc["color"]["r"] = red_state;
  doc["color"]["g"] = green_state;
  doc["color"]["b"] = blue_state;
  doc["color"]["w"] = white_state;
  doc["program"] = program_state;

  serializeJson(doc, buf);
  Serial.println(buf);

  mqtt.publish(topic_status, buf);
}

/*
  write the pins to update the strips
*/
void stripUpdate() {
    if(s1) {
      analogWrite(S1_WHITEPIN,
                  ((state_state * ((white_state * brightness_state) / 255))));
      analogWrite(S1_REDPIN,
                  ((state_state * ((red_state * brightness_state) / 255))));
      analogWrite(S1_GREENPIN,
                  ((state_state * ((green_state * brightness_state) / 255))));
      analogWrite(S1_BLUEPIN,
                  ((state_state * ((blue_state * brightness_state) / 255))));
    }
    if(s2) {
      analogWrite(S2_WHITEPIN,
                  ((state_state * ((white_state * brightness_state) / 255))));
      analogWrite(S2_REDPIN,
                  ((state_state * ((red_state * brightness_state) / 255))));
      analogWrite(S2_GREENPIN,
                  ((state_state * ((green_state * brightness_state) / 255))));
      analogWrite(S2_BLUEPIN,
                  ((state_state * ((blue_state * brightness_state) / 255))));
    }
    if(s3) {
      analogWrite(S3_WHITEPIN,
                  ((state_state * ((white_state * brightness_state) / 255))));
      analogWrite(S3_REDPIN,
                  ((state_state * ((red_state * brightness_state) / 255))));
      analogWrite(S3_GREENPIN,
                  ((state_state * ((green_state * brightness_state) / 255))));
      analogWrite(S3_BLUEPIN,
                  ((state_state * ((blue_state * brightness_state) / 255))));
    }
}

/*
  Set-up
*/
void setup() {
  Serial.begin(115200);
  Serial.println("shelfstrip EL-Client starting!");

  // Sync-up with esp-link, this is required at the start of any sketch and
  // initializes the callbacks to the wifi status change callback. The callback
  // gets called with the initial status right after Sync() below completes.
  esp.wifiCb.attach(
      wifiCb); // wifi status change callback, optional (delete if not desired)
  bool ok;
  do {
    ok = esp.Sync(); // sync up with esp-link, blocks for up to 2 seconds
    if (!ok)
      Serial.println("EL-Client sync failed!");
  } while (!ok);
  Serial.println("EL-Client synced!");

  // Set-up callbacks for events and initialize with es-link.
  mqtt.connectedCb.attach(mqttConnected);
  mqtt.disconnectedCb.attach(mqttDisconnected);
  mqtt.publishedCb.attach(mqttPublished);
  mqtt.dataCb.attach(mqttData);
  mqtt.setup();

  // Serial.println("ARDUINO: setup mqtt lwt");
  // mqtt.lwt("/lwt", "offline", 0, 0); //or mqtt.lwt("/lwt", "offline");

  Serial.println("EL-MQTT ready");
}

int count;
static uint32_t last;

void loop() {
  esp.Process();

  if (connected && (millis() - last) > FADESPEED) {
      if (change == true) {
          inprocess = true;
          Serial.println("change");

          if (state_recd) {
              if (program_recd == 0) { //white normal strip
                  program_state = program_recd;
                  red_state = 0;
                  green_state = 0;
                  blue_state = 0;
                  white_state = 255;
                  brightness_state = brightness_recd;
                  s1 = true;
                  s2 = true;
                  s3 = true;
                  stripUpdate();
              } else if (program_recd == 1) { //red green blue color normal strip
                  program_state = program_recd;
                  red_state = red_recd;
                  green_state = green_recd;
                  blue_state = blue_recd;
                  white_state = 0;
                  brightness_state = brightness_recd;
                  s1 = true;
                  s2 = true;
                  s3 = true;
                  stripUpdate();
               } else if (program_recd == 2) { //red green Christmas strip
                  program_state = program_recd;
                  blue_state = 0;
                  white_state = 0;
                  brightness_state = brightness_recd;
                  red_state = 0;
                  green_state = 255;
                  s1 = true;
                  s2 = false;
                  s3 = true;
                  stripUpdate();
                  red_state = 255;
                  green_state = 0;
                  s1 = false;
                  s2 = true;
                  s3 = false;
                  stripUpdate();
              }
          } else if (!state_recd) { // turn strip off
              inprocess = true;
              state_state = state_recd;
              brightness_state = 0;
              red_state = 0;
              green_state = 0;
              blue_state = 0;
              white_state = 0;
              program_state = 0;
              s1 = true;
              s2 = true;
              s3 = true;
              stripUpdate();
          }
          inprocess = false;
    }
    jsonBuildPublish();
    change = false;
  }
  last = millis();
}
