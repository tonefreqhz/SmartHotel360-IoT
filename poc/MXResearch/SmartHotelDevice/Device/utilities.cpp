#include "AZ3166WiFi.h"
#include "HTS221Sensor.h"
#include "AzureIotHub.h"
#include "Arduino.h"
#include "parson.h"
#include "config.h"
#include "RGB_LED.h"
#include "Sensor.h"
#include "http_client.h"
#include "utilities.h"

#define RGB_LED_BRIGHTNESS 32
#define LOOP_DELAY 1000
#define EXPECTED_COUNT 5
#define MAGNET_PRESENT_DELTA 250
#define MAX_UPLOAD_SIZE (64 * 1024)

//#define MANAGEMENT_BASE_URL "https://sh360iot-DigitalTwins-4yp3l2tudkncw.westcentralus.azuresmartspaces.net/management/api/v1.0/";
#define MANAGEMENT_BASE_URL "https://SmartHotelADT.westus2.azuresmartspaces.net/management/api/v1.0/"

#define DEVICES_INCLUDE_ARGUMENT "includes=Sensors,ConnectionString,Types,SensorsTypes"


DevI2C *i2c;
HTS221Sensor *tempSensor;
LIS2MDLSensor *magnetSensor;
int magnetAxes[3];
int magnetBaseX;
int magnetBaseY;
int magnetBaseZ;
bool lastMagnetStatus;
RGB_LED rgbLed;
int interval = INTERVAL;
float lastTemperatureSent;
bool lastRoomOccupied;
bool lastRoomOccupiedSent;



char* initializeWiFi()
{
  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    return ip.get_address();
  }
  return nullptr;
}

int getInterval()
{
    return interval;
}

void showSendConfirmation()
{
    pinMode(LED_USER, OUTPUT);
    DigitalOut LedUser(LED_BUILTIN);
    digitalWrite(LED_USER, 1);
    delay(500);
    digitalWrite(LED_USER, 0);
}

void initSensors()
{
    i2c = new DevI2C(D14, D15);

    // Initialize temperature sensor
    tempSensor = new HTS221Sensor(*i2c);
    tempSensor->init(NULL);

    lastTemperatureSent = -1000;
    lastRoomOccupied = false;
    lastRoomOccupiedSent = false;

    // Initialize magnetometer
    magnetSensor = new LIS2MDLSensor(*i2c);
    magnetSensor->init(NULL);
  
    magnetSensor->getMAxes(magnetAxes);
    magnetBaseX = magnetAxes[0];
    magnetBaseY = magnetAxes[1];
    magnetBaseZ = magnetAxes[2];
  
    int count = 0;
    int delta = 10;
    char buffer[20];
    while (true)
    {
        delay(LOOP_DELAY);
        magnetSensor->getMAxes(magnetAxes);
        
        // Waiting for the data from magnetSensor to become stable
        if (abs(magnetBaseX - magnetAxes[0]) < delta && abs(magnetBaseY - magnetAxes[1]) < delta && abs(magnetBaseZ - magnetAxes[2]) < delta)
        {
            count++;
            if (count >= EXPECTED_COUNT)
            {
                break;
            }
        }
        else
        {
            count = 0;
            magnetBaseX = magnetAxes[0];
            magnetBaseY = magnetAxes[1];
            magnetBaseZ = magnetAxes[2];
        }
    }
}

float readTemperature()
{
    tempSensor->reset();

    float temperature = 0;
    tempSensor->getTemperature(&temperature);

    return temperature;
}

bool readMagnetometerStatus()
{
    magnetSensor->getMAxes(magnetAxes);
    return (abs(magnetBaseX - magnetAxes[0]) > MAGNET_PRESENT_DELTA || abs(magnetBaseY - magnetAxes[1]) > MAGNET_PRESENT_DELTA || abs(magnetBaseZ - magnetAxes[2]) > MAGNET_PRESENT_DELTA);
}

bool readRoomOccupied()
{
    bool magnetStatus = readMagnetometerStatus();

    if (magnetStatus && magnetStatus != lastMagnetStatus)
    {
        lastRoomOccupied = !lastRoomOccupied;
    }

    lastMagnetStatus = magnetStatus;

    return lastRoomOccupied;
}

void setDeviceLightLevel(int lightLevel)
{
    if (lightLevel <=30)
    {
        rgbLed.turnOff();
        rgbLed.setColor(0, 0, RGB_LED_BRIGHTNESS);
    }
    else if (lightLevel <= 65)
    {
        rgbLed.turnOff();
        rgbLed.setColor(0, RGB_LED_BRIGHTNESS, 0);
    }
    else
    {
        rgbLed.turnOff();
        rgbLed.setColor(RGB_LED_BRIGHTNESS, 0, 0);
    }
}

