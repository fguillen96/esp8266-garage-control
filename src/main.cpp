// *******************************************************************************************************
// DESCRIPTION: TODO
// AUTOR: Fran Guillén
// BOARD: Wemos D1 mini
// *****************************************************************************************************

#define DEBUGGING

#ifdef DEBUGGING
  #define DEBUG(x)       Serial.print (x)
  #define DEBUGLN(x)     Serial.println (x)
#else
  #define DEBUG(x)
  #define DEBUGLN(x) 
#endif


#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>   // MQTT library
#include <ArduinoJson.h>    // JSON library
#include <time.h>


// ---------- CERTIFICATE ---------
const char caCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDtzCCAp+gAwIBAgIUGbIhBoSk1tJJJXA93lIJjmwOWScwDQYJKoZIhvcNAQEL
BQAwazELMAkGA1UEBhMCRVMxDzANBgNVBAgMBk1hbGFnYTEPMA0GA1UECgwGTE1U
ZWNoMRcwFQYDVQQDDA5zZWxmY2VydC5sb2NhbDEhMB8GCSqGSIb3DQEJARYSc3Jz
d29yZWJAZ21haWwuY29tMB4XDTIxMTAxNDIwMTkwNFoXDTMxMTAxMjIwMTkwNFow
azELMAkGA1UEBhMCRVMxDzANBgNVBAgMBk1hbGFnYTEPMA0GA1UECgwGTE1UZWNo
MRcwFQYDVQQDDA5zZWxmY2VydC5sb2NhbDEhMB8GCSqGSIb3DQEJARYSc3Jzd29y
ZWJAZ21haWwuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtDSl
YvHiCYK3pUTJKHutBvaMTWpMFnWpLWcjVRTdM5mKp7HOaIcsDDud8k6RWFfZZxA8
UnU6b2HTJw3g2kmw1f9YRDMVf1uHIBoHl4fwjEzEs65AzMughn8SP3e+NyDXSTHo
60kYoTrZmny+I3rlyJU9QQn2eG0/v7pOzq3qMu/xzWkfMMsPKCzKdBam+RoqgRKE
o9pXvwXB8oDmQCOfmSYS+FTqR0KcaTfQ6F99HrOcxV2xhMr7w8WWNniG4lSAg78Z
W0KekZUutUOXZYVjKZGvCIMky08iTgAqGJL9Or2k7jFPMiMp2LMP0SRqA4vdDKrl
9HR3XoE8NbH5/RZxCwIDAQABo1MwUTAdBgNVHQ4EFgQUniypLIL+OwUxncPzJvFT
oTyjIcAwHwYDVR0jBBgwFoAUniypLIL+OwUxncPzJvFToTyjIcAwDwYDVR0TAQH/
BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAZMFTlWNnAun7YOlCSKJ6MO2C/FqV
cqHa9COiN7t3OYYuNIE24eEw0UPTgCj9GyTEGJV4BwKtJx2DgegWOKB7gi7XrbQo
Nna4z1PQ4m0vhPdt17qTLiwmYlU94Uqu2fj8eMNLGX8YNuclJvcNEmBFSMTuQ6Tj
Vd80Ds0diILs2E8SJjzTqtY/H/1BDogakZtCetcDqcARFnOwqFsWtByxpiHV62dv
I6skFmTmoI4IRQlywVbZgMYK1bkPxipiAfJjP4SCMILX8QNKQe9Bgel58okp8A/k
n2gwdZPDUNOzCG6eM3Gnd0rdAwlWfBPi7J0P61uc++cmbDQcUbOCx202ZQ==
-----END CERTIFICATE-----
)EOF";

const char* fingerprint = "2a:9a:ba:d2:76:74:61:e5:39:39:b3:bb:80:2f:b5:07:99:5f:f3:37";//"31:e0:3b:d4:f2:63:c3:00:be:21:a4:d2:4b:c3:ab:41:69:11:75:99";


