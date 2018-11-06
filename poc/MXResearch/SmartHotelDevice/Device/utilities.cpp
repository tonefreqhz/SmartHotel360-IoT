#include "AZ3166WiFi.h"
#include "HTS221Sensor.h"
#include "AzureIotHub.h"
#include "Arduino.h"
#include "parson.h"
#include "config.h"
#include "RGB_LED.h"
#include "Sensor.h"

#define RGB_LED_BRIGHTNESS 32
#define LOOP_DELAY 1000
#define EXPECTED_COUNT 5
#define MAGNET_PRESENT_DELTA 400


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

void createSensorMessagePayload(int messageId, float temperature, bool roomOccupied, char *payload)
{
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
}
