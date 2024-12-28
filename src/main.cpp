
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <NTPClient.h>

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "wifiConfig.h"

#define TEMPERATURE_PRECISION 10
#define NUM_DEVS 20
#define BOTTOM_TEMP_OFFSET 15
#define TEMP_FACTOR 5
#define AVG_BUFFERS 1440 // 1 day @ 1 minute-cycle

#define INTERVALL 100
#define CYCLES_100MS (6 * INTERVALL)

//int LED_BUILTIN = 2;

const char compile_date[] = __DATE__ " " __TIME__;

// https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/

const long utcOffsetInSeconds = 3600;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

int8_t rssi = 0;
uint16_t avgIndex = 0;
uint16_t avgFilled = 0;
uint8_t temperatures[NUM_DEVS];
uint8_t prev_temperatures[NUM_DEVS];
uint8_t max_temperatures[NUM_DEVS];
uint8_t min_temperatures[NUM_DEVS];
uint8_t store_temperatures[NUM_DEVS];
uint8_t avg_temperatures_buffer[NUM_DEVS][AVG_BUFFERS];
uint8_t avg_temperatures[NUM_DEVS];
DeviceAddress addresses[NUM_DEVS];

String lastReadoutTime;
byte trend[NUM_DEVS];
uint8_t wifiRestarts;

String currentChartData;
String currentChartDataY;
String currentChartDev;

char *myStrings[] = {
    "Flur  V",
    "Flur  R",
    "Diele V",
    "Diele R",
    "Gaest V",
    "Gaest R",
    "AZ Sy V",
    "AZ Sy R",
    "Schlf V",
    "Schlf R",
    "WZ No V",
    "WZ No R",
    "WZ Su V",
    "WZ Su R",
    "Kuech V",
    "Kuech R",
    "Bad   V",
    "Bad   R",
    "HWR   V",
    "HWR   R",
};

char *trendSymbols[] = {
    "",        // "&#8594;", // same
    "&#8593;", // up
    "&#8595;", // down
};

char *trendColors[] = {
    "#FFFFFF\">", // "&#8594;", // same
    "#FF5555\">", // up
    "#5555FF\">", // down
};

WebServer server(80);
WiFiClient espClient;

uint8_t NUM_OF_DEVICES;

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
  Serial.println("");
}

String PrintTemperature(uint8_t temperature)
{
  return String(temperature / ((float)TEMP_FACTOR) + BOTTOM_TEMP_OFFSET, 1);
}

uint8_t CompressTemperature(float temperatur)
{
  return (int)((temperatur - BOTTOM_TEMP_OFFSET) * TEMP_FACTOR);
}

bool ntpSuccess = false;
String PrintTime()
{
  return "bla";
  Serial.println("PrintTime");
  if (!ntpSuccess)
  {
    Serial.println("PrintTime1");
    ntpSuccess = timeClient.update();
    Serial.println("PrintTime1b");
    Serial.print("timeClient.update success=");
    Serial.println(ntpSuccess ? "TRUE" : "FALSE");
  }
  Serial.println("PrintTime2");
  return timeClient.getFormattedTime();
}

