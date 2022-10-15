

#ifndef _MQTT_DEFINES_H_
#define _MQTT_DEFINES_H_

//// CUSTOM-DEFINES
#ifndef MQTT_TOPIC_RPC
#define MQTT_TOPIC_RPC "rpc"
#endif

#ifndef MQTT_TOPIC_CMD
#define MQTT_TOPIC_CMD "cmd"
#endif

#ifndef MQTT_TOPIC_EVT
#define MQTT_TOPIC_EVT "evt"
#endif

#ifndef MQTT_TOPIC_ACK
#define MQTT_TOPIC_ACK "ack"
#endif

#ifndef MQTT_RPC_QOS
#define MQTT_RPC_QOS 0
#endif

#ifndef MQTT_CMD_QOS
#define MQTT_CMD_QOS 1
#endif


///// FIXED DEFINES
#define MQTT_TOPIC_SEP '/'

#define MQTT_TOPIC_TYPE_CMD 1
#define MQTT_TOPIC_TYPE_RPC 2
#define MQTT_TOPIC_TYPE_ACK 3

#define MQTT_CLIENT_PAD_SIZE 384

#define MQTT_TOPIC_PREFIX_LENGTH 60
#define MQTT_MAX_CLIENTID_LENGTH 32
#define MQTT_MAX_USERNAME_LENGTH 64
#define MQTT_MAX_PASSWD_LENGTH 100

#endif