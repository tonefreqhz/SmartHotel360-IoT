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

#endif /* UTILITY_H */