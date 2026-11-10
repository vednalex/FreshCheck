//imports
#include <DHT.h>
#include <WiFiS3.h>
#include "arduino_secrets.h"
#include "page.h"  // the web page (HTML + JavaScript)

// Constants
#define DHTPIN 11
#define DHTTYPE DHT11

// create dht object
DHT mydht(DHTPIN, DHTTYPE);

//vars
int br = 9600;
int setUpDelay = 1000;
unsigned long lastRead = 0;            // when we last read the sensor (in ms)
const unsigned long READ_MS = 2000;    // how often to read: every 2000 ms = 2 s
float tempC;                           //temp in celcuis
float tempF;                           //temp in farenhiet
float hum;                             //humidity
float dewF = NAN;                      //dew in farenheit
const int HIST = 180;                  // number of slots (180 min = 3 hours)
float histDP[HIST];                    // the saved dew points
int histCount = 0;                     // how many slots are filled so far
int histNext = 0;                      // which slot to write next
unsigned long lastSample = 0;          // when we last saved a sample
const unsigned long SAMPLE_MS = 5000;  // save every 60 s

//web server
WiFiServer server(80);

// Reads the DHT11 and updates tempC, tempF, hum
void readSensor() {
  float c = mydht.readTemperature(false);
  float h = mydht.readHumidity();

  // If the read failed, keep the last good values instead of storing nan
  if (isnan(c) || isnan(h)) {
    Serial.println("DHT read failed");
    return;  // leave the function early
  }

  tempC = c;
  tempF = c * 9.0 / 5.0 + 32.0;  // convert instead of reading the sensor twice
  hum = h;
  dewF = calcDewPointF(tempC, hum);
}

// Dew point (Magnus formula). Inputs: temp in °C, humidity in %. Returns °F.
float calcDewPointF(float tC, float rh) {
  const float a = 17.62;
  const float b = 243.12;
  float gamma = log(rh / 100.0) + (a * tC) / (b + tC);  // log() = natural log (ln)
  float dewC = (b * gamma) / (a - gamma);
  return dewC * 9.0 / 5.0 + 32.0;  // hand back the answer in °F
}

// Saves the current dew point into the history
void saveSample() {
  if (isnan(dewF)) return;            // no good reading yet, so don't save
  histDP[histNext] = dewF;            // write into the next slot
  histNext = (histNext + 1) % HIST;   // move forward, wrapping 179 → 0
  if (histCount < HIST) histCount++;  // count up until all 180 are full
}

// Sends the HTML web page
// Sends the web page
void sendPage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Connection: close");
  client.println();
  client.print(PAGE);
}

// Sends the latest reading as JSON, e.g. {"f":72.5,"h":45,"dp":49.8}
void sendData(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  String json = "{\"f\":" + String(tempF, 1) + ",\"h\":" + String(hum, 0) + ",\"dp\":" + String(dewF, 1) + "}";
  client.println(json);
}

// Sends the history, oldest → newest, e.g. [55.1,55.3,56.0]
void sendHistory(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  String json = "[";
  int oldest = (histNext - histCount + HIST) % HIST;  // where the oldest sample is
  for (int i = 0; i < histCount; i++) {
    if (i > 0) json += ",";                          // comma BETWEEN items, not before the first
    json += String(histDP[(oldest + i) % HIST], 1);  // walk forward, wrapping around
  }
  json += "]";
  client.println(json);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(br);
  delay(1500);  // serial monitor open time

  Serial.print("Connecting to ");
  Serial.println(SECRET_SSID);

  //join network if fail try again in 5 sec
  while (WiFi.begin(SECRET_SSID, SECRET_PASS) != WL_CONNECTED) {
    Serial.println("Connection Failed, retrying...");
    delay(5000);
  }

  //waiting for a IP Address
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(200);
  }

  Serial.print("Connection Secured! IP Address: ");
  Serial.println(WiFi.localIP());

  mydht.begin();
  delay(setUpDelay);

  // reads so that when interface is launched it starts with accurate data
  readSensor();
  saveSample();
  server.begin();  // start looking for browsers
}

void loop() {

  WiFiClient client = server.available();  //did the browser connect
  if (client) {
    delay(10);                                          // time for request to arrive
    String requestLine = client.readStringUntil('\n');  // e.g. "GET /data HTTP/1.1"
    while (client.available()) client.read();           // throw away the rest

    if (requestLine.startsWith("GET /history")) {
      sendHistory(client);
    } else if (requestLine.startsWith("GET /data")) {
      sendData(client);
    } else {
      sendPage(client);
    }

    client.stop();  // hang up
  }
  if (millis() - lastRead >= READ_MS) {  // has it been at least 2 s?
    lastRead = millis();                 // start the next 2-s countdown

    readSensor();

    String message = "Temp in C: " + String(tempC) + " Temp in F: " + String(tempF) + " Humidity: " + String(hum) + " Dew point: " + String(dewF);
    Serial.println(message);
  }

  if (millis() - lastSample >= SAMPLE_MS) {  // has it been 60 s?
    lastSample = millis();
    saveSample();
  }
}
