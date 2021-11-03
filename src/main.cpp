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
#define CLIENT_ID       "Piter_garage"                 // MQTT client ID

// ---------- MQTT TOPICS ----------
#define TOPIC_PUB_DOOR      "garage/door"
#define TOPIC_SUB_CONFIG    "garage/config"
#define TOPIC_SUB_CONTROL   "garage/control"
#define TOPIC_WILL          "garage/status"


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


// ********************************************************************
//                     FUNCTION PROTOTYPES
// ********************************************************************
void ConnectWifi();
bool ConnectMQTT();
unsigned long UnixTime();
void UpdateDoorStatus(bool);
void RelayControl(bool);


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
  if (strcmp(topic, TOPIC_SUB_CONTROL) == 0) {
    if (payload[0] == '1' or payload[0] == 't') {
      DEBUGLN("PP");
      RelayControl(true);
    }
  }

  else if (strcmp(topic, TOPIC_SUB_CONFIG) == 0) {

    // --------- MQTT VARIABLES ----------
    StaticJsonDocument<100> doc;
    deserializeJson(doc, payload, length);
    JsonObject obj = doc.as<JsonObject>();

    // ---------- CHANGING SAMPLE TIME ---------
    if (obj.containsKey("sample_time")) {
      DEBUGLN("Sample time changed");
    }
  }
}



// ********************************************************************
//                      INTERRUPTS
// ********************************************************************
IRAM_ATTR void SecuritySwitch() {
  if (!interrupt_flag) {
    digitalWrite(DO_PP_RELAY, HIGH);
    interrupt_flag = true;
  }
}



// ********************************************************************
//                     BOARD SETUP
// ********************************************************************
void setup() {
  // --------- DEBUG ---------
  #ifdef DEBUGGING
    Serial.begin(9600);
  #endif

  // ---------- PIN CONFIG ----------
  pinMode(DO_LED, OUTPUT);
  digitalWrite(DO_LED, HIGH);
  pinMode(DI_SENSOR_DOOR, INPUT_PULLUP);
  //pinMode(DI_SENSOR_SECURITY, INPUT_PULLUP);
  pinMode(DO_PP_RELAY, OUTPUT);
  //attachInterrupt(digitalPinToInterrupt(DI_SENSOR_SECURITY), SecuritySwitch, FALLING);


  // --------- WIFI MANAGER CONFIG --------
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("LMTech");
  //ConnectWifi();

  // --------- NTP SERVER CONFIG ---------
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // --------- MANAGING CERTS ---------
  //espClient.setTrustAnchors(&caCertX509);
  //espClient.setX509Time(now());
  //espClient.allowSelfSignedCerts();
  //espClient.setFingerprint(fingerprint);
  //espClient.setInsecure();
  
  // ---------- MQTT CONNECTION --------
  // Connection is done in loop (non blocking)
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(OnReceiveMQTT);

}

// ********************************************************************
//                           LOOP
// ********************************************************************
void loop() {
  // --------- SECURITY INTERRUPT SWITCH ----------
  // TODO
  if (interrupt_flag) {
    interrupt_flag = false;
    RelayControl(true);
  }

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


bool ConnectMQTT() {
  String clientId = CLIENT_ID;
  // Certificates to connect to server
  espClient.setTrustAnchors(&caCertX509);

  // Attempt to connect
  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, TOPIC_WILL, 1, true, "Disconnected")) {
    mqttClient.publish(TOPIC_WILL, "Connected", true);
    mqttClient.subscribe(TOPIC_SUB_CONFIG, 1);
    mqttClient.subscribe(TOPIC_SUB_CONTROL, 0); 
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
  mqttClient.publish(TOPIC_PUB_DOOR, buffer, true); 
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