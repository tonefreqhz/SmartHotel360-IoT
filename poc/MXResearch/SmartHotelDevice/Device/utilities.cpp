#include "AZ3166WiFi.h"
#include "HTS221Sensor.h"
#include "AzureIotHub.h"
#include "Arduino.h"
#include "parson.h"
#include "config.h"
#include "RGB_LED.h"
#include "Sensor.h"

#define RGB_LED_BRIGHTNESS 32

DevI2C *i2c;
HTS221Sensor *sensor;
static RGB_LED rgbLed;
static int interval = INTERVAL;
static float lastTemperature;
static boolean lastMotionDetected;

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
    sensor = new HTS221Sensor(*i2c);
    sensor->init(NULL);

    lastTemperature = -1000;
}

float readTemperature()
{
    sensor->reset();

    float temperature = 0;
    sensor->getTemperature(&temperature);

    return temperature;
}

float readMotion()
{
    sensor->reset();

    float temperature = 0;
    sensor->getTemperature(&temperature);

    return false;
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

void createSensorMessagePayload(int messageId, float temperature, boolean motionDetected, char *payload)
{
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    char* serialized_string = NULL;

    json_object_set_number(root_object, "messageId", messageId);

    if (temperature != lastTemperature)
    {
        lastTemperature = temperature;
        json_object_set_number(root_object, "temperature", lastTemperature);
    }

    if (motionDetected != lastMotionDetected)
    {
        lastMotionDetected = motionDetected;
        json_object_set_boolean(root_object, "motion", lastMotionDetected);
    }
    
    serialized_string = json_serialize_to_string_pretty(root_value);

    snprintf(payload, MESSAGE_MAX_LEN, "%s", serialized_string);
    json_free_serialized_string(serialized_string);
    json_value_free(root_value);
}
