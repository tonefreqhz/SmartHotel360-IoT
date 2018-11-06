#ifndef UTILITY_H
#define UTILITY_H

char* initializeWiFi(void);

void initSensors(void);

float readTemperature();
boolean readMotion();

void setDeviceLightLevel(int desiredLightLevel);

bool createSensorMessagePayload(int messageId, float temperature, boolean motionDetected, char *payload);

int getInterval(void);

void showSendConfirmation(void);

#endif /* UTILITY_H */