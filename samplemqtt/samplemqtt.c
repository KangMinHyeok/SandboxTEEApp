#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
//#include <unistd.h>
#include <time.h>
#include <arpa/inet.h> // for htonl

#include <MQTTAsync.h>

#define ADDRESS     "ssl://147.46.244.130:8883"
#define CLIENTID    "RaspberryPiAlpha"
#define USERNAME    "demo"
#define PASSWORD    "guest"
#define TOPIC       "hello"
//#define PAYLOAD     "Msg from RaspberryPiAlpha"
#define QOS         0
#define TIMEOUT     10000L
#define CAPATH      "./sp_cert_ca.crt"

static inline uint64_t htonll(uint64_t hostll){
	struct _llexpand {
		uint32_t upper, lower;
	} llexpand;

	llexpand.upper = htonl((uint32_t)((hostll >> 32U) & ((uint64_t)0xffffffff)));
	llexpand.lower = htonl((uint32_t)(hostll & ((uint64_t)0xffffffff)));

	return *(uint64_t *)(&llexpand);
}


volatile MQTTAsync_token deliveredtoken;
void delivered(void *context, MQTTAsync_token dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}
int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    int i;
    char* payloadptr;
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");
    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}
void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

int main(int argc, char* argv[])
{
    int ret;

    MQTTAsync client;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;

    ret = MQTTAsync_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if(ret != MQTTASYNC_SUCCESS){
            MQTTAsync_destroy(&client);
            return -1;
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.automaticReconnect = 1;
    ssl_opts.trustStore = CAPATH;
    conn_opts.ssl = &ssl_opts;
    MQTTAsync_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    if ((ret = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", ret);
	MQTTAsync_destroy(&client);
        exit(EXIT_FAILURE);
    }
//    pubmsg.payload = PAYLOAD;
//    pubmsg.payloadlen = strlen(PAYLOAD);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    deliveredtoken = 0;
    MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;

    while(1){
	    struct timespec this_scoop;
	    ret = clock_gettime(CLOCK_MONOTONIC, &this_scoop);

	    struct timespec elapsed;
	    long long elapsed_time_ns;

	    struct timespec now;
	    ret = clock_gettime(CLOCK_REALTIME, &now);
	    if(ret < 0){
		    printf("failed to get real time, skipping this step\n");
	    } else {

			typedef struct ecgdatapoint_t {
			    uint64_t epoch_milliseconds;    /* unit: [ms] */
			    uint64_t voltage;               /* unit: [uV] */
			} __attribute__((__packed__)) ecgdatapoint_t;

		ecgdatapoint_t data;
		data.epoch_milliseconds = htonll((uint64_t)now.tv_sec * 1000 + now.tv_nsec / (uint64_t)1e6);
		data.voltage = htonll((now.tv_sec % 4 * 1000) + 2000);

		pubmsg.payload = (char *)(&data);
		pubmsg.payloadlen = sizeof(data);

		ret = MQTTAsync_sendMessage(client, TOPIC, &pubmsg, &response_options);

		pubmsg.payload = NULL;
		pubmsg.payloadlen = 0;
	//        if(ret != MQTTASYNC_SUCCESS){
	//            printf("Failed to send message, return code %d\n", ret);
	//            MQTTAsync_destroy(&client);
	//            exit(EXIT_FAILURE);
	//        }
	//	printf("Sent Message %d\n", k);
	//	sleep(1);
	    }
	
    }
//    printf("Waiting for publication of %s\n"
//            "on topic %s for client with ClientID: %s\n",
//            PAYLOAD, TOPIC, CLIENTID);
    MQTTAsync_waitForCompletion(client, response_options.token, 10000);
    //while(deliveredtoken != response_options.token);

    MQTTAsync_disconnectOptions disconn_options = MQTTAsync_disconnectOptions_initializer;
    MQTTAsync_disconnect(client, &disconn_options);
    MQTTAsync_destroy(&client);
    return ret;
}

