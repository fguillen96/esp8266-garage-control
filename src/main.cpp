// *******************************************************************************************************
// DESCRIPTION: Garage control program v1.0
// AUTOR: Fran Guillén
// BOARD: Wemos D1 mini
// *****************************************************************************************************

#define DEBUGGING

#include <debug.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>         // MQTT library
#include <ArduinoJson.h>          // JSON library
#include <time.h>

// ---------- SENSITIVE INFORMATION --------
#include <secrets.h>
#include <CertificateFile.h>



// ---------- DEFAULT SYSTEM CONFIGURATION ---------
#define RELAY_TIMEOUT         200
#define SENSOR_READ_INTERVAL  500
ADC_MODE(ADC_VCC);

// ---------- DIGITAL INPUTS ----------
#define DI_SENSOR_DOOR         4
#define DI_SENSOR_SECURITY     13


// ---------- DIGITAL OUTPUTS ----------
#define DO_PP_RELAY    5
#define DO_LED         2

// ---------- WIFI AND MQTT CONNECTION (secrets.h) ----------
#define WIFI_SSID       SECRET_WIFI_SSID               // WIFI SSID
#define WIFI_PASSWORD   SECRET_WIFI_PASSWORD           // WIFI password
#define MQTT_SERVER     SECRET_MQTT_SERVER             // MQTT server IP
#define MQTT_PORT       SECRET_MQTT_PORT               // MQTT port
#define MQTT_USERNAME   SECRET_MQTT_USERNAME           // MQTT server username
#define MQTT_PASSWORD   SECRET_MQTT_PASSWORD           // MQTT server password

// ---------- MQTT TOPICS ----------
#define TOPIC_PUB_DOOR_INFO      "/garage/door/status"
#define TOPIC_SUB_DOOR_CTRL      "/garage/door/control"
#define TOPIC_SUB_CONFIG         "/config"
#define TOPIC_PUB_DEV_STAT       "/info"

// --------- WILL MESSAGE ---------
#define WILL_MESSAGE  R"EOF("{"status": "disconnected"}")EOF"


// ********************************************************************
//                     FUNCTION PROTOTYPES
// ********************************************************************
void SendDeviceStatus();
char* GetDeviceId();
char* GetDeviceTopic(const char*, const char*);
void ConnectWifi();
bool ConnectMQTT();
unsigned long UnixTime();
void UpdateDoorStatus(bool);
void RelayControl(bool);


// ********************************************************************
//                     GLOBAL VARIABLES
// ********************************************************************
// ---------- WIFI AND MQTT CLIENT ----------
X509List caCertX509(caCert);
WiFiManager wifiManager;
WiFiClientSecure  espClient;
PubSubClient mqttClient(espClient);

// --------- INTERRUPT FLAG --------
volatile bool interrupt_flag = false;

// ---------- TIME CONTROL RELAY VARIABLES ---------
bool relay_on = false;
unsigned long start_time = 0;

// ---------- CHIP INFO ---------
const char* device_id = GetDeviceId();

// ---------- CHIP TOPICS STRINGS ----------
const char* sub_topic_config = GetDeviceTopic(device_id, TOPIC_SUB_CONFIG);
const char* sub_topic_door_ctrl = GetDeviceTopic(device_id, TOPIC_SUB_DOOR_CTRL);
const char* pub_topic_door_stat = GetDeviceTopic(device_id, TOPIC_PUB_DOOR_INFO);
const char* pub_topic_dev_stat =  GetDeviceTopic(device_id, TOPIC_PUB_DEV_STAT);


// ********************************************************************
//                      CALLBACKS
// ********************************************************************
void OnReceiveMQTT(char *topic, byte *payload, unsigned int length) {
  DEBUG("Message arrived [");
  DEBUG(topic);
  DEBUG("] ");
  for (unsigned int i = 0; i < length; i++) {
    DEBUG((char)payload[i]);
  }
  DEBUGLN();

  // ---------- FILTERING BY TOPIC ----------
  if (strcmp(topic, sub_topic_door_ctrl) == 0) {
    if (payload[0] == '1' or payload[0] == 't') {
      DEBUGLN("PP");
      RelayControl(true);
    }
  }

  else if (strcmp(topic, sub_topic_config) == 0) {
    // --------- MQTT VARIABLES ----------
    StaticJsonDocument<100> doc;
    deserializeJson(doc, payload, length);
    JsonObject obj = doc.as<JsonObject>();

    // ---------- CHANGING SAMPLE TIME ---------
    if (obj.containsKey("deviceStatus")) {
      SendDeviceStatus(); 
    }
  }
}


