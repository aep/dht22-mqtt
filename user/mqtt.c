#if 1
#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "ip_addr.h"
#include "espconn.h"
#include "os_type.h"
#include "driver/uart.h"
#include "user_interface.h"
#include "unistd.h"
#include "mqtt.h"
#include "espconn.h"
#else
struct espconn {
};
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "mqtt.h"
#define os_memcpy memcpy
#define espconn_sent write
#endif

#define PAYLOAD_MAX 510
#define PAYLOAD_OFF 12

#define LSB(A) (uint8_t)(A & 0x00FF)
#define MSB(A) (uint8_t)((A & 0xFF00) >> 8)

static int mqtt_bake_payload_len(char *message, int length)
{
    char buffer[16];
    int at = 0;
    uint8_t d;
    do {
        d = length % 128;
        length /= 128;
        if (length > 0) {
            d |= 0x80;
        }
        buffer[at++] = d;
    } while (length > 0);

    os_memcpy(message + PAYLOAD_OFF - at, buffer, at);
    return at;
}

static void mqtt_bake_message_frame(char *message, int *message_offset, int *message_length, uint8_t message_type)
{
    int at = mqtt_bake_payload_len(message, *message_length);

    *message_offset -= at;
    *message_length += at;

    *message_offset -= 1;
    *message_length += 1;

    uint8_t header = message_type << 4;
    message[*message_offset] = header;
}


static int mqtt_write_int16_bullshit(char *payload, int16_t value)
{
    *((uint8_t*)(payload))    = MSB(value);
    *((uint8_t*)(payload+ 1)) = LSB(value);
    return 2;
}


void mqtt_send_hello(struct espconn *connection, const char *clientid, bool cleansess)
{
    char message[PAYLOAD_MAX];
    int  plen = 0;
    int  message_offset = PAYLOAD_OFF;
    char *payload = message + message_offset;

    // length of protocol name
    plen += mqtt_write_int16_bullshit(payload + plen, 6);

    // protocol name
    os_memcpy(payload + plen, "MQIsdp", 6);
    plen += 6;

    // protocol_level
    payload[plen] = 3;
    plen += 1;

    // connect_flags
    uint8_t connect_flags  = 0;
    FLAG_CLEANSESS(connect_flags, cleansess ? 1 : 0 );
    payload[plen] = connect_flags;
    plen += 1;

    // keep_alive
    plen += mqtt_write_int16_bullshit(payload + plen, 0);

    // length of clientid
    uint16_t clientidlen = strlen(clientid);
    plen += mqtt_write_int16_bullshit(payload + plen, clientidlen);

    //clientid
    os_memcpy(payload + plen, clientid, clientidlen);
    plen += clientidlen;

    mqtt_bake_message_frame(message, &message_offset, &plen, MQTT_MSG_CONNECT);
    espconn_sent(connection, message + message_offset, plen);
}

void mqtt_publish(struct espconn *connection, const char *key, const char *value)
{
    char message[PAYLOAD_MAX];
    int  plen = 0;
    int  message_offset = PAYLOAD_OFF;
    char *payload = message + message_offset;

    // topic length
    int topiclen = strlen(key);
    plen += mqtt_write_int16_bullshit(payload + plen, topiclen);

    // topic
    os_memcpy(payload + plen, key, topiclen);
    plen += topiclen;

    // DO NOT send package identifier here
    // this is qos0, which doesnt have that field

    // value
    int vallen = strlen(value);
    os_memcpy(payload + plen, value, vallen);
    plen += vallen;

    mqtt_bake_message_frame(message, &message_offset, &plen, MQTT_MSG_PUBLISH);
    espconn_sent(connection, message + message_offset, plen);
}