String SendHTML()
{
  String ptr = "<!DOCTYPE html> <html>";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">";
  // ptr += "<meta http-equiv=\"refresh\" content=\"10\">";

  /*
  ptr += ("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
  ptr += (".button { background-color: #333344; border: none; color: white; padding: 10px 24px;");
  ptr += ("text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;}");
  ptr += (".button2 {background-color: #888899;}</style></head>");
  */

  ptr += "<script>";
  ptr += "function toggleCheckbox(x) {";
  // ptr += "alert(\"kommt\");";
  ptr += "var xhr = new XMLHttpRequest();";
  ptr += "xhr.open(\"GET\", \"/\" + x , true);";
  ptr += "xhr.send();";
  ptr += "setTimeout(function(){ window.location.reload(\"Refresh\");}, 1000);";
  ptr += "}";

  ptr += "function ShowHideDiv(chkbox) {";
  ptr += "var display = chkbox.checked ? \"block\" : \"none\";";
  ptr += "alert(\"kommt\" + display);";

  ptr += "setTimeout(function(){ window.location.reload(\"Refresh\");}, 1000);";
  ptr += "}";
  ptr += "</script>";

  ptr += "<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>";

  ptr += "<title>FBH-Monitor</title><body  font-size: 14px; style=\"font-family: sans-serif\">";

  if (NUM_OF_DEVICES != 20)
  {
    ptr += "<p>! No. of devices: ";
    ptr += NUM_OF_DEVICES;
  }
  ptr += "<table border=\"1\"><tr><th>Sensor</th><th>Value</th><th>Tr.</th><th>Min</th><th>Max</th><th>Store</th><th>Avg</th></tr>";
  for (int i = 0; i < 20; i++)
  {
    ptr += "<tr ";
    ptr += (i % 4 < 2) ? "bgcolor= #EEEEEE" : "bgcolor= #FFFFFF";
    ptr += "><td>";
    ptr += myStrings[i];
    ptr += "</td><td style=\"background: ";
    ptr += trendColors[trend[i]];
    ptr += PrintTemperature(temperatures[i]);
    ptr += "</td><td>";
    ptr += trendSymbols[trend[i]];
    ptr += "</td><td>";
    ptr += PrintTemperature(min_temperatures[i]);
    ptr += "</td><td>";
    ptr += PrintTemperature(max_temperatures[i]);
    ptr += "</td><td>";
    ptr += PrintTemperature(store_temperatures[i]);
    ptr += "</td><td>";
    ptr += PrintTemperature(avg_temperatures[i]);
    ptr += "</td><td>";
    ptr += "<button onmouseup=\"toggleCheckbox('SetChart";
    ptr += i;
    ptr += "');\" class=\"button\" ontouchend=\"toggleCheckbox('SetChart";
    ptr += i;
    ptr += "');\">View</button>";
    ptr += "</td><td>";
    ptr += " <input type=\"checkbox\" id=\"asd\" onclick=\"ShowHideDiv(this)\">  ";
    ptr += "</td></tr>";
  }

  ptr += "</table>";
  ptr += "<br/>";
  ptr += "<button onmouseup=\"toggleCheckbox('ResetMinMax');\" class=\"button\" ontouchend=\"toggleCheckbox('ResetMinMax');\">Reset Min/Max</button>";
  ptr += "<button onmouseup=\"toggleCheckbox('Store');\" class=\"button\" ontouchend=\"toggleCheckbox('Store');\">Speichern</button>";
  ptr += "<button onmouseup=\"toggleCheckbox('ResetAvg');\" class=\"button\" ontouchend=\"toggleCheckbox('Store');\">Reset Avg</button>";

  ptr += "<div>  <canvas id=\"myChart\"></canvas></div>";
  ptr += "<br/>WIfI: ";
  ptr += rssi;
  ptr += " dB";
  ptr += "<br/>Current Time: ";
  ptr += PrintTime();
  ptr += "<br/>Last Readout: ";
  ptr += lastReadoutTime;
  ptr += "<br/>Cycle seconds: ";
  ptr += CYCLES_100MS / 10;
  ptr += "<br/>Build: ";
  ptr += compile_date;
  ptr += "<br/>Values in AVG buffer: ";
  ptr += avgFilled;
  ptr += "<br/>Avg length: ";
  ptr += String(CYCLES_100MS * avgFilled / 600.0, 1);
  ptr += " minutes<br/>Wifi restarts: ";
  ptr += wifiRestarts;
  /*
    ptr += "<br/>currentChartData: ";
    ptr += currentChartData;

    ptr += "<br/>currentChartDataY: ";
    ptr += currentChartDataY;
  */
  ptr += "</body></html>";

  ptr += "<script>const ctx = document.getElementById('myChart');"
         "new Chart(ctx, {"
         "  type: 'line',"
         "  data: {"
         "    labels: [" +
         currentChartDataY + "],"
                             //"    labels: ['24:05:11','12:12:12','6:13:43'],"
                             "    datasets: [{"
                             "    label: '" +
         currentChartDev + "',"
                           "    data: [" +
         currentChartData + "],"
                            "    borderWidth: 1"
                            "    }]"
                            "  },"
                            "  options: {"
                            "    scales: {"

                            "      y: {"
                            "        beginAtZero: true"
                            "         }"
                            "       }"
                            "    }"
                            "  });</script>";
  return ptr;
}

void handleRoot()
{
  Serial.print("Handle root");
  server.send(200, "text/html", SendHTML());
}

