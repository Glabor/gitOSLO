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

// String ssid = "chgServer";
// String ssid = "Incubateur";
// String password = "IDescartes77420*";
String ssid = "iPhone Guillaume";
String password = "luminaire";
// const char *udpAddress = "chg.me.local";
const char *udpAddress = "172.20.10.4";
WiFiUDP Udp;
unsigned int localUdpPort = 4210; //  port to listen on
unsigned int servUdpPort = 4210;  //  port to listen on
char incomingPacket[255];         // buffer for incoming packets

bool ledOn = false;
bool cont = true;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
// unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 100;

bool testBattBool = false;
String fileDate;

int battSend;
int tempLvl;
float battTemp;
int etrNum = 1;
int wake0 = 1589194800;

RTC_DS3231 rtc;
File myFile;
bool ledState = true;
AsyncWebServer server(80);
File root;

byte captSubBuf[3];
byte sdBuf[1200];
int r = 0;

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

// void sdSetup() {
//     digitalWrite(SDcard, LOW);
//     /*set up sd*/
//     int milliSD = millis();
//     while ((!SD.begin()) && ((millis() - milliSD) < 3000)) {
//     }
//     if (!SD.begin()) {
//         // Serial.println("SD begin fail");
//         rtc.setAlarm1(rtc.now() + TimeSpan(5), DS3231_A1_Date); // set alarm for a second in the future
//         rtc.clearAlarm(1);
//     }
//     digitalWrite(SDcard, HIGH);
// }

void alarmSetup() {
    // alarm setup
    if (rtc.begin()) {
        rtc.disable32K();
        rtc.writeSqwPinMode(DS3231_OFF);
        rtc.disableAlarm(2);
        rtc.setAlarm1(rtc.now() + TimeSpan(1), DS3231_A1_Date);
    } else {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1)
            delay(10);
    }
}

float measBatt() {
    float battVolt = 0;
    float cellVolt;
    for (int battCount = 1; battCount <= 100; battCount++) {
        cellVolt = analogRead(34); // reading pin 9 to measure battery level
        if (cellVolt > battVolt) {
            battVolt = cellVolt;
        }
        delay(10);
    }
    battVolt = battVolt / 4096 * 3.3 * 2; // convert measure from 12bytes to volts
    return battVolt;
}

// void sdSetup() {
//     digitalWrite(SDcard, LOW);
//     /*set up sd*/
//     int milliSD = millis();
//     while ((!SD.begin()) && ((millis() - milliSD) < 3000)) {
//     }
//     if (!SD.begin()) {
//         // Serial.println("SD begin fail");
//         rtc.setAlarm1(rtc.now() + TimeSpan(5), DS3231_A1_Date); // set alarm for a second in the future
//         rtc.clearAlarm(1);
//     }
//     digitalWrite(SDcard, HIGH);
// }

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

void accBuffering(int meas) {
    // divide int to two bytes
    sdBuf[r] = highByte(meas);
    r++;
    sdBuf[r] = lowByte(meas);
    r++;
}