// ---------- DEFAULT SYSTEM CONFIGURATION ---------
#define RELAY_TIMEOUT   200
#define SENSOR_READ_INTERVAL  500

// ---------- DIGITAL INPUTS ----------
#define DI_SENSOR_DOOR         4
#define DI_SENSOR_SECURITY     13


// ---------- DIGITAL OUTPUTS ----------
#define DO_PP_RELAY    5

// ---------- WIFI AND MQTT CONNECTION ----------
#define WIFI_SSID       "Trojan"
#define WIFI_PASSWORD   "ShupalaGamba69"
#define MQTT_SERVER     "185.137.122.40"                 // MQTT server IP
#define MQTT_PORT       8883                             // MQTT port
#define MQTT_USERNAME   "liebre"                         // MQTT server username
#define MQTT_PASSWORD   "magic"                          // MQTT server password
#define CLIENT_ID       "Piter_garage"                   // MQTT client ID

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
unsigned long GetEpochTime();
void UpdateDoorStatus(bool);


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
      digitalWrite(DO_PP_RELAY, HIGH);
      relay_on = true;
      start_time = millis();
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
  pinMode(DI_SENSOR_DOOR, INPUT_PULLUP);
  pinMode(DI_SENSOR_SECURITY, INPUT_PULLUP);
  pinMode(DO_PP_RELAY, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(DI_SENSOR_SECURITY), SecuritySwitch, FALLING);

  // --------- MANAGING CERTS ---------
  espClient.setTrustAnchors(&caCertX509);
  espClient.allowSelfSignedCerts();
  espClient.setFingerprint(fingerprint);
  //espClient.setInsecure();

  // --------- WIFI MANAGER CONFIG --------
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.autoConnect("LMTech");
  //ConnectWifi();
  
  // ---------- MQTT CONNECTION --------
  // Connection is done in loop (non blocking)
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(OnReceiveMQTT);

  // --------- NTP SERVER CONFIG ---------
  configTime(0, 0, "pool.ntp.org");
  GetEpochTime();
}

// ********************************************************************
//                           LOOP
// ********************************************************************
void loop() {
  // ---------- MANAGE MQTT CONNECTION (NON BLOCKING) ----------
  static unsigned long lastReconnectAttempt = 0;
  if (!mqttClient.connected()) {
    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      if (ConnectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  }

  // --------- SECURITY INTERRUPT SWITCH ----------
  // TODO
  if (interrupt_flag) {
    interrupt_flag = false;
    relay_on = true;
    start_time = millis();
  }

  // --------- RELAY TIMEOUT ---------
  if (relay_on && (millis() - start_time >= RELAY_TIMEOUT)) {
    relay_on = false;
    digitalWrite(DO_PP_RELAY, LOW);
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

  // ---------- CONNECTION PROCESS ----------
  mqttClient.loop();
  wifiManager.process();
}



// ********************************************************************
//                      LOCAL FUNCTIONS
// ********************************************************************
bool ConnectMQTT() {
  String clientId = CLIENT_ID;

  // Attempt to connect
  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, TOPIC_WILL, 1, true, "Disconnected")) {
    mqttClient.publish(TOPIC_WILL, "Connected", true);
    DEBUGLN("Connected to MQTT server");
    mqttClient.subscribe(TOPIC_SUB_CONFIG, 1);
    mqttClient.subscribe(TOPIC_SUB_CONTROL, 0); 
    }

  return mqttClient.connected();
}

// --------- GETTING TIME ---------
unsigned long GetEpochTime() {
  time_t current_time =  now();
  time(&current_time);
  return current_time;
}

// --------- UPDATE DOOR STATUS AND SEND MQTT SERVER --------
void UpdateDoorStatus(bool open) {
  const int capacity = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<capacity> doc;
  char buffer[80];

  if (open) doc["status"] = "open";
  else doc["status"] = "closed";

  doc["timestamp"] = GetEpochTime();

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