#include <Arduino.h>
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>
#include <WiFiUdp.h>

#define APin 36
#define boostCtrl 33
#define tempPin 39
#define battPin A13
#define SD_CS 34
#define FORMAT_LITTLEFS_IF_FAILED true
#define LED 13

String ssid = "SENSAR 4661";
String password = "H6{3g897";
// String ssid = "chgServer";
// const char *udpAddress = "172.20.10.2";
// const char *udpAddress = "10.42.0.1";
const char *udpAddress = "192.168.216.59";

WiFiUDP Udp;
unsigned int localUdpPort = 4210; //  port to listen on
unsigned int servUdpPort = 4210;  //  port to listen on
char incomingPacket[255];         // buffer for incoming packets

bool ledOn = false;
bool dl = false;
unsigned long lastTime = 0;
unsigned long timerDelay = 100;

bool testBattBool = false;
String fileDate;

int battSend;
int tempLvl;
float battTemp;
int etrNum = 2;
int wake0 = 1589194800;

RTC_DS3231 rtc;
float battVolt = 0;
float rtcTemp = 0;
int timestamp;

File myFile;
bool ledState = true;
AsyncWebServer server(80);
File root;

byte captSubBuf[3];
byte sdBuf[1200];
int r = 0;
float sensAvg;
int avgCount = 0;

File logFile;
String logName = "/log.txt";

const char *html = "<p>%PLACEHOLDER%</p>";
String processor(const String &var) {
    String listHtml = "";
    File root = LittleFS.open("/");
    File file = root.openNextFile();

    while (file) {

        Serial.print("FILE: ");
        Serial.println(file.path());
        // Serial.println(file.name());
        listHtml += "<a href = \"down/?file=" + String(file.path()) + "\">" + String(file.path()) + "</a><br />";

        file = root.openNextFile();
    }
    Serial.println(listHtml);
    Serial.println(var);

    if (var == "PLACEHOLDER")
        return listHtml;

    return String();
}

void initPins() {
    pinMode(13, OUTPUT);
    pinMode(boostCtrl, OUTPUT);
    pinMode(APin, INPUT);
    pinMode(tempPin, INPUT);
    pinMode(battPin, INPUT);
    // pinMode(SD_CS, OUTPUT);*

    digitalWrite(LED, ledState);
    digitalWrite(boostCtrl, LOW);
    digitalWrite(boostCtrl, LOW);

    analogReadResolution(12);
}

void initSerial() {
    Serial.begin(115200);
    Serial.println("begin");
}

void initLittleFS() {
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
}

void alarmSetup() {
    if (rtc.begin()) {
        rtc.disable32K();
        rtc.writeSqwPinMode(DS3231_OFF);
        rtc.disableAlarm(2);
        rtc.setAlarm1(rtc.now() + TimeSpan(1), DS3231_A1_Date);
        timestamp = rtc.now().unixtime();
    } else {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1)
            delay(10);
    }
}

String dateRTC() { // date from RTC
    DateTime dateDate = rtc.now();
    String dateString = "";
    dateString += String(dateDate.year(), DEC);
    dateString += "/";
    dateString += String(dateDate.month(), DEC);
    dateString += "/";
    dateString += String(dateDate.day(), DEC);
    dateString += "    ";
    dateString += String(dateDate.hour(), DEC);
    dateString += ":";
    dateString += String(dateDate.minute(), DEC);
    dateString += ":";
    dateString += String(dateDate.second(), DEC);
    return dateString;
}

void initServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Hi! I am ESP32.");
    });

    server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request) {
        int paramsNr = request->params();
        Serial.println(paramsNr);
        String downFile = "/1/test1.txt";
        for (int i = 0; i < paramsNr; i++) {
            AsyncWebParameter *p = request->getParam(i);
            Serial.print("Param name: ");
            Serial.println(p->name());
            Serial.print("Param value: ");
            Serial.println(p->value());
            Serial.println("------");
            downFile = p->value();
        }
        request->send(LittleFS, downFile, "text/plain", true);
    });

    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
        delay(500);
        dl = true;
        request->send(200, "text/plain", "server stop");
    });

    server.on("/rend", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/sick.bin", "text/plain", false);
    });

    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", html, processor);
    });
}

