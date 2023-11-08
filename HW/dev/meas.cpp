#include <Arduino.h>
#include <AsyncElegantOTA.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>

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

String ssid = "Incubateur";
String password = "IDescartes77420*";
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
long ADC1_result, ADC1_hi, ADC1_mid, ADC1_lo;
long ADC2_result, ADC2_hi, ADC2_mid, ADC2_lo;
long ADC3_result, ADC3_hi, ADC3_mid, ADC3_lo;
long ADC1_status24, ADC1_status16, ADC1_status8, ADC1_status0;
long ADC2_status24, ADC2_status16, ADC2_status8, ADC2_status0;
long ADC3_status24, ADC3_status16, ADC3_status8, ADC3_status0;
long ADC1_read1, ADC1_read2, ADC2_read1, ADC2_read2, ADC3_read1, ADC3_read2;
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
int runTime = 60000; // 120000 ms = 2 minutes

unsigned char CheckStatus_ADC1(byte thisRegister)
{
    /* checks status of ADC 1*/
    digitalWrite(adc1, LOW); // reading
    SPI.transfer(thisRegister | READ);
    ADC1_status16 = SPI.transfer(0x00);
    ADC1_status8 = SPI.transfer(0x00);
    ADC1_status0 = SPI.transfer(0x00);
    ADC1_status24 = (ADC1_status16 << 16) | (ADC1_status8 << 8) | ADC1_status0;
    digitalWrite(adc1, HIGH);
    ADC1_status0 = ADC1_status0 >> 4;
    return (ADC1_status0);
}

unsigned char CheckStatus_ADC2(byte thisRegister)
{
    /* checks status of ADC 2*/
    digitalWrite(adc2, LOW); // reading
    SPI.transfer(thisRegister | READ);
    ADC2_status16 = SPI.transfer(0x00);
    ADC2_status8 = SPI.transfer(0x00);
    ADC2_status0 = SPI.transfer(0x00);
    ADC2_status24 = (ADC2_status16 << 16) | (ADC2_status8 << 8) | ADC2_status0;
    digitalWrite(adc2, HIGH);
    ADC2_status0 = ADC2_status0 >> 4;
    return (ADC2_status0);
}

unsigned char CheckStatus_ADC3(byte thisRegister)
{
    /* checks status of ADC 3*/
    digitalWrite(adc3, LOW); // reading
    SPI.transfer(thisRegister | READ);
    ADC3_status16 = SPI.transfer(0x00);
    ADC3_status8 = SPI.transfer(0x00);
    ADC3_status0 = SPI.transfer(0x00);
    ADC3_status24 = (ADC3_status16 << 16) | (ADC3_status8 << 8) | ADC3_status0;
    digitalWrite(adc3, HIGH);
    ADC3_status0 = ADC3_status0 >> 4;
    return (ADC3_status0);
}

void ReadResult_ADC1(byte thisRegister)
{
    /* reads result of ADC 1 */
    digitalWrite(adc1, LOW); // reading
    SPI.transfer(thisRegister | READ);
    ADC1_hi = SPI.transfer(0x00);
    ADC1_mid = SPI.transfer(0x00);
    ADC1_lo = SPI.transfer(0x00);
    digitalWrite(adc1, HIGH);
    if (i == false)
    {
        ADC1_read1 = (ADC1_hi << 16) | (ADC1_mid << 8) | ADC1_lo; // read 1 after set pulse
    }
    else
    {
        ADC1_read2 = (ADC1_hi << 16) | (ADC1_mid << 8) | ADC1_lo; // read 2 after reset pulse
        ADC1_result = ADC1_read2 - ADC1_read1;
        // Serial.print(ADC1_result);
        // Serial.print(",");
        dataMessage += String(ADC1_result) + ",";
        for (size_t i = 0; i < 3; i++)
        {
            dataBuffer[r] = lowByte(ADC1_result >> 8 * (2 - i));
            r++;
        }
    }
}

void ReadResult_ADC2(byte thisRegister)
{
    /* reads result of ADC 2 */
    digitalWrite(adc2, LOW); // reading
    SPI.transfer(thisRegister | READ);
    ADC2_hi = SPI.transfer(0x00);
    ADC2_mid = SPI.transfer(0x00);
    ADC2_lo = SPI.transfer(0x00);
    digitalWrite(adc2, HIGH);
    if (i == false)
    {
        ADC2_read1 = (ADC2_hi << 16) | (ADC2_mid << 8) | ADC2_lo; // read 1 after set pulse
    }
    else
    {
        ADC2_read2 = (ADC2_hi << 16) | (ADC2_mid << 8) | ADC2_lo; // read 2 after reset pulse
        ADC2_result = ADC2_read2 - ADC2_read1;                    // calculating average of the 2 reads
        // Serial.print(ADC2_result);
        // Serial.print(",");
        dataMessage += String(ADC2_result) + ",";
        for (size_t i = 0; i < 3; i++)
        {
            dataBuffer[r] = lowByte(ADC2_result >> 8 * (2 - i));
            r++;
        }
    }
}

