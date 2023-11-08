#include <Arduino.h>
#include <AsyncElegantOTA.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>
#include <WiFiUdp.h>

#define adc1 12
#define adc2 27
#define adc3 33
#define SCK 5
#define MISO 19
#define MOSI 18

#define ON_SR 13
#define ON_HW 15
#define SDcard 21 // pins IO1 and Tx are soldered together
#define set 14
#define reset 32
#define trig 26 // test pulse for SR function
#define FORMAT_LITTLEFS_IF_FAILED true
#define battPin A13

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
String mess2Send="";

AsyncWebServer server(80);
File root;
const char *html = "<p>%PLACEHOLDER%</p>";
String processor(const String &var)
{
    String listHtml = "";
    File root = LittleFS.open("/");
    File file = root.openNextFile();

    while (file)
    {

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

unsigned long time1;
int beginrotW;
int endrotW;
///////////ADC//////////////
int result = 0x37; // register selection where the ADC result is stored
long ADC1_result;
long ADC2_result;
long ADC3_result;
long max_x = 0, max_y = 0, max_z = 0;
long min_x = 16777215, min_y = 16777215, min_z = 16777215;
long avg_x = 0, avg_y = 0, avg_z = 0;
long sum_x = 0, sum_y = 0, sum_z = 0;
byte drdy1, drdy2, drdy3;
byte READ = 0b10000000;
bool i = false;
int count_x = 0, count_y = 0, count_z = 0;

////////////SD///////////////
File file;    // file with ADC data
File logFile; // file with logs of date, battery voltage and battery temp
String dataMessage;
byte dataBuffer[100];
int r = 0;
////////////RTC///////////////
RTC_DS3231 rtc;
float battVolt = 0;
float rtcTemp = 0;
int timestamp;
bool ledOn = false;
int etrNum = 3;
bool dl = false;

String year, month, day, hour, minute, second;
String date, temps, TimeStamp;
unsigned long x0, x1;

int boxNum = 1;
int startTime;
String tsMeas;
String startDate;
int logTime;
String logDate;
float BattLvl = 0;
int runTime = 120000; // 120000 ms = 2 minutes

unsigned char CheckStatus_ADC(byte thisRegister, int adc)
{
    /* checks status of ADC 1*/
    // SRI why global variables here since they are not used anywhere else ?
    digitalWrite(adc, LOW); // reading
    SPI.transfer(thisRegister | READ);
    long ADC_status16 = SPI.transfer(0x00);
    long ADC_status8 = SPI.transfer(0x00);
    long ADC_status0 = SPI.transfer(0x00);
    long ADC_status24 = (ADC_status16 << 16) | (ADC_status8 << 8) | ADC_status0;
    digitalWrite(adc, HIGH);
    ADC_status0 = ADC_status0 >> 4;
    return (ADC_status0);
}

void ReadResult_ADC(byte thisRegister, int adc)
{
    long ADC_read1, ADC_read2;
    long ADC_result;
    /* reads result of ADC 1 */
    digitalWrite(adc, LOW); // reading
    SPI.transfer(thisRegister | READ);
    long ADC_hi = SPI.transfer(0x00);
    long ADC_mid = SPI.transfer(0x00);
    long ADC_lo = SPI.transfer(0x00);
    digitalWrite(adc, HIGH);
    if (i == false)
    {
        ADC_read1 = (ADC_hi << 16) | (ADC_mid << 8) | ADC_lo; // read 1 after set pulse
    }
    else
    {
        ADC_read2 = (ADC_hi << 16) | (ADC_mid << 8) | ADC_lo; // read 2 after reset pulse
        ADC_result = ADC_read2 - ADC_read1;
        // Serial.print(ADC1_result);
        // Serial.print(",");
        dataMessage += String(ADC_result) + ",";
        for (size_t i = 0; i < 3; i++)
        {
            dataBuffer[r] = lowByte(ADC_result >> 8 * (2 - i));
            r++;
        }
        if (adc == adc1)
        {
            sum_x = sum_x + ADC_result;
            count_x++;
            if (ADC_result > max_x)
            {
                max_x = ADC_result;
            }
            if (ADC_result < min_x)
            {
                min_x = ADC_result;
            }
        }
        else if (adc == adc2)
        {
            sum_y = sum_y + ADC2_result;
            count_y++;
            if (ADC2_result > max_y)
            {
                max_y = ADC2_result;
            }
            if (ADC2_result < min_y)
            {
                min_y = ADC2_result;
            }
        }
        else if (adc == adc3)
        {
            Serial.println(dataMessage);
            digitalWrite(SDcard, LOW);
            if (file)
            {
                // Serial.println("file is open to print");
                file.write(dataBuffer, r);
                r = 0;
            }
            file.write('\t');
            file.write('\r');
            file.write('\t');

            dataMessage = "";
            digitalWrite(SDcard, HIGH);

            // Serial.println(ADC3_result);

            sum_z = sum_z + ADC3_result;
            count_z++;
            if (ADC3_result > max_z)
            {
                max_z = ADC3_result;
            }
            if (ADC3_result < min_z)
            {
                min_z = ADC3_result;
            }
        }
    }
}

void ADC_conv()
{
    /* during conversion, gets the accelero data */
    /* checks for drdy pin and reads the result */
    drdy1 = CheckStatus_ADC(0x38, adc1); // function to check status register
    drdy2 = CheckStatus_ADC(0x38, adc2); // function to check status register
    drdy3 = CheckStatus_ADC(0x38, adc3); // function to check status register

    int time3 = millis();
    bool timeOut = false;

    while ((drdy1 != 1 || drdy2 != 1 || drdy3 != 1) & (millis() - time3 < 20))
    {
        drdy1 = CheckStatus_ADC(0x38, adc1); // function to check status register
        drdy2 = CheckStatus_ADC(0x38, adc2); // function to check status register
        drdy3 = CheckStatus_ADC(0x38, adc3); // function to check status register
    }
    // Serial.printf("%d,%d,%d\n", drdy1, drdy2, drdy3);

    ReadResult_ADC(result, adc1);
    ReadResult_ADC(result, adc2);
    ReadResult_ADC(result, adc3);
}

void Write(byte thisRegister, byte thisValue)
{
    /* SPI write function */
    digitalWrite(adc1, LOW);
    digitalWrite(adc2, LOW);
    digitalWrite(adc3, LOW);
    SPI.transfer(thisRegister);
    SPI.transfer(thisValue);
    digitalWrite(adc1, HIGH);
    digitalWrite(adc2, HIGH);
    digitalWrite(adc3, HIGH);
}

void SR_pwm()
{
    /* set and reset pulses */
    /* each pulse calls for a conversion by the ADC */
    digitalWrite(set, HIGH);
    digitalWrite(reset, LOW);
    delayMicroseconds(10);

    delayMicroseconds(50);
    i = false;         // if i = false , then ADC conversion during SET
    Write(0x01, 0x70); // writes to CONV_START - result stored in DATA7

    ADC_conv();

    digitalWrite(set, LOW);
    delayMicroseconds(10);

    digitalWrite(reset, HIGH);
    delayMicroseconds(10);

    delayMicroseconds(50);
    i = true;          // if i = true , then ADC conversion during RESET
    Write(0x01, 0x70); // writes to CONV_START - result stored in DATA7
    ADC_conv();

    digitalWrite(reset, LOW);
    delayMicroseconds(10);
}

void alarmSetup()
{
    // alarm setup
    if (rtc.begin())
    {
        rtc.disable32K();
        rtc.writeSqwPinMode(DS3231_OFF);
        rtc.disableAlarm(2);
        rtc.setAlarm1(rtc.now() + TimeSpan(1), DS3231_A1_Date);
        timestamp = rtc.now().unixtime();
    }
    else
    {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1)
            delay(10);
    }
}

void ADCsetup()
{
    Write(0x00, 0x00); // PD - register normal mode
    Write(0x03, 0x00); // CAL_START - PGA gain calibration
    Write(0x08, 0x38); // FILTER - set to SINC4 highest data rate
    Write(0x09, 0x46); // CTRL
    Write(0x0A, 0x00); // SOURCE
    Write(0x0B, 0x23); // MUX_CTRL0
    Write(0x0C, 0xFF); // MUX_CTRL1
    Write(0x0D, 0x00); // MUX_CTRL2
    Write(0x0E, 0x20); // PGA
    // Write(0x01, 0x70);  // writes to CONV_START - result stored in DATA7
    // Serial.println("setup of ADCs complete");
}

String dateRTC()
{ // date from RTC
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

void goSleep(int sleepTime)
{
    /* goes to sleep */
    // Serial.println("goSleep");

    long T = rtc.getTemperature();
    // sdSetup();

    digitalWrite(SDcard, LOW);
    logFile = SD.open("log.txt", FILE_APPEND);

    if (logFile)
    {
        // Serial.println("logging test");
        logFile.println(logDate);
        logFile.println(logTime);
        logFile.print(", temp: ");
        logFile.println(T);
        logFile.print("battery lvl: ");
        logFile.println(BattLvl);
        logFile.print("sleeping: ");
        logFile.print(sleepTime);
        logFile.println("s");
        logFile.print("\n");
    }

    logFile.flush();

    logFile.close();
    digitalWrite(SDcard, HIGH);

    alarmSetup();
    rtc.setAlarm1(rtc.now() + sleepTime, DS3231_A1_Date);

    rtc.clearAlarm(1);
    delay(500);
    // Serial.println("Alarm cleared");
}

void littleFS_test()
{
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
    {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    Serial.println("Opening");
    File file = LittleFS.open("/test.txt", FILE_WRITE);
    if (!file)
    {
        Serial.println("There was an error opening the file for writing");
        LittleFS.remove("/test.txt");
        return;
    }
    unsigned long beg = millis();
    for (size_t i = 0; i < 2000; i++)
    {
        unsigned long mid = millis();
        if (file.print(String(mid - beg) + "\t" + String(i) + ",TEST\n"))
        {
            // Serial.println("File was written");
            ;
        }
        else
        {
            Serial.println("File write failed");
        }
    }
    file.close();
    unsigned long end = millis();
    Serial.println(end - beg);

    File file2 = LittleFS.open("/test.txt");
    if (!file2)
    {
        Serial.println("Failed to open file for reading");
        return;
    }
    Serial.println("File Content:");
    while (file2.available())
    {
        // Serial.write(file2.read());
        file2.read();
    }
    file2.close();

    Serial.println("Test complete");
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
                // digitalWrite(LED, ledOn);
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
    String message = String(etrNum) + "," + String(timestamp) + "," + String(battVolt) + "," + String(rtcTemp) + "," + mess2Send + "," + String(emitMode);
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
        request->send(LittleFS, "/hw.bin", "text/plain", false);
    });

    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", html, processor);
    });
}