void handleNotFound()
{
  Serial.print("Handle Not-Found");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// https://randomnerdtutorials.com/esp8266-ds18b20-temperature-sensor-web-server-with-arduino-ide/
//  GPIO where the DS18B20 is connected to
const int oneWireBus = 27;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
// DeviceAddress tempDeviceAddress;

void SetupOneWire()
{

  sensors.begin(); // 2 x
  sensors.begin(); // https://github.com/milesburton/Arduino-Temperature-Control-Library/issues/85
  NUM_OF_DEVICES = sensors.getDeviceCount();
  Serial.print("numberOfDevices: ");
  Serial.println(NUM_OF_DEVICES, DEC);

  for (int i = 0; i < NUM_OF_DEVICES; i++)
  {
    //**(addresses[i]) = (uint8_t[8] *) malloc(sizeof(DeviceAddress));
    digitalWrite(LED_BUILTIN, (i % 2) == 1 ? LOW : HIGH);
    // Search the wire for address
    if (sensors.getAddress(addresses[i], i))
    {

      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(addresses[i]);
      sensors.setResolution(addresses[i], TEMPERATURE_PRECISION);
    }
    else
    {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
}

String TwoDigitPrint(int number)
{
  String r = "";
  r += (number < 10 ? "0" : "");
  r += number;
  return r;
}

String EpochToString(long epoch)
{
  String r = "'";
  r += TwoDigitPrint(epoch % 86400 / 3600);
  r += ":";
  r += TwoDigitPrint((epoch % 3600) / 60);
  r += ":";
  r += TwoDigitPrint(epoch % 60);
  r += "'";
  return r;
}

void SetChartDataForDev(uint8_t dev)
{
  String temp = "";
  String tempX = "";

  auto epoch = timeClient.getEpochTime() - avgFilled * CYCLES_100MS / 10;
  auto readPtr = avgFilled < AVG_BUFFERS ? 0 : avgIndex % AVG_BUFFERS;

  for (uint16_t count = 0; count < avgFilled; count++)
  {
    temp += PrintTemperature(avg_temperatures_buffer[dev][readPtr]);
    temp += ",";

    epoch += CYCLES_100MS / 10;
    tempX += EpochToString(epoch);
    tempX += ",";

    if (++readPtr > AVG_BUFFERS - 1)
      readPtr = 0;
  }

  temp += PrintTemperature(avg_temperatures_buffer[dev][readPtr]);
  currentChartData = temp;
  epoch += CYCLES_100MS / 10;
  tempX += EpochToString(epoch);
  currentChartDataY = tempX;
  currentChartDev = myStrings[dev];
}

#define DEBUG_PRINT 0

uint8_t CalculateAverage(uint8_t *array)
{
  uint32_t sum = 0;

  if (avgIndex > avgFilled)
    avgFilled = avgIndex;

  for (int i = 0; i < avgFilled + 1; i++)
    sum += array[i];
  if (DEBUG_PRINT)
  {
    Serial.print("Sum: ");
    Serial.println(sum);
    Serial.print("Avg: ");
    Serial.println(PrintTemperature(sum / (avgFilled + 1)));
  }
  return sum / (avgFilled + 1);
}

void ReadAllTempSensors()
{
  return;
  Serial.println("ReadAllTempSensors");

  sensors.requestTemperatures();
  lastReadoutTime = PrintTime();

  for (int i = 0; i < NUM_OF_DEVICES; i++)
  {
    Serial.print("Read ");
    Serial.println(i);

    prev_temperatures[i] = temperatures[i];
    // Output the device ID
    if (DEBUG_PRINT)
    {
      Serial.print(i, DEC);
      Serial.print(": ");
    }

    uint8_t tempC = CompressTemperature(sensors.getTempC(addresses[i]));
    temperatures[i] = tempC;

    if (DEBUG_PRINT)
      Serial.print(PrintTemperature(tempC));

    if (tempC < min_temperatures[i])
      min_temperatures[i] = tempC;

    if (tempC > max_temperatures[i])
      max_temperatures[i] = tempC;

    if (tempC > prev_temperatures[i] + 1)
    {
      trend[i] = 1;
      if (DEBUG_PRINT)
        Serial.println(" ↑");
    }
    else if (tempC < prev_temperatures[i] - 1)
    {
      trend[i] = 2;
      if (DEBUG_PRINT)
        Serial.println(" ↓");
    }
    else
    {
      trend[i] = 0;
      if (DEBUG_PRINT)
        Serial.println(" -");
    }
    avg_temperatures_buffer[i][avgIndex] = tempC;
  }
  if (DEBUG_PRINT)
  {
    Serial.print("calling CalculateAverage with: avgFilled=");
    Serial.print(avgFilled);
    Serial.print(" / avgIndex=");
    Serial.println(avgIndex);
  }
  for (int i = 0; i < NUM_DEVS; i++)
    avg_temperatures[i] = CalculateAverage(avg_temperatures_buffer[i]);

  if (++avgIndex > AVG_BUFFERS)
    avgIndex = 0;
}

// WIFI RECONNECT
unsigned long previous_time = 0;
unsigned long wifiTimeoutDelay = 120000; // 120 seconds delay
byte blinkPattern = 0xAA;
void CheckWifiConnection()
{
  unsigned long current_time = millis(); // number of milliseconds since the upload

  if ((WiFi.status() != WL_CONNECTED) && (current_time - previous_time >= wifiTimeoutDelay))
  {
    blinkPattern = 0x01;
    Serial.print(millis());
    Serial.println("Reconnecting to WIFI network");
    // WiFi.disconnect();
    WiFi.reconnect();
    wifiRestarts++;
  }
  else
    blinkPattern = 0xFE;
  previous_time = current_time;
}

int tempTask = 0;
int checkWifiTask = 0;
int blinkTask = 0;
byte bitShift = 0;
char myHex[10] = "";
unsigned long previousMillis = 0;
hw_timer_t *My_timer = NULL;

void IRAM_ATTR myLoopFunc()
{
  Serial.println("myLoopFunc");
  if (--tempTask < 1)
  {
    Serial.println("myLoopFunc1");
    tempTask = CYCLES_100MS; // 600 * 100ms = 60 sec

    ReadAllTempSensors();
    Serial.println("myLoopFunc1b");
    PrintTime();
    Serial.println("myLoopFunc1c");
    // rssi = WiFi.RSSI();
  }
  if (--blinkTask < 1)
  {
    Serial.println("myLoopFunc2");
    blinkTask = 5; // 5 * 100ms = 0.5 sec
  }
  if (--checkWifiTask < 50)
  {
    Serial.println("myLoopFunc3");
    checkWifiTask = 5; // 5 * 100ms = 0.5 sec
    // CheckWifiConnection();
  }
  Serial.println("myLoopFunc4");
  digitalWrite(LED_BUILTIN, ((blinkPattern >> bitShift) & 0x01) ? HIGH : LOW);
  if (++bitShift > 7)
    bitShift = 0;
  Serial.println("myLoopFunc5");
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  Serial.println("START");
  Serial.println("Setting Wifi mode..");

  for (int i = 0; i < NUM_DEVS; i++)
    min_temperatures[i] = 0xFF;

  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Waiting for Wifi connection..");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("timeClient.begin()....");
  timeClient.begin();
  digitalWrite(LED_BUILTIN, LOW);
  SetupOneWire();

  if (MDNS.begin("esp32"))
  {
    Serial.println("MDNS responder started");
    server.on("/", handleRoot);
    server.onNotFound(handleNotFound);

    server.on("/ResetMinMax", []()
              {
                Serial.print("ResetMinMax");
                for (int i =0;i< NUM_DEVS;i++)
                min_temperatures[i] = max_temperatures[i] = temperatures[i]; });

    server.on("/Store", []()
              {
                Serial.print("Store");
                for (int i =0;i< NUM_DEVS;i++)
                store_temperatures[i] = temperatures[i] ; });

    server.on("/ResetAvg", []()
              {                Serial.print("ResetAvg");                
                avgIndex= avgFilled = 0; });

    server.on("/SetChart0", []()
              { SetChartDataForDev(0); });
    server.on("/SetChart1", []()
              { SetChartDataForDev(1); });
    server.on("/SetChart2", []()
              { SetChartDataForDev(2); });
    server.on("/SetChart3", []()
              { SetChartDataForDev(3); });
    server.on("/SetChart4", []()
              { SetChartDataForDev(4); });
    server.on("/SetChart5", []()
              { SetChartDataForDev(5); });
    server.on("/SetChart6", []()
              { SetChartDataForDev(6); });
    server.on("/SetChart7", []()
              { SetChartDataForDev(7); });
    server.on("/SetChart8", []()
              { SetChartDataForDev(8); });
    server.on("/SetChart9", []()
              { SetChartDataForDev(9); });
    server.on("/SetChart10", []()
              { SetChartDataForDev(10); });
    server.on("/SetChart11", []()
              { SetChartDataForDev(11); });
    server.on("/SetChart12", []()
              { SetChartDataForDev(12); });
    server.on("/SetChart13", []()
              { SetChartDataForDev(13); });
    server.on("/SetChart14", []()
              { SetChartDataForDev(14); });
    server.on("/SetChart15", []()
              { SetChartDataForDev(15); });
    server.on("/SetChart16", []()
              { SetChartDataForDev(16); });
    server.on("/SetChart17", []()
              { SetChartDataForDev(17); });
    server.on("/SetChart18", []()
              { SetChartDataForDev(18); });
    server.on("/SetChart19", []()
              { SetChartDataForDev(19); });

    server.begin();
    Serial.println("HTTP server started");

    ArduinoOTA.onStart([]()
                       {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
        .onEnd([]()
               { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error)
                 {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

    ArduinoOTA.begin();

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    My_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(My_timer, &myLoopFunc, true);
    timerAlarmWrite(My_timer, 1000000, true);
    timerAlarmEnable(My_timer);
  }
  else
    Serial.println("MDNS responder NOT started!!!!!");
}

void loop()
{
  server.handleClient();
  ArduinoOTA.handle();
}