void saveCapt(int captTime) {
    digitalWrite(boostCtrl, LOW);
    digitalWrite(boostCtrl, HIGH);
    // phase of  definite time to save values from adc, from a sensor
    int cycCount = 0;
    r = 0;
    unsigned int measVal;

    // comSPI(SD_CS);
    // SD.begin(SD_CS);
    logFile = LittleFS.open(logName, FILE_APPEND);
    if (logFile) {
        logFile.print("measure capt : ");
        logFile.println(dateRTC());
    }
    logFile.flush();
    logFile.close();

    // create folder for file
    DateTime startDate = rtc.now();
    int startTime = startDate.unixtime();
    // create folder to save chunk of data
    fileDate = String(startTime);
    String beginStr = fileDate.substring(0, 7);
    String endStr = fileDate.substring(7, 10);

    ////check if folder is in fact a file, following a false writing in sd card
    ////if the file is bad, it gets deleted (corresponds to about 15 min)
    // if (SD.exists("/capt/" + beginStr)) { // first folder exists
    //     File folder = SD.open("/capt/" + beginStr);
    //     if (folder.isDirectory()) { // first folder is a directory
    //     } else {                    // if not directory, it gets deleted
    //         SD.remove("/capt/" + beginStr);
    //     }

    //     if (SD.exists("/capt/" + beginStr + "/" + endStr)) { // scnd folder exists (cannot exist if first is deleted)
    //         File folder = SD.open("/capt/" + beginStr + "/" + endStr);
    //         if (folder.isDirectory()) { // scnd folder is directory
    //         } else {                    // if not directory, it gets deleted
    //             SD.remove("/capt/" + beginStr + "/" + endStr);
    //         }
    //     }
    // }

    // if (!SD.exists("/capt/" + beginStr + "/" + endStr)) { // create new folder
    //     SD.mkdir("/capt");
    //     SD.mkdir("/capt/" + beginStr);
    //     SD.mkdir("/capt/" + beginStr + "/" + endStr);
    // }
    // String captName = "/capt/" + beginStr + "/" + endStr + "/capt.bin";
    String captName = "/sick.bin";
    // when enough data in stacked, save on sd
    int milli0 = millis();
    File fichier = LittleFS.open(captName, FILE_WRITE);
    // save data
    if (fichier) {
        // loop measures during defined time
        int milliB = millis();
        while ((millis() - milli0) < captTime * 1000) {
            cycCount++;
            if ((millis() - milliB) > 1000) {
                milliB = millis();
                digitalWrite(13, !ledState);
                ledState = !ledState;
            }
            while (r < 1000) {
                // time
                accBuffering((int)(millis() - milli0));
                /// accumulate data in captBuf, to save in chunks later
                int micros1 = micros();
                int count1 = 0;
                float val1 = 0;
                while ((micros() - micros1) < 4000) {
                    count1++;
                    val1 = (float)((val1 * (count1 - 1) + (float)analogRead(APin)) / (float)count1); // read adc
                }
                int val = (int)val1;
                Serial.print("values measured : ");
                Serial.print(val1);
                Serial.print(", ");

                Serial.println(val);

                accBuffering(val);
                // delay(5);//TODO average
            }
            for (int j = 0; j < r; j++) {
                fichier.write(sdBuf[j]);
            }
            fichier.write('\t');
            fichier.write('\r');
            fichier.write('\t');

            // start accumulating again, until time runs out for this phase
            r = 0;
            // Serial.println("CAPT#" + String(cycCount));
        }
    }
    fichier.flush();
    fichier.close();
    digitalWrite(boostCtrl, LOW);
    digitalWrite(boostCtrl, LOW);
    // delay(5000);
}

String lastTimePath = "";

// void emit(void) {
//   int count = 1;
//   bool nxtBool = false;
//   while (!restartBool) {
//     if ((millis() - lastTime) > timerDelay) {
//       //Check WiFi connection status
//       if (WiFi.status() == WL_CONNECTED) {
//         WiFiClient client;
//         HTTPClient http;
//         // Your Domain name with URL path or IP address with path
//         http.begin(client, serverName);
//         if (testBattBool) {
//           //test the battery to compare with lora feather
//           restartBool = true;
//           r = 37;
//           for (size_t i = 0; i < r; i++) {
//             binBuf[i] = lowByte(i);
//           }
//           binBuf[0] = highByte(battSend);
//           binBuf[1] = lowByte(battSend);
//         } else {
//           binBuf[0] = '\r';
//           binBuf[1] = '\n';
//           binBuf[2] = '\r';
//           binBuf[3] = '\n';
//           binBuf[4] = highByte(etrNum);
//           binBuf[5] = lowByte(etrNum);
//           binBuf[6] = highByte(battSend);
//           binBuf[7] = lowByte(battSend);
//           binBuf[8] = highByte(tempLvl);
//           binBuf[9] = lowByte(tempLvl);
//           r = 10;
//           //get data to send from file
//           Serial.println(lastTimePath);
//           myFile = SD.open(lastTimePath + "/capt.bin");
//           // myFile = SD.open("/capt/1673531/502/capt1.html");
//           myFile.seek(sdPos);
//           while ((myFile.available()) && (r < 6000)) {
//             // txtBuf[r] = (char)myFile.read();
//             binBuf[r] = myFile.read();
//             r++;
//             sdPos = myFile.position();
//           }
//           if (!myFile.available()) {
//             sdPos = 0;
//             // count++;
//             // nxtBool = true;
//             restartBool = true;
//           }
//           // if (count > 3) {
//           //   restartBool = true;
//           // }
//           myFile.close();
//         }
//         // If you need an HTTP request with a content type: text/plain
//         http.addHeader("Content-Type", "application/octet-stream");
//         // int httpResponseCode = http.POST(String(txtBuf));
//         int httpResponseCode = http.POST(binBuf, r);
//         r = 0;
//         Serial.print("HTTP Response code: ");
//         Serial.println(httpResponseCode);
//         // if (rstBool) {
//         //   delay(10000);
//         //   rstBool = false;
//         // }
//         // Free resources
//         http.end();
//         // if (nxtBool) {
//         //   delay(100);
//         //   http.begin(client, serverName);
//         //   http.addHeader("Content-Type", "text/plain");
//         //   int httpResponseCode = http.POST(String(lastTimePath + "/capt.bin"));
//         //   // int httpResponseCode = http.POST(String(lastTimePath + "/capt" + String(count) + ".html"));
//         //   http.end();
//         //   nxtBool = false;
//         // }
//       } else {
//         Serial.println("WiFi Disconnected");
//         restartBool = true;
//       }
//       lastTime = millis();
//     }
//   }
// }

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
    while (!rtc.setAlarm1(rtc.now() + timeDif, DS3231_A1_Date)) {
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
        delay(500);
        // Serial.println(rtc.alarmFired(1));
        // when alarm cleared, the system shuts down since the battery is cut off
        // battery comes back on when time of alarm comes
    }
}

