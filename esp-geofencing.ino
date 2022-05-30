#include "PubSubClient.h"
#include <WiFi.h>

const char* ssid = "gldnpz";
const char* password = "sapigemuk55555";

const char* MQTT_SERVER = "104.209.180.133";
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
  if (!pubsub.loop()) {
    pubsub.connect(CLIENT_ID.c_str());
  }

  if (client) {
    long current_time = millis();
    long previous_time = current_time;
    Serial.println("New client.");
    String http_message = client.readString();

    client.println("HTTP/1.1 200 OK");
    client.println("Connection: close");

    if (http_message.indexOf("GET") != -1) {
      Serial.println("Sending page...");
      client.println("Content-type:text/html");
      client.println();

      client.println(
R"(<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      html, body { height: 100%; }
      body { display: flex; align-items: center; justify-content: center; flex-direction: column; } 
    </style>
  </head>
  <body>
    <div id="id-info">ID: N/A</div>
    <div id="name">Name: <input id="name-input" name="name" /></div>
    <div id="location-info">Location: N/A</div>
    <script>
      const idElement = document.getElementById("id-info");
      const locationElement = document.getElementById("location-info");
      const nameInput = document.getElementById("name-input");

      const CHARSET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
      id = Array.apply(null, Array(16)).map(() => CHARSET[Math.floor(Math.random() * CHARSET.length)]).join("");
      idElement.innerText = `ID: ${id}`;

      let name = "";
      nameInput.addEventListener("change", ({ target: { value } }) => name = value);

      let lat = null
      let long = null
      if (navigator.geolocation) {
        navigator.geolocation.watchPosition(
          async ({ coords: { latitude, longitude } }) => {
            locationElement.innerHTML = `Location: ${latitude}, ${longitude}`
            lat = latitude
            long = longitude
          },
          ({ message }) => {
            locationElement.innerHTML = `Error: ${message}`
          }
        )
      } else {
        locationElement.innerHTML = 'Error: Geolocation is not supported by this browser'
      }

      async function sendUpdate() {
        if (id && lat && long && name) {
          await fetch("/", {
            method: "POST",
            body: `${id},${name},${lat},${long}`
          })
        }
      }

      setInterval(sendUpdate, 5000);
    </script>
  </body>
</html>)"
      );
      client.println();
    } else if (http_message.indexOf("POST") != -1) {
      Serial.println("Location data received.");
      client.println();

      String body = get_body(http_message);
      
      Serial.printf("MQTT connection: %s\n", pubsub.connected() ? "CONNECTED" : "DISCONNECTED");
      bool publish_status = pubsub.publish(TOPIC, body.c_str());
      Serial.printf("Publish status: %s\n", publish_status ? "SUCCESS" : "FAILURE");
    }

    client.stop();
    Serial.println("Client disonnected.");
  }
}