// ********************************************************************
//                     BOARD SETUP
// ********************************************************************
void setup() {
  
  // --------- DEBUG ---------
  wifiManager.setDebugOutput(false);
  #ifdef DEBUGGING
    Serial.begin(9600);
    wifiManager.setDebugOutput(true);
  #endif


  // ---------- PIN CONFIG ----------
  pinMode(DO_LED, OUTPUT);
  digitalWrite(DO_LED, HIGH);
  pinMode(DI_SENSOR_DOOR, INPUT_PULLUP);
  pinMode(DO_PP_RELAY, OUTPUT);


  // --------- WIFI MANAGER CONFIG --------
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("LMTech");
  //ConnectWifi();

  // --------- NTP SERVER CONFIG ---------
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  
  // ---------- MQTT CONNECTION --------
  // Connection is done in loop (non blocking)
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(OnReceiveMQTT);

}

// ********************************************************************
//                           LOOP
// ********************************************************************
void loop() {
  // --------- RELAY TIMEOUT ---------
  if (relay_on && (millis() - start_time >= RELAY_TIMEOUT)) {
    RelayControl(false);
  }


  // ---------- READING DIGITAL INPUT SENSOR ---------
  static bool door_open = true;
  static bool prev_door_open = false;
  static unsigned long prev_millis_lecture = 0;
  if(millis() - prev_millis_lecture >= SENSOR_READ_INTERVAL) {
    prev_millis_lecture = millis();
    door_open = digitalRead(DI_SENSOR_DOOR);
  }

  // --------- IF STATUS CHANGED, UPDATE ----------
  if (door_open != prev_door_open) {
    prev_door_open = door_open;
    DEBUG("Door open: ");
    DEBUGLN(door_open);
    UpdateDoorStatus(door_open);
  }


  // ---------- MANAGE MQTT CONNECTION (NON BLOCKING) ----------
  static unsigned long lastReconnectAttempt = 0;
  if (!mqttClient.connected() && time(nullptr) > 1635448489) {
    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      if (ConnectMQTT()) {
        DEBUGLN(" MQTT connected");
        lastReconnectAttempt = 0;
        UpdateDoorStatus(door_open);
      }
      else {
        DEBUGLN("MQTT connection error");
      }
    }
  }


  // ---------- CONNECTION PROCESS ----------
  mqttClient.loop();
  wifiManager.process();
}



// ********************************************************************
//                      LOCAL FUNCTIONS
// ********************************************************************
char* GetDeviceId() {
  uint8_t mac[6];
  wifi_get_macaddr(STATION_IF, mac);
  char *string = (char*)malloc(13);
  sprintf(string, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return string;
}


char* GetDeviceTopic(const char* id, const char* topic) {
  char *string = (char*)malloc(strlen(id) + strlen(topic) + 1);
  strcpy(string, device_id);
  strcat(string, topic);
  return string;
}

void RelayControl(bool activate) {
  if (activate) {
    digitalWrite(DO_PP_RELAY, HIGH);
    digitalWrite(DO_LED, LOW);
    relay_on = true;
    start_time = millis();
  }
  else{
    digitalWrite(DO_PP_RELAY, LOW);
    digitalWrite(DO_LED, HIGH);
    relay_on = false;
  }
}


void SendDeviceStatus() {
  const int capacity = JSON_OBJECT_SIZE(7);
  StaticJsonDocument<capacity> doc;
  char buffer[200];
  doc["status"] = "connected";
  doc["id(mac)"] = device_id;
  doc["heapFragmentation"] = ESP.getHeapFragmentation();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["vcc"] = ESP.getVcc();
  doc["IP"] = WiFi.localIP();
  doc["BSSID"] = WiFi.BSSID();

  serializeJsonPretty(doc, buffer);
  mqttClient.publish(pub_topic_dev_stat, buffer, true);
}
  


bool ConnectMQTT() {
  // Certificates to connect to server
  espClient.setTrustAnchors(&caCertX509);

  // Attempt to connect
  if (mqttClient.connect(device_id, MQTT_USERNAME, MQTT_PASSWORD, pub_topic_dev_stat, 1, true, WILL_MESSAGE)) {
    SendDeviceStatus();
    mqttClient.subscribe(sub_topic_config, 1);
    mqttClient.subscribe(sub_topic_door_ctrl, 0);
    }

  return mqttClient.connected();
}


// --------- UPDATE DOOR STATUS AND SEND MQTT SERVER --------
void UpdateDoorStatus(bool open) {
  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  char buffer[80];

  if (open) doc["status"] = "open";
  else doc["status"] = "closed";

  doc["ts"] = (int)time(nullptr);

  serializeJsonPretty(doc, buffer);
  mqttClient.publish(pub_topic_door_stat, buffer, true); 
}

/* Not using this function. Wifi is managed by wifimanager library

void ConnectWifi() {
  DEBUGLN("Connecting to ");
  DEBUG(WIFI_SSID);
  DEBUG(" ...");

  WiFi.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG(".");
  }

  DEBUGLN();
  DEBUGLN("WiFi connected");

  DEBUG("IP address: ");
  DEBUGLN(WiFi.localIP());
}

*/