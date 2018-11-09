#ifndef UTILITY_H
#define UTILITY_H

char* initializeWiFi(void);

void initSensors(void);

float readTemperature();
bool readRoomOccupied();

void setDeviceLightLevel(int desiredLightLevel);

bool createSensorMessagePayload(int messageId, float temperature, bool motionDetected, char *payload);

int getInterval(void);

void showSendConfirmation(void);

bool sendPayloadToFunction(char *azureFunctionUri, char *content);

const char* getDTIoTHubConnectionString(char* hardwareId, char* sasToken);

char* httpTest();

#endif /* UTILITY_H */