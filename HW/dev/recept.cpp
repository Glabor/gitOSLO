#include <Arduino.h>
#include <AsyncElegantOTA.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>

#define FORMAT_LITTLEFS_IF_FAILED true

String ssid = "Incubateur";
String password = "IDescartes77420*";
AsyncWebServer server(80);

bool goget = false;
String targetIP;

void setup() {
    Serial.begin(115200);
    Serial.println("begin");

    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/rend", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/read.txt", "text/plain", false);
    });

    server.on(
        "/receiver", HTTP_POST, [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Serial.write(data, len);
            Serial.println(String(data, len));
            targetIP=String(data, len);
            goget=true;
            request->send(200);
        });
    server.begin();
}

unsigned long prev_mil = 0;
void loop(void) {
    if ((millis() - prev_mil) > 2000) {
        Serial.println("tick");
        prev_mil = millis();
        if ((WiFi.status() == WL_CONNECTED) && goget) {
            HTTPClient http;
            goget=false;
            http.begin("http://"+targetIP+"/rend");
            int time = millis();
            File file = LittleFS.open("/read.txt", FILE_WRITE);
            if (!file) {
                Serial.println("There was an error opening the file for writing");
                LittleFS.remove("/read.txt");
            }

            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String webpageContent = http.getString();
                // You have the HTML content of the webpage in 'webpageContent'.
                file.println(webpageContent);
            } else {
                Serial.printf("HTTP request failed with error code %d\n", httpCode);
            }
            file.close();
            http.end();
            Serial.printf("time : %d\n", millis() - time);
        }
    }
}