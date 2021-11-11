#include "rc_mqtt.h"
#include "logs.h"
#include "mqtt_adaptor.h"

#ifdef __QUARK_FREERTOS__

void messageArrived(void* client, MessageData* data)
{
    client_network_t* cnt = (client_network_t*)client;
    if (cnt->ma != NULL) {
        cnt->ma(cnt->context, data->topicName->cstring, MQTTstrlen(*data->topicName), data->message);
    }
}

int MQTTClient_create(void** handle, const char* serverURI, const char* clientId,
		int persistence_type, void* persistence_context)
{
    client_network_t* cnt = (client_network_t*)*handle;
    NetworkInit(&cnt->network);
    MQTTClientInit(&cnt->client, &cnt->network, 5000, cnt->sendbuf, sizeof(cnt->sendbuf), cnt->readbuf, sizeof(cnt->readbuf));

    char* host = (char*)cnt->host; // use buffer temp
    strcpy(host, serverURI + sizeof("tcp://") - 1);
    char* s = strchr(host, ':');
    cnt->port = atoi(s + 1);

    s[0] = '\0';

    *handle = cnt;
    return 0;
}


int MQTTClient_connect(void* handle, MQTTClient_connectOptions* options)
{
    client_network_t* cnt = (client_network_t*)handle;
    char* host = (char*)cnt->host;
    int rc = 0;
    if ((rc = NetworkConnect(&cnt->network, host, cnt->port)) != 0) {
        LOGI(MQ_TAG, "Return code from remote(%s:%d) network connect is %d", host, cnt->port, rc);
        return -1;
    }

    if ((rc = MQTTConnect(&cnt->client, options)) != 0) {
		LOGI(MQ_TAG, "Return code from MQTT connect is %d", rc);
        return -1;
    } else {
		LOGI(MQ_TAG, "MQTT Connected, remote(%s:%d)", host, cnt->port);
    }

    if (cnt->client.thread.task != NULL && MQTTStartTask(&cnt->client) != 0) {
        LOGI(MQ_TAG, "Start backgroup task failed");
        return -1;
    }
    
    return 0;
}

int MQTTClient_isConnected(void* handle)
{
    client_network_t* cnt = (client_network_t*)handle;
    return MQTTIsConnected(&cnt->client);
}

int MQTTClient_disconnect(void* handle, int timeout)
{
    client_network_t* cnt = (client_network_t*)handle;
    MQTTDisconnect(&cnt->client);
    return 0;
}

void MQTTClient_free(void* ptr)
{
}

void MQTTClient_freeMessage(MQTTClient_message** msg)
{
}

void MQTTClient_destroy(void** handle)
{
    client_network_t* cnt = (client_network_t*)*handle;
    if (cnt->client.thread.task != NULL) {
        LOGI(MQ_TAG, "MQTTClient_destroy delete task");
        vTaskDelete(cnt->client.thread.task);
        cnt->client.thread.task = NULL;
    }
    rc_free(cnt);
    return;
}

int MQTTClient_subscribe(void* handle, const char* topic, int qos)
{
    client_network_t* cnt = (client_network_t*)handle;
    return MQTTSubscribe(&cnt->client, topic, qos, messageArrived);
}

int MQTTClient_publish(void* handle, const char* topicName, int payloadlen, void* payload, int qos, int retained,
																 MQTTClient_deliveryToken* dt)
{
    client_network_t* cnt = (client_network_t*)handle;
    MQTTMessage message;
    message.payload = payload;
    message.payloadlen = payloadlen;
    message.retained = retained;
    message.dup = 0;
    message.qos = qos;
    if (MQTTPublish(&cnt->client, topicName, &message) == 0) {
        *dt = message.id; 
    }
}

int MQTTClient_setCallbacks(void* handle, void *context, MQTTClient_connectionLost *cl, 
    MQTTClient_messageArrived *ma, MQTTClient_deliveryComplete *dc)
{
    client_network_t* cnt = (client_network_t*)handle;
    cnt->cl = cl;
    cnt->ma = ma;
    cnt->dc = dc;
    cnt->context = context;
    return 0;
}

#endif
