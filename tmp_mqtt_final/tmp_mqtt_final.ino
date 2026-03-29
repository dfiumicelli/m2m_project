#include <WiFi.h>
#include "mqtt_client.h"

const int TMP_PIN = 34;

const char* ssid = "FM01-C2800";
const char* password = "Pa$$w0rd";

const char* mqtt_uri  = "mqtt://192.168.3.230:1883";
const char* mqtt_user = "admin";
const char* mqtt_pass = "Pallavolo.2";

const char* topic_temp = "casa/stanza/temperatura";

esp_mqtt_client_handle_t mqttClient = NULL;
volatile bool wifiConnected = false;
volatile bool mqttConnected = false;

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000;

unsigned long lastWifiRetry = 0;
const unsigned long wifiRetryInterval = 10000;

float readTMP36C() {
  static bool firstReadDone = false;

  if (!firstReadDone) {
    analogReadMilliVolts(TMP_PIN);   // scarta solo la primissima lettura
    firstReadDone = true;
  }

  uint32_t mv = analogReadMilliVolts(TMP_PIN);
  return (mv - 500.0) / 10.0;
}


void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("WiFi associato");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifiConnected = true;
      Serial.print("WiFi OK, IP: ");
      Serial.println(WiFi.localIP());
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wifiConnected = false;
      mqttConnected = false;
      Serial.println("WiFi disconnesso");
      break;

    default:
      break;
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      Serial.println("MQTT connesso");
      break;

    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      Serial.println("MQTT disconnesso");
      break;

    case MQTT_EVENT_ERROR:
      mqttConnected = false;
      Serial.println("MQTT errore");
      break;

    default:
      break;
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("Avvio connessione WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
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

  if (mqttClient == NULL) {
    Serial.println("Errore init MQTT");
    return;
  }

  esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqttClient);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.onEvent(onWiFiEvent);
  connectWiFi();

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  startMQTT();
}

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiRetry >= wifiRetryInterval) {
      lastWifiRetry = now;
      connectWiFi();
    }
  }

  if (wifiConnected && mqttConnected && now - lastPublish >= publishInterval) {
    lastPublish = now;

    float tempC = readTMP36C();

    char payload[64];
    snprintf(
      payload,
      sizeof(payload),
      "{\"temperature\":%.2f}",
      tempC
    );

    Serial.print("Payload JSON: ");
    Serial.println(payload);

    int msg_id = esp_mqtt_client_publish(mqttClient, topic_temp, payload, 0, 1, 0);

    Serial.print("publish msg_id=");
    Serial.println(msg_id);
  }


  delay(10);
}
