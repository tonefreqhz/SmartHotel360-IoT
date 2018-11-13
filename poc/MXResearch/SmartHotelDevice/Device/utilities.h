#ifndef UTILITY_H
#define UTILITY_H

typedef struct
{
    const char* id;
    const char* dataType;
    const char* dataUnitType;
    const char* deviceId;
    int pollRate;
    const char* portType;
    const char* spaceId;
    const char* type;
} SensorInfo;

typedef struct
{
    const char* id;
    const char* connectionString;
    const SensorInfo* sensors;
    const char* friendlyName;
    const char* deviceType;
    const char* deviceSubtype;
    const char* hardwareId;
    const char* spaceId;
    const char* status;
} DeviceInfo;

char* initializeWiFi(void);

void initSensors(void);

float readTemperature();
bool readRoomOccupied();

void setDeviceLightLevel(int desiredLightLevel);

bool createSensorMessagePayload(int messageId, float temperature, bool motionDetected, char *payload);

int getInterval(void);

void showSendConfirmation(void);

const char* getDTIoTHubConnectionString(char* hardwareId, char* sasToken);

DeviceInfo* getDTIoTHubDeviceInfo(char* hardwareId, char* sasToken);

//bool sendPayloadToFunction(char *azureFunctionUri, char *content);

#endif /* UTILITY_H */