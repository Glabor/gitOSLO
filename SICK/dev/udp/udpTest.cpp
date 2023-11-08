#include <Arduino.h>
// #include <AsyncElegantOTA.h>
#include <WebServer.h>

#include <RTClib.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define battPin A13

const char *ssid = "Incubateur";
const char *password = "IDescartes77420*";
const char *udpAddress = "192.168.125.168";
WiFiUDP Udp;
unsigned int localUdpPort = 4210; //  port to listen on
unsigned int servUdpPort = 4210;  //  port to listen on
char incomingPacket[255];         // buffer for incoming packets

bool ledOn = false;
float battVolt = 0;

float measBatt() {
    float cellVolt=0.0;
    int count=0;
    for (int battCount = 1; battCount <= 100; battCount++) {
        count++;
        cellVolt = (float)((cellVolt * (count - 1) + (float)analogRead(battPin)) / (float)count); // reading pin to measure battery level
        if (cellVolt > battVolt) {
            battVolt = cellVolt;
        }
        Serial.print("batt :");
        Serial.println(cellVolt/ 4096 * 3.3 * 2);
        delay(10);
    }
    Serial.print("final : ");
    Serial.println(battVolt/ 4096 * 3.3 * 2);
    battVolt = battVolt / 4096 * 3.3 * 2; // convert measure from 12bytes to volts
    // rtcTemp = rtc.getTemperature();
    return cellVolt;
}

void setup() {
    pinMode(13, OUTPUT);
    int status = WL_IDLE_STATUS;
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected to wifi");
    Udp.begin(localUdpPort);
    Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
}

void readpacket() {
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        // receive incoming UDP packets
        Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
        int len = Udp.read(incomingPacket, 255);
        if (len > 0) {
            incomingPacket[len] = 0;
        }
        Serial.printf("UDP packet contents: %s\n", incomingPacket);
        Serial.printf("%x \n", incomingPacket[0]);
        if (incomingPacket[0] == 0x01) {
            ledOn = true;
        } else {
            ledOn = false;
        }
    }
    if (ledOn) {
        digitalWrite(13, HIGH);

    } else {
        digitalWrite(13, LOW);
    }
}

void loop() {
    float batt=measBatt();
    readpacket();
    // once we know where we got the inital packet from, send data back to that IP address and port
    Udp.beginPacket(udpAddress, servUdpPort);
    // Just test touch pin - Touch0 is T0 which is on GPIO 4.
    Udp.print(String(batt));
    Udp.endPacket();
    Serial.println("send");
    delay(2000);
}
