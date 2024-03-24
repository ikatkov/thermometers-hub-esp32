#include <WiFiMulti.h>
#define DEVICE "ESP32"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include "ATC_MiThermometer.h"
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "build_version.h"
#include <credentials.h>

#define TZ_INFO "UTC-8"
#define MAX_BATCH_SIZE 2
#define WRITE_BUFFER_SIZE 4
#define WRITE_PRECISION WritePrecision::S
// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient influxDBClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// Declare Data point
Point measurementPoint("thermometer-v2");

JsonDocument versionJSON;
JsonDocument dataJSON;
#define BLE_SCAN_TIME_SEC 5 // BLE scan time in seconds

// List of known sensors' BLE addresses
std::vector<std::string> knownBLEAddresses = {"a4:c1:38:17:35:30", "a4:c1:38:47:00:1c", "a4:c1:38:52:31:ff"};

ATC_MiThermometer miThermometer(knownBLEAddresses);
AsyncWebServer server(80);
WiFiMulti wifiMulti;

unsigned long ota_progress_millis = 0;

void onOTAStart()
{
    // Log when OTA has started
    Serial.println("OTA update started!");
    // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final)
{
    // Log every 1 second
    if (millis() - ota_progress_millis > 1000)
    {
        ota_progress_millis = millis();
        Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
    }
}

void onOTAEnd(bool success)
{
    // Log when OTA has finished
    if (success)
    {
        Serial.println("OTA update finished successfully!");
    }
    else
    {
        Serial.println("There was an error during OTA update!");
    }
    // <Add your own code here>
}

void handle_get_root(AsyncWebServerRequest *request)
{
    Serial.println("Handling GET API request");
    JsonArray array = dataJSON.to<JsonArray>();
    for (int i = 0; i < miThermometer.data.size(); i++)
    {
        JsonObject nested = array.add<JsonObject>();
        nested["mac"] = knownBLEAddresses[i].c_str();
        nested["timestamp"] = miThermometer.data[i].timestamp;
        nested["temperature"] = miThermometer.data[i].temperature / 100.0;
        nested["humidity"] = miThermometer.data[i].humidity / 100.0;
        nested["batt_voltage"] = miThermometer.data[i].batt_voltage / 1000.0;
        nested["batt_level"] = miThermometer.data[i].batt_level;
        nested["rssi"] = miThermometer.data[i].rssi;
    }
    String json;
    serializeJson(dataJSON, json);
    Serial.println(json);
    request->send(200, "application/json", json);
}

void handle_get_version(AsyncWebServerRequest *request)
{
    Serial.println("Handling GET version API request");
    versionJSON["git_revision"] = GIT_REVISION;
    versionJSON["build_timestamp"] = BUILD_TIMESTAMP;
    String json;
    serializeJson(versionJSON, json);
    Serial.println(json);
    request->send(200, "application/json", json);
}

void handle_get_reboot(AsyncWebServerRequest *request)
{
    Serial.println("Handling GET Reboot API request");
    request->send(200);
    delay(1000);
    ESP.restart();
}

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(ssid, password);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (wifiMulti.run() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

    server.on("/", HTTP_GET, handle_get_root);
    server.on("/version", HTTP_GET, handle_get_version);
    server.on("/reboot", HTTP_GET, handle_get_reboot);

    ElegantOTA.begin(&server); // Start ElegantOTA
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);
    ElegantOTA.setAutoReboot(true);

    server.begin();
    Serial.println("HTTP server started");

    // Check InfluxDB server connection
    if (influxDBClient.validateConnection())
    {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(influxDBClient.getServerUrl());
    }
    else
    {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(influxDBClient.getLastErrorMessage());
    }
    // Increase buffer to allow caching of failed writes
    influxDBClient.setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION).batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE));

    // Initialization
    miThermometer.begin();
}

void loop()
{
    // If no Wifi signal, try to reconnect it
    if (wifiMulti.run() != WL_CONNECTED)
    {
        Serial.println("Wifi connection lost");
    }

    // Set sensor data invalid
    miThermometer.resetData();

    // Get sensor data - run BLE scan for <scanTime>
    unsigned found = miThermometer.getData(BLE_SCAN_TIME_SEC);

    for (int i = 0; i < miThermometer.data.size(); i++)
    {
        u64_t timestamp = time(nullptr);
        if (miThermometer.data[i].valid)
        {
            miThermometer.data[i].timestamp = timestamp;
            Serial.println();
            Serial.printf("Sensor %d: %s\n", i, knownBLEAddresses[i].c_str());
            Serial.printf("temperature %.1f°C\n", miThermometer.data[i].temperature / 100.0);
            Serial.printf("humidity %d%%\n", miThermometer.data[i].humidity / 100.0);
            Serial.printf("%.3fV\n", miThermometer.data[i].batt_voltage / 1000.0);
            Serial.printf("batt_level %d%%\n", miThermometer.data[i].batt_level);
            Serial.printf("rssi %ddBm\n", miThermometer.data[i].rssi);
            Serial.println();

            // Add tags to the data point
            measurementPoint.clearTags();
            measurementPoint.addTag("device", knownBLEAddresses[i].c_str());
            measurementPoint.clearFields();
            measurementPoint.setTime(miThermometer.data[i].timestamp);
            measurementPoint.addField("temperature", miThermometer.data[i].temperature / 100.0);
            measurementPoint.addField("humidity", miThermometer.data[i].humidity / 100.0);
            measurementPoint.addField("batt_voltage", miThermometer.data[i].batt_voltage / 1000.0);
            measurementPoint.addField("batt_level", miThermometer.data[i].batt_level);
            measurementPoint.addField("rssi", miThermometer.data[i].rssi);

            Serial.print("Write to buffer/server: ");
            Serial.println(measurementPoint.toLineProtocol());
            influxDBClient.writePoint(measurementPoint);
            if (!influxDBClient.writePoint(measurementPoint))
            {
                Serial.print("InfluxDB write failed: ");
                Serial.println(influxDBClient.getLastErrorMessage());
            }
        }
    }
    Serial.print("Devices found: ");
    Serial.println(found);
    Serial.println();

    // Delete results fromBLEScan buffer to release memory
    miThermometer.clearScanResults();
    delay(5000);
}