void littleFS_test() {
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    Serial.println("Opening");
    File file = LittleFS.open("/test.txt", FILE_WRITE);
    if (!file) {
        Serial.println("There was an error opening the file for writing");
        LittleFS.remove("/test.txt");
        return;
    }
    unsigned long beg = millis();
    for (size_t i = 0; i < 2000; i++) {
        unsigned long mid = millis();
        if (file.print(String(mid - beg) + "\t" + String(i) + ",TEST\n")) {
            // Serial.println("File was written");
            ;
        } else {
            Serial.println("File write failed");
        }
    }
    file.close();
    unsigned long end = millis();
    Serial.println(end - beg);

    File file2 = LittleFS.open("/test.txt");
    if (!file2) {
        Serial.println("Failed to open file for reading");
        return;
    }
    Serial.println("File Content:");
    while (file2.available()) {
        // Serial.write(file2.read());
        file2.read();
    }
    file2.close();

    Serial.println("Test complete");
}

void setup() {
    int sleepTime = 33;
    int measTime = 10;
    pinMode(13, OUTPUT);

    Serial.begin(115200);
    Serial.println("begin");
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    littleFS_test();

    pinMode(13, OUTPUT);
    digitalWrite(13, ledState);
    pinMode(boostCtrl, OUTPUT);
    pinMode(APin, INPUT);
    pinMode(tempPin, INPUT);
    pinMode(battPin, INPUT);
    // pinMode(SD_CS, OUTPUT);
    digitalWrite(boostCtrl, LOW);
    digitalWrite(boostCtrl, LOW);

    // different phases of the measure cycle
    // setting up the different modules
    analogReadResolution(12);

    alarmSetup();
    // sdSetup();
    measBatt();

    // different measures and signals, parameter is time of phase in seconds
    // Serial.println("acc");
    analogReadResolution(12);
    Serial.println("capt");
    saveCapt(measTime);

    // SD.begin(SD_CS);
    Serial.println();

    // get last saved file to send
    //  lastTimePath = pathLast("/capt");
    lastTimePath = "/capt/" + fileDate.substring(0, 7) + "/" + fileDate.substring(7, 10);
    digitalWrite(boostCtrl, LOW);

    // emit
    int milli0 = millis();

    Serial.println("starting wifi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    // WiFi.begin(ssid);
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

    if (!MDNS.begin("esp32")) {
        Serial.println("Error starting mDNS");
        return;
    }

    Udp.begin(localUdpPort);
    Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

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
    server.on("/rend", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/sick.bin", "text/plain", false);
    });
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", html, processor);
    });
    AsyncElegantOTA.begin(&server); // Start ElegantOTA

    server.begin();

    // go sleep
    //  while (true) {
    //    if (((millis() - milli0) < 30000) || (restartBool)) {
    //      // server.end();
    //      Serial.println("sleep");
    //      goSleep(sleepTime);  ///1800
    //      Serial.println("restart");
    //      ESP.restart();
    //      Serial.println("restarted");
    //    }
    //    delay(100);
    //  }
    Serial.println("sleep");
    // goSleep(sleepTime); /// 1800
    // ESP.restart();
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
        cont = !cont;
    }
    if (ledOn) {
        digitalWrite(13, HIGH);

    } else {
        digitalWrite(13, LOW);
    }
}

void loop() {
    readpacket();

    if (cont) {
        Udp.beginPacket(udpAddress, servUdpPort);
        // Just test touch pin - Touch0 is T0 which is on GPIO 4.
        Udp.print("test");
        Udp.endPacket();
        Serial.println("send");
        Serial.println(cont);
        // once we know where we got the inital packet from, send data back to that IP address and port
        delay(100);
    }
    else{
        goSleep(20);
    }
    delay(1000);
}
