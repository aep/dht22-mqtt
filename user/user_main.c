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


#define MAXTIMINGS 10000
#define BREAKTIME 20
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

static volatile os_timer_t sensor_timer;
static char hwaddr[6];
static char clientid[64];

static int poll_counter = 0;
static int no_connection_counter = 0;
static struct espconn *broker_connection = NULL;
static bool broker_connected = false;

static void ICACHE_FLASH_ATTR at_tcpclient_sent_cb(void *arg) {
}

static void ICACHE_FLASH_ATTR at_tcpclient_recv_cb(void *arg, char*pdata, unsigned short len) {
    os_printf("received data\n");
}

static void ICACHE_FLASH_ATTR at_tcpclient_discon_cb(void *arg) {
    struct espconn *pespconn = (struct espconn *)arg;
    os_free(pespconn->proto.tcp);

    os_free(pespconn);
    broker_connection = NULL;
    os_printf("disconnected\n");
    system_restart();
}

static void ICACHE_FLASH_ATTR send_reading(float t, float h)
{
    char value[128];
    os_sprintf(value, "{\"temp\":%d,\"humid\":%d}", (int)(t*100), (int)(h*100));
    mqtt_publish(broker_connection, "/esp/dht22", value);
}

static void ICACHE_FLASH_ATTR at_tcpclient_connect_cb(void *arg)
{
    broker_connection = (struct espconn *)arg;
    broker_connected = true;

    os_printf("tcp client connect\r\n");
    os_printf("pespconn %p\r\n", broker_connection);

    mqtt_send_hello(broker_connection, clientid, true);

    espconn_regist_sentcb(broker_connection, at_tcpclient_sent_cb);
    espconn_regist_recvcb(broker_connection, at_tcpclient_recv_cb);
    espconn_regist_disconcb(broker_connection, at_tcpclient_discon_cb);

}
static bool ICACHE_FLASH_ATTR broker_established()
{
    if (++no_connection_counter > 5) {
        os_printf("can't establish connection. reboot\n");
        system_restart();
    }

    int st = wifi_station_get_connect_status();
    if (st != STATION_GOT_IP) {
        os_printf("sensor_poll: no station yet\r\n");
        return false;
    }

    if (broker_connection == NULL) {
        os_printf("sensor_poll: no connection yet\r\n");

        struct espconn *pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
        if (pCon == NULL)
        {
            os_printf("CONNECT FAIL\r\n");
            return false;
        }
        pCon->type = ESPCONN_TCP;
        pCon->state = ESPCONN_NONE;

//        espconn_gethostbyname(struct espconn *pespconn, const char *hostname,
//                ip_addr_t *addr, dns_found_callback found);

        uint32_t ip = ipaddr_addr("85.119.83.194");

        pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
        pCon->proto.tcp->local_port = espconn_port();
        pCon->proto.tcp->remote_port = 1883;

        os_memcpy(pCon->proto.tcp->remote_ip, &ip, 4);

        espconn_regist_connectcb(pCon, at_tcpclient_connect_cb);
        espconn_connect(pCon);

        broker_connection = pCon;

        return false;
    }

    if (!broker_connected) {
        os_printf("sensor_poll: not established yet\r\n");
        return false;
    }

    no_connection_counter = 0;
    return true;
}


static void ICACHE_FLASH_ATTR sensor_poll(void *arg)
{
    if (poll_counter++ > 1000)
        system_restart();

    if (!broker_established())
        return;

    os_printf("reading dht\r\n");
//    os_printf("sending dht data\r\n");
//    send_reading(0.2, 0.2);
//    return;

    int counter = 0;
    int laststate = 1;
    int i = 0;
    int j = 0;
    int checksum = 0;
    //int bitidx = 0;
    //int bits[250];

    int data[100];

    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    GPIO_OUTPUT_SET(2, 1);
    os_delay_us(250000);
    GPIO_OUTPUT_SET(2, 0);
    os_delay_us(20000);
    GPIO_OUTPUT_SET(2, 1);
    os_delay_us(40);
    GPIO_DIS_OUTPUT(2);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);


    // wait for pin to drop?
    while (GPIO_INPUT_GET(2) == 1 && i<100000) {
        os_delay_us(1);
        i++;
    }

    if(i == 100000)
        return;

    // read data!

    for (i = 0; i < MAXTIMINGS; i++) {
        counter = 0;
        while ( GPIO_INPUT_GET(2) == laststate) {
            counter++;
            os_delay_us(1);
            if (counter == 1000)
                break;
        }
        laststate = GPIO_INPUT_GET(2);
        if (counter == 1000) break;

        //bits[bitidx++] = counter;

        if ((i>3) && (i%2 == 0)) {
            // shove each bit into the storage bytes
            data[j/8] <<= 1;
            if (counter > BREAKTIME)
                data[j/8] |= 1;
            j++;
        }
    }

    /*
       for (i=3; i<bitidx; i+=2) {
       os_printf("bit %d: %d\n", i-3, bits[i]);
       os_printf("bit %d: %d (%d)\n", i-2, bits[i+1], bits[i+1] > BREAKTIME);
       }
       os_printf("Data (%d): 0x%x 0x%x 0x%x 0x%x 0x%x\n", j, data[0], data[1], data[2], data[3], data[4]);
       */
    float temp_p, hum_p;
    if (j >= 39) {
        checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        if (data[4] == checksum) {
            /* yay! checksum is valid */

            hum_p = data[0] * 256 + data[1];
            hum_p /= 10;

            temp_p = (data[2] & 0x7F)* 256 + data[3];
            temp_p /= 10.0;
            if (data[2] & 0x80)
                temp_p *= -1;

            os_printf("sending dht data\r\n");
            send_reading(temp_p, hum_p);
        }
    }

}

void ICACHE_FLASH_ATTR user_init(void)
{
    //NOTE the uart is 76000 kbs
    os_printf("\r\n booting and shit \r\n");


    char ssid[32] = WIFI_SSID;
    char password[64] = WIFI_PASSWORD;
    struct station_config stationConf;
    wifi_set_opmode( 0x1 );
    os_memcpy(&stationConf.ssid, ssid, 32);
    os_memcpy(&stationConf.password, password, 64);

    wifi_station_set_config(&stationConf);


    //Set GPIO2 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);

    wifi_get_macaddr(0, hwaddr);
    os_sprintf(clientid,  "esp:" MACSTR , MAC2STR(hwaddr));

    os_timer_disarm(&sensor_timer);

    //Setup timer
    os_timer_setfn(&sensor_timer, (os_timer_func_t *)sensor_poll, NULL);


    //Arm the timer
    //&sensor_timer is the pointer
    //1000 is the fire time in ms
    //0 for once and 1 for repeating
    os_timer_arm(&sensor_timer, 4000, 1);
}


