#include "AZ3166WiFi.h"
#include "HTS221Sensor.h"
#include "AzureIotHub.h"
#include "Arduino.h"
#include "parson.h"
#include "config.h"
#include "RGB_LED.h"
#include "Sensor.h"
#include "http_client.h"

#define RGB_LED_BRIGHTNESS 32
#define LOOP_DELAY 1000
#define EXPECTED_COUNT 5
#define MAGNET_PRESENT_DELTA 250
#define MAX_UPLOAD_SIZE (64 * 1024)


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

bool sendPayloadToFunction(char *azureFunctionUri, char *content)
{
    int length = strlen(content);

    if (content == NULL || length <= 0 || length > MAX_UPLOAD_SIZE)
    {
    Serial.println("Content not valid");
    return false;
    }

    HTTPClient client = HTTPClient(HTTP_POST, azureFunctionUri);
    const Http_Response *response = client.send(content, length);

    return (response != NULL && response->status_code == 200);
}

const char SSL_CA_PEM[] = "-----BEGIN CERTIFICATE-----\n"
                          "MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n"
                          "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
                          "DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n"
                          "SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n"
                          "GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n"
                          "AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n"
                          "q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n"
                          "SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n"
                          "Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n"
                          "a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n"
                          "/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n"
                          "AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n"
                          "CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n"
                          "bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n"
                          "c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n"
                          "VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n"
                          "ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n"
                          "MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n"
                          "Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n"
                          "AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n"
                          "uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n"
                          "wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n"
                          "X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n"
                          "PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n"
                          "KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n"
                          "-----END CERTIFICATE-----\n";

char* httpTest()
{
    HTTPClient *httpClient = new HTTPClient(SSL_CA_PEM, HTTP_GET, "https://httpbin.org/status/418");
    const Http_Response* result = httpClient->send();

    char* status = (char *)malloc(20);

    if (result == NULL)
    {
        sprintf(status, "Error: %s", httpClient->get_error());
    }
    else
    {
        sprintf(status, "%s", result->status_message);
    }

    delete httpClient;

    return status;
}