void initComp() {
    initPins();
    initSerial();
    initLittleFS();
    alarmSetup();
    initServer();
}

float measBatt() {
    float cellVolt;
    int count=0;
    for (int battCount = 1; battCount <= 100; battCount++) {
        count++;
        float meas=analogRead(battPin);
        cellVolt = (float)((cellVolt * (count - 1) + (float)analogRead(battPin)) / (float)count); // reading pin to measure battery level

        // cellVolt = analogRead(battPin); // reading pin to measure battery level
        // if (cellVolt > battVolt) {
        //     battVolt = cellVolt;
        // }
        // Serial.print("batt :");
        // Serial.println(cellVolt / 4096 * 3.3 * 2);
        delay(10);
    }
    // Serial.print("final : ");
    // Serial.println(battVolt / 4096 * 3.3 * 2);
    cellVolt = 0.3 + cellVolt / 4096 * 3.3 * 2; // convert measure from 12bytes to volts
    rtcTemp = rtc.getTemperature();
    return cellVolt;
}

bool wifiConnect() {
    int count = 0;
    // trying to connect to wifi a number of times
    for (size_t j = 0; j < 3; j++) {
        WiFi.begin(ssid, password);
        // WiFi.begin(ssid);
        Serial.println("Connecting");

        // Wait for connection (10 dots)
        while ((WiFi.status() != WL_CONNECTED) && count < 10) {
            for (int i = 0; i < 10; i++) {
                ledOn = !ledOn;
                digitalWrite(LED, ledOn);
                delay(100);
            }
            Serial.print(".");
            delay(200);
            count++;
        }
        count = 0;
        // if connected : update value
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("");
            Serial.print("Connected to WiFi network with IP Address: ");
            Serial.println(WiFi.localIP());
            // if (!MDNS.begin("esp32")) {
            //     Serial.println("Error starting mDNS");
            // }
            Udp.begin(localUdpPort);
            Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);
            return true;
        } else {
            Serial.println("WiFi connection trial failed");
        }
        // turn off wifi
        WiFi.disconnect(true);
        delay(500);
        // WiFi.mode>(WIFI_OFF);
    }
    return false;
}

bool readpacket() {
    int packetSize = Udp.parsePacket();
    bool rec = false;
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
        rec = true;
    }
    if (ledOn) {
        digitalWrite(13, HIGH);

    } else {
        digitalWrite(13, LOW);
    }
    return rec;
}

bool udpRafale(String mess) {
    int toRafale = 10;
    int startMillis = millis();
    int sendMillis;
    bool readB = false;
    bool cont = true;
    mess=WiFi.localIP().toString().c_str()+String(",")+mess;

    while (cont && (millis() - startMillis < toRafale * 1000)) {
        if (readB) {
            Udp.beginPacket(udpAddress, servUdpPort);
            Udp.print(mess);
            Udp.endPacket();
            sendMillis = millis();
            readB = false;
        }
        if (millis() - sendMillis > 100) {
            cont = !readpacket();
            readB = true;

            Serial.print(millis() - startMillis);
            Serial.print(", ");
            Serial.println(cont);
        }
    }
    return cont;
}

void emit(int emitMode) {
    String message = String(etrNum) + "," + String(timestamp) + "," + String(battVolt) + "," + String(rtcTemp) + "," + String(sensAvg) + "," + String(emitMode);
    if (wifiConnect()) {
        bool conf = !udpRafale(message);
        if (emitMode == 1 && conf) {
            Serial.println("server begin");
            server.begin();
            int servStart = millis();
            while (!dl && (millis() - servStart < 10000)) {
                delay(2000);
            }
        }
    }
    WiFi.disconnect(true);
}