void ReadResult_ADC3(byte thisRegister)
{
    /* reads result of ADC 3 */
    digitalWrite(adc3, LOW); // reading
    SPI.transfer(thisRegister | READ);
    ADC3_hi = SPI.transfer(0x00);
    ADC3_mid = SPI.transfer(0x00);
    ADC3_lo = SPI.transfer(0x00);
    digitalWrite(adc3, HIGH);
    if (i == false)
    {
        ADC3_read1 = (ADC3_hi << 16) | (ADC3_mid << 8) | ADC3_lo;
    }
    else
    {
        ADC3_read2 = (ADC3_hi << 16) | (ADC3_mid << 8) | ADC3_lo;
        ADC3_result = ADC3_read2 - ADC3_read1; // calculating average of the 2 reads
        Serial.println(ADC3_result);

        dataMessage += String(ADC3_result) + "\n";
        for (size_t i = 0; i < 3; i++)
        {
            dataBuffer[r] = lowByte(ADC3_result >> 8 * (2 - i));
            r++;
        }

        Serial.print(dataMessage);
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
        sum_x = sum_x + ADC1_result;
        count_x++;
        if (ADC1_result > max_x)
        {
            max_x = ADC1_result;
        }
        if (ADC1_result < min_x)
        {
            min_x = ADC1_result;
        }

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

void ADC_conv()
{
    /* during conversion, gets the accelero data */
    /* checks for drdy pin and reads the result */
    drdy1 = CheckStatus_ADC1(0x38); // function to check status register
    drdy2 = CheckStatus_ADC2(0x38); // function to check status register
    drdy3 = CheckStatus_ADC3(0x38); // function to check status register

    int time3 = millis();
    bool timeOut = false;

    while ((drdy1 != 1 || drdy2 != 1 || drdy3 != 1) & (millis() - time3 < 20))
    {
        drdy1 = CheckStatus_ADC1(0x38); // function to check status register
        drdy2 = CheckStatus_ADC2(0x38); // function to check status register
        drdy3 = CheckStatus_ADC3(0x38); // function to check status register
    }
    // Serial.printf("%d,%d,%d\n", drdy1, drdy2, drdy3);

    ReadResult_ADC1(result);
    ReadResult_ADC2(result);
    ReadResult_ADC3(result);
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
    }
    else
    {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1)
            delay(10);
    }
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

float measBatt()
{
    float battVolt = 0;
    float cellVolt;
    for (int battCount = 1; battCount <= 100; battCount++)
    {
        cellVolt = analogRead(34); // reading pin 9 to measure battery level
        if (cellVolt > battVolt)
        {
            battVolt = cellVolt;
        }
        delay(10);
    }
    battVolt = battVolt / 4096 * 3.3 * 2; // convert measure from 12bytes to volts
    return battVolt;
}

String fileName(String FileName)
{
    // sdSetup();

    String nowStr = String(startTime);
    String beginStr = nowStr.substring(0, 5);
    String endStr = nowStr.substring(5, 10);
    ////check if folder is in fact a file, following a false writing in sd card
    ////if the file is bad, it gets deleted (corresponds to about 15 min)
    if (SD.exists("/" + FileName + "/" + beginStr))
    { // first folder exists
        File folder = SD.open("/" + FileName + "/" + beginStr);
        if (folder.isDirectory())
        { // first folder is a directory
        }
        else
        { // if not directory, it gets deleted
            SD.remove("/" + FileName + "/" + beginStr);
        }

        if (SD.exists("/" + FileName + "/" + beginStr + "/" + endStr))
        { // scnd folder exists (cannot exist if first is deleted)
            File folder = SD.open("/" + FileName + "/" + beginStr + "/" + endStr);
            if (folder.isDirectory())
            { // scnd folder is directory
            }
            else
            { // if not directory, it gets deleted
                SD.remove("/" + FileName + "/" + beginStr + "/" + endStr);
            }
        }
    }

    if (!SD.exists("/" + FileName + "/" + beginStr + "/" + endStr))
    { // create new folder
        SD.mkdir("/" + FileName + "/");
        SD.mkdir("/" + FileName + "/" + beginStr);
        SD.mkdir("/" + FileName + "/" + beginStr + "/" + endStr);
    }
    return ("/" + FileName + "/" + beginStr + "/" + endStr + "/" + FileName + ".txt");
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
    // setup of SD card
    // sdSetup();
    // setup of ADCs
    ADCsetup();
    logTime = rtc.now().unixtime();
    randomSeed(logTime);

    // // digitalWrite(SDcard, LOW);
    String mess2Send = String(beginrotW);
    pinMode(ON_HW, INPUT); // battery measure pin too
    analogReadResolution(12);

    BattLvl = measBatt();
    mess2Send = mess2Send + "," + String(BattLvl);
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
    mess2Send = mess2Send + "," + String(avg_x) + "," + String(avg_y) + "," + String(avg_z) + "\n";
    // Serial.println(mess2Send);
    mess2Send = "";
    logDate = dateRTC();
    // Serial.println("stop");
    logTime = rtc.now().unixtime();
    Serial.println(mess2Send);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", "Hi! I am ESP32."); });
    server.on("/down", HTTP_GET, [](AsyncWebServerRequest *request)
              {
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

        request->send(LittleFS, downFile, "text/plain", true); });
    server.on("/rend", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/hw.bin", "text/plain", false); });
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", html, processor); });
    AsyncElegantOTA.begin(&server); // Start ElegantOTA

    server.begin();

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
