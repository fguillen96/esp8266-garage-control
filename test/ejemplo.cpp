#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <PubSubClient.h>

//enable only one of these below, disabling both is fine too.
#define CHECK_CA_ROOT
// #define CHECK_PUB_KEY
//define CHECK_FINGERPRINT
////--------------------------////


#ifndef SECRET
    const char ssid[] = "Trojan";
    const char pass[] = "ShupalaGamba69";

    #define HOSTNAME "esp8266_mqtt_client1"

    const char MQTT_HOST[] = "185.137.122.40";
    const int MQTT_PORT = 8883;
    const char MQTT_USER[] = "liebre"; // leave blank if no credentials used
    const char MQTT_PASS[] = "magic"; // leave blank if no credentials used

    const char MQTT_SUB_TOPIC[] = "home/" HOSTNAME "/in";
    const char MQTT_PUB_TOPIC[] = "home/" HOSTNAME "/out";

    #ifdef CHECK_CA_ROOT
    static const char digicert[] PROGMEM = R"EOF(
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
    #endif

    #ifdef CHECK_PUB_KEY
    // Extracted by: openssl x509 -pubkey -noout -in ca.crt
    static const char pubkey[] PROGMEM = R"KEY(
    -----BEGIN PUBLIC KEY-----
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxx
    -----END PUBLIC KEY-----
    )KEY";
    #endif

    #ifdef CHECK_FINGERPRINT
	// Extracted by: openssl x509 -fingerprint -in ca.crt
    static const char fp[] PROGMEM = "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD";
    #endif
#endif

//////////////////////////////////////////////////////

#if (defined(CHECK_PUB_KEY) and defined(CHECK_CA_ROOT)) or (defined(CHECK_PUB_KEY) and defined(CHECK_FINGERPRINT)) or (defined(CHECK_FINGERPRINT) and defined(CHECK_CA_ROOT)) or (defined(CHECK_PUB_KEY) and defined(CHECK_CA_ROOT) and defined(CHECK_FINGERPRINT))
  #error "cant have both CHECK_CA_ROOT and CHECK_PUB_KEY enabled"
#endif

BearSSL::WiFiClientSecure net;
PubSubClient client(net);

time_t now2;
unsigned long lastMillis = 0;

void mqtt_connect()
{
  while (!client.connected()) {
    Serial.print("Time: ");
    Serial.print(ctime(&now2));
    Serial.print("MQTT connecting ... ");
    if (client.connect(HOSTNAME, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected.");
      client.subscribe(MQTT_SUB_TOPIC);
    } else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println(". Try again in 5 seconds.");
      /* Wait 5 seconds before retrying */
      delay(5000);
    }
  }
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("connected!");

  Serial.print("Setting time using SNTP");
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now2 = time(nullptr);
  while (now2 < 1510592825) {
    delay(500);
    Serial.print(".");
    now2 = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now2, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  #ifdef CHECK_CA_ROOT
    BearSSL::X509List cert(digicert);
    net.setTrustAnchors(&cert);
  #endif
  #ifdef CHECK_PUB_KEY
    BearSSL::PublicKey key(pubkey);
    net.setKnownKey(&key);
  #endif
  #ifdef CHECK_FINGERPRINT
    net.setFingerprint(fp);
  #endif
  #if (!defined(CHECK_PUB_KEY) and !defined(CHECK_CA_ROOT) and !defined(CHECK_FINGERPRINT))
    net.setInsecure();
  #endif

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(receivedCallback);
  mqtt_connect();
}

void loop()
{
  now2 = time(nullptr);
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Checking wifi");
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      WiFi.begin(ssid, pass);
      Serial.print(".");
      delay(10);
    }
    Serial.println("connected");
  }
  else
  {
    if (!client.connected())
    {
      mqtt_connect();
    }
    else
    {
      client.loop();
    }
  }

  if (millis() - lastMillis > 5000) {
    lastMillis = millis();
    client.publish(MQTT_PUB_TOPIC, ctime(&now2), false);
  }
}