void setup()
{
    Serial.begin(115200);
    Serial.println("begin");
    SPI.begin(SCK, MISO, MOSI);
    SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
    Serial.println("spi");
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
    {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    littleFS_test();
    initServer();

    // pinMode(SDcard, OUTPUT);
    pinMode(adc1, OUTPUT); // chip selects for ADC
    pinMode(adc2, OUTPUT);
    pinMode(adc3, OUTPUT);
    pinMode(ON_SR, OUTPUT);
    pinMode(set, OUTPUT);   // Vset
    pinMode(reset, OUTPUT); // Vreset

    digitalWrite(adc1, HIGH);
    digitalWrite(adc2, HIGH);
    digitalWrite(adc3, HIGH);
    digitalWrite(ON_SR, LOW);
    digitalWrite(set, LOW);
    digitalWrite(reset, LOW);

    // alarm setup
    alarmSetup();
    // setup of ADCs
    ADCsetup();
    logTime = rtc.now().unixtime();
    randomSeed(logTime);

    // // digitalWrite(SDcard, LOW);
    mess2Send = String(beginrotW);
    pinMode(ON_HW, INPUT); // battery measure pin too
    analogReadResolution(12);

    battVolt = measBatt();
    mess2Send = mess2Send + "," + String(battVolt);
    Serial.println(mess2Send);
    pinMode(ON_HW, OUTPUT); // battery measure pin too

    startTime = rtc.now().unixtime();
    // String fn = fileName("hw");
    String fn = "/hw.bin";

    file = LittleFS.open(fn, FILE_WRITE);
    delay(100);

    dataMessage = startTime + "\n";
    digitalWrite(ON_SR, HIGH);
    digitalWrite(ON_HW, HIGH);
    delay(100);

    emit(0);
    time1 = millis();
    while (millis() - time1 < runTime)
    {
        dataMessage = dataMessage + String(millis() - time1) + ",";
        long tsMeas = millis() - time1;
        for (size_t i = 0; i < 4; i++)
        {
            dataBuffer[r] = lowByte(tsMeas >> 8 * (3 - i));
            r++;
        }

        // set and reset PWMs to initiate the honeywell
        // data is converted by ADC after set pulse and reset pulse
        // turn on the BOOST circuits

        SR_pwm();

        // after acquiring data , sleep for sometime
        // bufResults(); //put all results in buffer to save and send
        // saveResults(); //save results to transmit later
    }
    // digitalWrite(SDcard, LOW);

    file.close();
    // file.close();
    delay(100);

    // digitalWrite(SDcard, HIGH);

    delay(10);

    avg_x = sum_x / count_x;
    avg_y = sum_y / count_y;
    avg_z = sum_z / count_z;
    mess2Send = mess2Send + "," + String(max_x) + "," + String(min_x) + "," + String(max_y) + "," + String(min_y) + "," + String(max_z) + "," + String(min_z);
    mess2Send = mess2Send + "," + String(avg_x) + "," + String(avg_y) + "," + String(avg_z) ;
    // Serial.println(mess2Send);
    // mess2Send = "";
    logDate = dateRTC();
    // Serial.println("stop");
    logTime = rtc.now().unixtime();
    Serial.println(mess2Send);
    emit(1);

    // WiFi.mode(WIFI_STA);
    // WiFi.begin(ssid, password);
    // Serial.println("");

    // // Wait for connection
    // while (WiFi.status() != WL_CONNECTED)
    // {
    //     delay(500);
    //     Serial.print(".");
    // }
    // Serial.println("");
    // Serial.print("Connected to ");
    // Serial.println(ssid);
    // Serial.print("IP address: ");
    // Serial.println(WiFi.localIP());

    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send(200, "text/plain", "Hi! I am ESP32."); });
    // server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request)
    //           {
    //     int paramsNr = request->params();
    //     Serial.println(paramsNr);
    //     String downFile = "/1/test1.txt";

    //     for (int i = 0; i < paramsNr; i++) {

    //         AsyncWebParameter *p = request->getParam(i);
    //         Serial.print("Param name: ");
    //         Serial.println(p->name());
    //         Serial.print("Param value: ");
    //         Serial.println(p->value());
    //         Serial.println("------");
    //         downFile = p->value();
    //     }

    //     request->send(LittleFS, downFile, "text/plain", true); });
    // server.on("/rend", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send(LittleFS, "/hw.bin", "text/plain", false); });
    // server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send_P(200, "text/html", html, processor); });
    // AsyncElegantOTA.begin(&server); // Start ElegantOTA

    // server.begin();

    // goSleep(1800); //sleep for 14400
}

unsigned long prev_mil = 0;
void loop(void)
{
    if ((millis() - prev_mil) > 10000)
    {
        Serial.println("tick");
        prev_mil = millis();
        HTTPClient http;

        http.begin("http://192.168.125.163/receiver"); // URL for the receiver endpoint
        http.addHeader("Content-Type", "text/plain");

        String payload = WiFi.localIP().toString(); // The string you want to send

        int httpResponseCode = http.POST(payload);
        Serial.println("payload : " + payload);

        if (httpResponseCode > 0)
        {
            String response = http.getString();
            Serial.println("HTTP Response code: " + String(httpResponseCode));
            Serial.println("Response: " + response);
        }
        else
        {
            Serial.println("HTTP Error code: " + String(httpResponseCode));
        }

        http.end();
    }
}