bool createSensorMessagePayload(int messageId, float temperature, bool roomOccupied, char *payload)
{
    if (temperature == lastTemperatureSent && roomOccupied == lastRoomOccupiedSent)
    {
        return false;
    }
    
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    char* serialized_string = NULL;

    json_object_set_number(root_object, "messageId", messageId);

    if (temperature != lastTemperatureSent)
    {
        lastTemperatureSent = temperature;
        json_object_set_number(root_object, "temperature", lastTemperatureSent);
    }

    if (roomOccupied != lastRoomOccupiedSent)
    {
        lastRoomOccupiedSent = roomOccupied;
        json_object_set_boolean(root_object, "motion", lastRoomOccupiedSent);
    }
    
    serialized_string = json_serialize_to_string_pretty(root_value);

    snprintf(payload, MESSAGE_MAX_LEN, "%s", serialized_string);
    json_free_serialized_string(serialized_string);
    json_value_free(root_value);

    return true;
}

const char* getDTIoTHubConnectionString(char* hardwareId, char* sasToken)
{
    char azureFunctionUri[256];
    sprintf(azureFunctionUri, "%sDevices?hardwareIds=%s&%s", MANAGEMENT_BASE_URL, hardwareId, DEVICES_INCLUDE_ARGUMENT);

    HTTPClient *httpClient = new HTTPClient(HTTP_GET, azureFunctionUri);
    httpClient->set_header("Authorization", sasToken);
    const Http_Response* result = httpClient->send();

    char outputString[512];

    char* status = (char *)malloc(20);

    JSON_Value* root_value = json_parse_string(result->body);
    JSON_Array* devices = json_value_get_array(root_value);
    if (json_array_get_count(devices) != 1)
    {
        return nullptr;
    }
    JSON_Object* device = json_array_get_object(devices, 0);
    const char* connectionString = json_object_get_string(device, "connectionString");

    delete httpClient;

    return connectionString;
}

DeviceInfo* getDTIoTHubDeviceInfo(char* hardwareId, char* sasToken)
{
    char azureFunctionUri[256];
    sprintf(azureFunctionUri, "%sDevices?hardwareIds=%s&%s", MANAGEMENT_BASE_URL, hardwareId, DEVICES_INCLUDE_ARGUMENT);

    HTTPClient *httpClient = new HTTPClient(HTTP_GET, azureFunctionUri);
    httpClient->set_header("Authorization", sasToken);
    const Http_Response* result = httpClient->send();

    char outputString[512];

    char* status = (char *)malloc(20);

    JSON_Value* root_value = json_parse_string(result->body);
    JSON_Array* devices = json_value_get_array(root_value);
    if (json_array_get_count(devices) != 1)
    {
        return nullptr;
    }

    DeviceInfo* deviceInfo = new DeviceInfo;

    JSON_Object* device = json_array_get_object(devices, 0);
    deviceInfo->id = json_object_get_string(device, "id");
    deviceInfo->connectionString = json_object_get_string(device, "connectionString");
    deviceInfo->friendlyName = json_object_get_string(device, "friendlyName");
    deviceInfo->deviceType = json_object_get_string(device, "deviceType");
    deviceInfo->deviceSubtype = json_object_get_string(device, "deviceSubtype");
    deviceInfo->hardwareId = json_object_get_string(device, "hardwareId");
    deviceInfo->spaceId = json_object_get_string(device, "spaceId");
    deviceInfo->status = json_object_get_string(device, "status");


    JSON_Array* sensors = json_object_get_array(device, "sensors");
    for (int sensorIndex=0; sensorIndex<json_array_get_count(sensors); sensorIndex++)
    {
        SensorInfo* sensorInfo = new SensorInfo;

        JSON_Object* sensor = json_array_get_object(devices, sensorIndex);
        sensorInfo->id = json_object_get_string(sensor, "id");
    }

    Screen.print(0, "Works!");


    delete httpClient;

    return deviceInfo;
}

//bool sendPayloadToFunction(char *azureFunctionUri, char *content)
//{
//    int length = strlen(content) + 1;
//
//    if (content == NULL || length <= 0 || length > MAX_UPLOAD_SIZE)
//    {
//    Serial.println("Content not valid");
//    return false;
//    }
//
//    HTTPClient client = HTTPClient(HTTP_POST, azureFunctionUri);
//    const Http_Response *response = client.send(content, length);
//
//    return (response != NULL && response->status_code == 200);
//}
