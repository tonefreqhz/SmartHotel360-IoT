#ifndef UTILITY_H
#define UTILITY_H

void parseTwinMessage(DEVICE_TWIN_UPDATE_STATE, const char *);
bool createSensorMessagePayload(int messageId, float temperature, char *payload);

void SensorInit(void);
float readTemperature();
float readHumidity();

void blinkLED(void);
void blinkSendConfirmation(void);
int getInterval(void);

#endif /* UTILITY_H */