void logStart() {
    logFile = LittleFS.open(logName, FILE_APPEND);
    if (logFile) {
        logFile.print("measure capt : ");
        logFile.println(dateRTC());
    }
    logFile.flush();
    logFile.close();
}

void accBuffering(int meas) {
    // divide int to two bytes
    sdBuf[r] = highByte(meas);
    r++;
    sdBuf[r] = lowByte(meas);
    r++;
}

void sensAcq(int ts, File file) {
    r = 0;
    while (r < 1000) {
        // time
        accBuffering((int)(millis() - ts));
        /// accumulate data in captBuf, to save in chunks later
        int micros1 = micros();
        int count1 = 0;
        float val1 = 0;
        while ((micros() - micros1) < 4000) {
            count1++;
            val1 = (float)((float)(val1 * (count1 - 1) + (float)analogRead(APin)) / (float)count1); // read adc
        }
        int val = (int)val1;
        accBuffering(val);

        // Serial.print("values measured : ");
        // Serial.print(val1);
        // Serial.print(", ");
        // Serial.println(val);

        avgCount++;
        sensAvg = (float)((float)(sensAvg * (avgCount - 1) + (float)val1) / (float)avgCount);
    }
    for (int j = 0; j < r; j++) {
        file.write(sdBuf[j]);
    }
    file.write('\t');
    file.write('\r');
    file.write('\t');

    // start accumulating again, until time runs out for this phase
}

void saveSens() {
    int sensTime = 60;
    digitalWrite(boostCtrl, LOW);
    digitalWrite(boostCtrl, HIGH);
    r = 0;
    logStart();
    String sensName = "/sick.bin";
    int sensMillis = millis();
    File file = LittleFS.open(sensName, FILE_WRITE);
    if (file) {
        int ledMillis = millis();
        while ((millis() - sensMillis) < sensTime * 1000) {
            if ((millis() - ledMillis) > 1000) {
                ledMillis = millis();
                digitalWrite(LED, !ledState);
                ledState = !ledState;
            }
            sensAcq(sensMillis, file);
        }
    }
    file.flush();
    file.close();
    digitalWrite(boostCtrl, LOW);
    digitalWrite(boostCtrl, LOW);
}

void goSleep(float sleepTime) {
    // setting a time to wake up and clearing alarm
    int wakeTime = wake0 + etrNum * 120;
    // int sleepCyc[3] = {2000, 1, 8};
    int sleepT = 12; // 12=1h

    int block = 300;
    int nowTime = rtc.now().unixtime();
    int timeDif = (sleepT * block) - (nowTime - wakeTime) % (sleepT * block);
    alarmSetup();
    // while (!rtc.setAlarm1(rtc.now() + TimeSpan(timeDif), DS3231_A1_Date)) {}  // plan time to next block + cyc)
    // while (!rtc.setAlarm1(rtc.now() + timeDif, DS3231_A1_Date)) {
    // } // plan time to next block + cyc)
    while (!rtc.setAlarm1(rtc.now() + sleepTime, DS3231_A1_Date)) {
    } // plan time to next block + cyc)

    logFile = LittleFS.open(logName, FILE_APPEND);
    if (logFile) {
        logFile.print("sleeping ");
        logFile.print(String(sleepTime));
        logFile.print(" seconds : ");
        logFile.println(dateRTC());
    }
    logFile.flush();
    logFile.close();

    while (true) {
        digitalWrite(13, !ledState);
        ledState = !ledState;

        rtc.clearAlarm(1);
        Serial.println("sleep");
        delay(500);
    }
}

void setup() {
    initComp();

    battVolt=measBatt();
    emit(0);

    saveSens();
    emit(1);

    goSleep(60);
}

void loop() {
    delay(1000);
    Serial.println("sleep");
}