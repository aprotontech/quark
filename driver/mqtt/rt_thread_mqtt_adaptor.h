



#ifndef _QUARK_MQTT_RT_THREAD_ADAPTOR_
#define _QUARK_MQTT_RT_THREAD_ADAPTOR_

#if defined(__QUARK_RTTHREAD__)


#include "paho_mqtt.h"

typedef MQTTClient* inner_mqtt_ptr;
typedef int MQTTClient_deliveryToken;
typedef MQTTMessage MQTTClient_message;
typedef MQTTPacket_connectData MQTTClient_connectOptions;

typedef void MQTTClient_connectionLost(void *context, char *cause);
typedef int MQTTClient_messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
typedef void MQTTClient_deliveryComplete(void *context, MQTTClient_deliveryToken dt);

int MQTTClient_create(void** handle, const char* serverURI, const char* clientId,
		int persistence_type, void* persistence_context);
int MQTTClient_connect(void* handle, MQTTClient_connectOptions* options);
int MQTTClient_isConnected(void* handle);        
int MQTTClient_disconnect(void* handle, int timeout);
void MQTTClient_free(void* ptr);
void MQTTClient_destroy(void** handle);
void MQTTClient_freeMessage(MQTTClient_message** msg);
int MQTTClient_subscribe(void* handle, const char* topic, int qos);
int MQTTClient_publish(void* handle, const char* topicName, int payloadlen, void* payload, int qos, int retained,
																 MQTTClient_deliveryToken* dt);
int MQTTClient_setCallbacks(void* handle, void *context, MQTTClient_connectionLost *cl, 
    MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc);
#define MQTTClient_connectOptions_initializer {}
#define MQTTCLIENT_SUCCESS 0
#define MQTTCLIENT_PERSISTENCE_NONE 0

typedef MQTTClient quark_mqtt_client_wrap_t;

#endif //__QUARK_RTTHREAD__

#endif