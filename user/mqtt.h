#define MQTT_PROTOCOL_MAGIC "MQIsdp"
#define MQTT_PROTO_MAJOR 3
#define MQTT_PROTO_MINOR 1
#define MQTT_PROTOCOL_VERSION "MQTT/3.1"

#define FLAG_CLEANSESS(F, C)(F | ((C) << 1))
#define FLAG_WILL(F, W)(F | ((W) << 2))
#define FLAG_WILLQOS(F, Q)(F | ((Q) << 3))
#define FLAG_WILLRETAIN(F, R) (F | ((R) << 5))
#define FLAG_PASSWD(F, P)(F | ((P) << 6))
#define FLAG_USERNAME(F, U)(F | ((U) << 7))

#define MQTT_MSG_CONNECT     1
#define MQTT_MSG_CONACK      2
#define MQTT_MSG_PUBLISH     3
#define MQTT_MSG_PUBACK      4
#define MQTT_MSG_PUBREC      5
#define MQTT_MSG_PUBREL      6
#define MQTT_MSG_PUBCOMP     7
#define MQTT_MSG_SUBSCRIBE   8
#define MQTT_MSG_SUBACK      9
#define MQTT_MSG_UNSUBSCRIBE 10
#define MQTT_MSG_UNSUBACK11  11
#define MQTT_MSG_PINGREQ     12
#define MQTT_MSG_PINGRESP    13
#define MQTT_MSG_DISCONNECT  14



void mqtt_send_hello(struct espconn *connection, const char *clientid, bool cleansess);
void mqtt_publish(struct espconn *connection, const char *key, const char *value);
