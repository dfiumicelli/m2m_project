#include <WiFi.h>
#include "mqtt_client.h"

const int TMP_PIN = 34;

const char* ssid = "A56 di Daniele";
const char* password = "Pa$$w0rd.19";

const char* mqtt_uri  = "mqtt://192.168.3.230:1883";
const char* mqtt_user = "admin";
const char* mqtt_pass = "Pallavolo.2";

const char* topic_temp = "casa/stanza/temperatura";

IPAddress local_IP(192, 168, 3, 231);
IPAddress gateway(192, 168, 3, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

esp_mqtt_client_handle_t mqttClient = NULL;
volatile bool mqttConnected = false;

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000;

float readTMP36C() {
  static bool firstReadDiscarded = false;

  if (!firstReadDiscarded) {
    analogReadMilliVolts(TMP_PIN);
    firstReadDiscarded = true;
  }

  uint32_t mv = analogReadMilliVolts(TMP_PIN);
  return (mv - 500.0) / 10.0;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      break;

    case MQTT_EVENT_DISCONNECTED:
    case MQTT_EVENT_ERROR:
      mqttConnected = false;
      break;

    default:
      break;
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
}

void startMQTT() {
  if (mqttClient != NULL) return;

  esp_mqtt_client_config_t mqtt_cfg = {};
  mqtt_cfg.broker.address.uri = mqtt_uri;
  mqtt_cfg.credentials.username = mqtt_user;
  mqtt_cfg.credentials.authentication.password = mqtt_pass;
  mqtt_cfg.session.keepalive = 60;
  mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
  mqtt_cfg.network.disable_auto_reconnect = false;

  mqttClient = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqttClient);
}

void setup() {
  analogReadResolution(12);
  analogSetPinAttenuation(TMP_PIN, ADC_2_5db);

  connectWiFi();
  startMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  unsigned long now = millis();

  if (mqttConnected && (now - lastPublish >= publishInterval)) {
    lastPublish = now;

    float tempC = readTMP36C();

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"temperature\":%.2f}", tempC);

    esp_mqtt_client_publish(mqttClient, topic_temp, payload, 0, 1, 0);
  }

  delay(10);
}
