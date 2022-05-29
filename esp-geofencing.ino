#include <PubSubClient.h>
#include <WiFi.h>

const char* ssid = "gldnpz";
const char* password = "sapigemuk55555";

const char* MQTT_SERVER = "192.168.251.3";
String CLIENT_ID = "ESP32Client";

#define TOPIC "geofencing/location"

WiFiServer server(80);
WiFiClient mqtt_wifi_client;
PubSubClient pubsub(mqtt_wifi_client);

const long TIMEOUT = 2000;

String get_body(String http_message) {
  int body_start = http_message.indexOf("\r\n\r\n");
  if (body_start == -1) {
    return "";
  }
  return http_message.substring(body_start + 4);
}

TaskHandle_t maintain_connection_task;
void maintain_connection(void *pvParameters) {
  while(true) {
    pubsub.loop();

    vTaskDelay(3000);
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_MODE_STA);

  while (Serial == false) { }
  delay(2000);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  pubsub.setServer(MQTT_SERVER, 1883);

  xTaskCreatePinnedToCore(
    maintain_connection,
    "MaintainConnection",
    10000,
    NULL,
    tskIDLE_PRIORITY,
    &maintain_connection_task,
    1
  );
}

void mqttConnect(WiFiClient& client) {
  while (!pubsub.connected()) {
    if (!pubsub.connect(CLIENT_ID.c_str())) {
      Serial.printf("MQTT connection failed. Code: %d.\n", pubsub.state());
      delay(5000);
    } else {
      Serial.println("MQTT connected.");
    }
  }
}

void loop() {
  WiFiClient client = server.available();
  if (!pubsub.connected()) {
    mqttConnect(client);
  }
  /*if (!pubsub.loop()) {
    pubsub.connect(CLIENT_ID.c_str());
  }*/

  if (client) {
    long current_time = millis();
    long previous_time = current_time;
    Serial.println("New client.");
    String http_message = client.readString();

    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();

    if (http_message.indexOf("GET") != -1) {
      Serial.println("Sending page...");
      client.println(
R"(<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      html, body { height: 100%; }
      body { display: flex; align-items: center; justify-content: center; } 
    </style>
  </head>
  <body>
    <span id="location-info">Location: N/A</span>
    <script>
      const locationElement = document.getElementById("location-info");

      if (navigator.geolocation) {
        navigator.geolocation.watchPosition(
          async ({ coords: { latitude, longitude } }) => {
            locationElement.innerHTML = `Location: ${latitude}, ${longitude}`
            await fetch("/", {
              method: "POST",
              body: `${latitude},${longitude}`
            })
          },
          ({ message }) => {
            locationElement.innerHTML = `Error: ${message}`
          }
        )
      } else {
        locationElement.innerHTML = "Error: Geolocation is not supported by this browser"
      }
    </script>
  </body>
</html>)"
      );
      client.println();
    } else if (http_message.indexOf("POST") != -1) {
      Serial.println("Location data received.");

      String body = get_body(http_message);
      double latitude = body.substring(0, body.indexOf(",")).toDouble();
      double longitude = body.substring(body.indexOf(",") + 1).toDouble();

      Serial.printf("Latitude: %f, Longitude: %f\n", latitude, longitude);
      
      Serial.printf("MQTT connection: %s\n", pubsub.connected() ? "CONNECTED" : "DISCONNECTED");
      char value[] = "TES";
      bool publish_status = pubsub.publish(TOPIC, value);
      Serial.printf("Publish status: %s\n", publish_status ? "SUCCESS" : "FAILURE");
    }

    client.stop();
    Serial.println("Client disonnected.");
  }
}