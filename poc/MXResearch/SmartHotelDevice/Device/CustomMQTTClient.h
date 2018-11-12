// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.

#ifndef __CUSTOM_MQTT_CLIENT_H__
#define __CUSTOM_MQTT_CLIENT_H__

#include "AzureIotHub.h"

#include "EEPROMInterface.h"
#include "DevKitMQTTClient.h"
#include "CustomMQTTClient.h"
#include "SerialLog.h"
#include "SystemTickCounter.h"
#include "SystemWiFi.h"
#include "Telemetry.h"
#include "DevkitDPSClient.h"
#include "iothub_client_ll.h"
#include "SystemVersion.h"
#include <iothub_client_hsm_ll.h>


#ifdef __cplusplus
extern "C"
{
#endif


class CustomMQTTClient 
{
public:

    CustomMQTTClient(char* connectionString, bool hasDeviceTwin, bool traceOn);
    virtual ~CustomMQTTClient(void);

    EVENT_INSTANCE* CustomMQTTClient_Event_Generate(const char *eventString, EVENT_TYPE type);

    void CustomMQTTClient_Event_AddProp(EVENT_INSTANCE *message, const char * key, const char * value);

    bool CustomMQTTClient_Init(char* connectionString, bool hasDeviceTwin = false, bool traceOn = false);

    bool CustomMQTTClient_SetOption(const char* optionName, const void* value);

    bool CustomMQTTClient_SendEvent(const char *text);

    bool CustomMQTTClient_ReportState(const char *stateString);

    bool CustomMQTTClient_SendEventInstance(EVENT_INSTANCE *event);

    bool CustomMQTTClient_ReceiveEvent();

    void CustomMQTTClient_Check(bool hasDelay = true);

    void CustomMQTTClient_Close(void);

    void CustomMQTTClient_SetConnectionStatusCallback(CONNECTION_STATUS_CALLBACK connection_status_callback);

    void CustomMQTTClient_SetSendConfirmationCallback(SEND_CONFIRMATION_CALLBACK send_confirmation_callback);

    void CustomMQTTClient_SetMessageCallback(MESSAGE_CALLBACK message_callback);

    void CustomMQTTClient_SetDeviceTwinCallback(DEVICE_TWIN_CALLBACK device_twin_callback);

    void CustomMQTTClient_SetDeviceMethodCallback(DEVICE_METHOD_CALLBACK device_method_callback);

    void CustomMQTTClient_SetReportConfirmationCallback(REPORT_CONFIRMATION_CALLBACK report_confirmation_callback);

    void CustomMQTTClient_Reset(void);
    
private:
    
    int callbackCounter;
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle = NULL;
    int receiveContext = 0;
    int statusContext = 0;
    int trackingId = 0;
    int currentTrackingId = -1;
    bool clientConnected = false;
    bool resetClient = false;
    CONNECTION_STATUS_CALLBACK _connection_status_callback = NULL;
    SEND_CONFIRMATION_CALLBACK _send_confirmation_callback = NULL;
    MESSAGE_CALLBACK _message_callback = NULL;
    DEVICE_TWIN_CALLBACK _device_twin_callback = NULL;
    DEVICE_METHOD_CALLBACK _device_method_callback = NULL;
    REPORT_CONFIRMATION_CALLBACK _report_confirmation_callback = NULL;
    bool enableDeviceTwin = false;

    char* storedIotHubConnectionString;

    uint64_t iothub_check_ms;

    char *iothub_hostname = NULL;
    char *miniSolutionName = NULL;




    void CheckConnection();
    void AZIoTLog(LOG_CATEGORY log_category, const char *file, const char *func, const int line, unsigned int options, const char *format, ...);
    char* GetHostNameFromConnectionString(char *connectionString);
    void FreeEventInstance(EVENT_INSTANCE *event);
    void ConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *userContextCallback);
    void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback);
    IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback);
    void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, size_t size, void *userContextCallback);
    int DeviceMethodCallback(const char *methodName, const unsigned char *payload, size_t size, unsigned char **response, size_t *response_size, void *userContextCallback);
    void ReportConfirmationCallback(int statusCode, void *userContextCallback);
    bool SendEventOnce(EVENT_INSTANCE *event);

    void CustomLogTrace(const char *event, const char *message);
   

 

};


#ifdef __cplusplus
}
#endif

#endif /* __IOTHUB_MQTT_CLIENT_H__ */
