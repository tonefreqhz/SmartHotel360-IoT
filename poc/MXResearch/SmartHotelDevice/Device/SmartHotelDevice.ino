#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "CustomMQTTClient.h"

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
static char* dtIotHubConnectionString;
static int iotHubMessageCount = 0;
//static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
//static IOTHUB_CLIENT_HANDLE iotHubClientHandle;

static CustomMQTTClient* customMQTTClient;

static void SHSendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    showSendConfirmation();
  }
}

//static void SendConfirmationCallback2(IOTHUB_CLIENT_CONFIRMATION_RESULT_TAG result, void* dummy)
//{
//  Screen.print(0, "Success!");
//  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
//  {
//    showSendConfirmation();
//  }
//}

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

  const char* connectionString = getDTIoTHubConnectionString(HARDWARE_ID, SAS_TOKEN);
  dtIotHubConnectionString = (char *)malloc(strlen(connectionString) + 1);
  sprintf(dtIotHubConnectionString, "%s", connectionString);


  // Load connection from EEPROM
  //EEPROMInterface eeprom;
  //uint8_t connString[AZ_IOT_HUB_MAX_LEN + 1] = {'\0'};
  //int ret = eeprom.read(connString, AZ_IOT_HUB_MAX_LEN, 0x00, AZ_IOT_HUB_ZONE_IDX);
  //if (ret < 0)
  //{
  //    LogError("Unable to get the azure iot connection string from EEPROM. Please set the value in configuration mode.");
  //    return;
  //}
  //else if (ret == 0)
  //{
  //    LogError("The connection string is empty.\r\nPlease set the value in configuration mode.");
  //    return;
  //}
  //dtIotHubConnectionString = (char *)malloc(AZ_IOT_HUB_MAX_LEN + 1);
  //sprintf(dtIotHubConnectionString, "%s", connString);



  customMQTTClient = new CustomMQTTClient(dtIotHubConnectionString, true, false);
  customMQTTClient->CustomMQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "SmartHotelDevice");

  customMQTTClient->CustomMQTTClient_SetSendConfirmationCallback(SHSendConfirmationCallback);
  customMQTTClient->CustomMQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);

  sendIntervalInMs = SystemTickCounterRead();


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

        sprintf(outputString, "L- C:%d%% D:%d%%", lightLevel, desiredLightLevel);
        Screen.print(2, outputString);
        setDeviceLightLevel(lightLevel);

        bool roomOccupied = readRoomOccupied();

        sprintf(outputString, "%s", roomOccupied ? "Occupied" : "Vacant");
        Screen.print(3, outputString);

        if (createSensorMessagePayload(messageCount++, tempFahrenheit, roomOccupied, messagePayload))
        {
          EVENT_INSTANCE* message = customMQTTClient->CustomMQTTClient_Event_Generate(messagePayload, MESSAGE);
          customMQTTClient->CustomMQTTClient_SendEventInstance(message);
        }
        
        sendIntervalInMs = SystemTickCounterRead();
      }
    }
    else
    {
      customMQTTClient->CustomMQTTClient_Check(true);
      Screen.print(3, "Idle");
    }
  }
  delay(1000);
}
