#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "DevKitMQTTClient.h"

#include "config.h"
#include "utilities.h"
#include "SystemTickCounter.h"

static bool connectedToWiFi = false;
int messageCount = 1;
static bool sendModeIsActive = true;
static uint64_t sendIntervalInMs;
static char outputString[20];
static float desiredTempFahrenheit;
static int lightLevel;
static int desiredLightLevel;
//static char azureFunctionUri[128];
static char* dTIoTHubConnectionString;

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    showSendConfirmation();
  }
}

static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  float argument = 0;

  if (size > 0)
  {
    char *temp = (char *)malloc(size + 1);
    if (temp == NULL)
    {
      return result;
    }
    memcpy(temp, payload, size);
    temp[size] = '\0';
    argument = atof(temp);
    free(temp);
  }

  if (strcmp(methodName, "StartDeviceFeed") == 0)
  {
    LogInfo("Start sending sensor data");
    sendModeIsActive = true;
  }
  else if (strcmp(methodName, "StopDeviceFeed") == 0)
  {
    LogInfo("Stop sending sensor data");
    sendModeIsActive = false;
  }
  else if (strcmp(methodName, "SetDesiredAmbientLight") == 0)
  {
    desiredLightLevel = argument*100.0f;
    if (desiredLightLevel < 0)
    {
      desiredLightLevel = 0;
    }
    else if (desiredLightLevel > 100)
    {
      desiredLightLevel = 100;
    }
    LogInfo("Set desired ambient light to: '%d'", desiredLightLevel);
    lightLevel = desiredLightLevel;
  }
  else if (strcmp(methodName, "SetDesiredTemperature") == 0)
  {
    desiredTempFahrenheit = argument;
    LogInfo("Set desired temperature to: '%f' F", desiredTempFahrenheit);
  }
  else if (strcmp(methodName, "SetFeedback") == 0)
  {
    sprintf(outputString, "F: %.1f", argument);
    Screen.print(0, outputString);
  }
  else
  {
    LogInfo("No method with the name '%s' found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}

void setup()
{
  Screen.init();
  Screen.print(0, "SmartHotel IoT");
  Screen.print(1, "Initializing...");
  
  Screen.print(2, " > Serial");
  Serial.begin(115200);

  Screen.print(2, " > WiFi");
  Screen.print(3, "Connecting...");
  connectedToWiFi = false;
  char* wifiAddress = initializeWiFi();
  if (wifiAddress == nullptr)
  {
    connectedToWiFi = false;
    Screen.print(3, "No Wi-Fi\r\n ");
    return;
  }
  sprintf(outputString, "%s", wifiAddress);
  connectedToWiFi = true;
  Screen.print(3, "Connected\r\n");
  LogTrace("Connected to WiFI", NULL);

  Screen.print(2, " > Sensors");
  initSensors();

  Screen.print(2, " > IoT Hub");
  DevKitMQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "SmartHotelDevice");
  DevKitMQTTClient_Init(true);

  DevKitMQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  DevKitMQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);

  sendIntervalInMs = SystemTickCounterRead();

  //sprintf(azureFunctionUri, "http://%s.azurewebsites.net/api/SmartHotelFunction", (char *)AZURE_FUNCTION_APP_NAME);

  const char* connectionString = getDTIoTHubConnectionString(HARDWARE_ID, SAS_TOKEN);
  dTIoTHubConnectionString = (char *)malloc(strlen(connectionString) + 1);
  sprintf(dTIoTHubConnectionString, "%s", connectionString);

  Screen.print(1, outputString);
  Screen.print(2, "Ready");

  desiredTempFahrenheit = 70.0f;

  desiredLightLevel = 85;
  lightLevel = desiredLightLevel;

  delay(1000);
}

void loop()
{
  if (connectedToWiFi)
  {
    if (sendModeIsActive)
    {
      if ((int)(SystemTickCounterRead() - sendIntervalInMs) >= getInterval())
      {
        char messagePayload[MESSAGE_MAX_LEN];

        float tempCelsius = readTemperature();
        float tempFahrenheit = (tempCelsius * 1.8f) + 32.0f;

        sprintf(outputString, "T- C:%.1f D:%.1f", tempFahrenheit, desiredTempFahrenheit);
        Screen.print(1, outputString);

        sprintf(outputString, "L- C:%d D:%d", lightLevel, desiredLightLevel);
        Screen.print(2, outputString);
        setDeviceLightLevel(lightLevel);

        bool roomOccupied = readRoomOccupied();

        sprintf(outputString, "%s", roomOccupied ? "Occupied" : "Vacant");
        Screen.print(3, outputString);

        if (createSensorMessagePayload(messageCount++, tempFahrenheit, roomOccupied, messagePayload))
        {
          EVENT_INSTANCE* message = DevKitMQTTClient_Event_Generate(messagePayload, MESSAGE);
          DevKitMQTTClient_SendEventInstance(message);

          //if (sendPayloadToFunction(azureFunctionUri, messagePayload))
          //{
          //  sprintf(outputString, "Success");
          //  Screen.print(3, outputString);
          //}
          //else
          //{
          //  sprintf(outputString, "Failure");
          //  Screen.print(3, outputString);
          //}
        }
        
        sendIntervalInMs = SystemTickCounterRead();
      }
    }
    else
    {
      DevKitMQTTClient_Check();
      Screen.print(3, "Idle");
    }
  }
  delay(1000);
}
