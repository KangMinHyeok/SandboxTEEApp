#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <readecg.h>
#include <sendmqtt.h>

/*
 * krlee@sysi9:~/infotracking/ecgprocess/rpidevsync$ python3 ../../ecgdisplay/samples/ecgtomqttsim.py --help
 * usage: ecgtomqttsim.py [-h] [--sanitize] [-s SEED] [--noise NOISE]
 *                        [-r RESOLUTION] [--resolution-stderr RESOLUTION_STDERR]
 *                        [-i CLIENT_ID] [-H HOST] [-p PORT] [-t TOPIC]
 *                        [-u USERNAME] [-P PASSWORD] [--tls] [--cacert CACERT]
 *                        [--cert CERT] [--key KEY] [--insecure]
 *                        [--normal ACTION] [--abnormal ACTION]
 *                        [--normal-cycle ACTION] [--abnormal-cycle ACTION]
 * 
 * optional arguments:
 *   -h, --help            show this help message and exit
 *   --sanitize            sanitize behavior of program
 *   -s SEED, --seed SEED  seed for generating random number
 *   --noise NOISE         magnitude of noise of ecg signal
 *   -r RESOLUTION, --resolution RESOLUTION
 *                         time interval between each sample
 *   --resolution-stderr RESOLUTION_STDERR
 *                         std error of time interval between each sample
 *   -i CLIENT_ID, --client-id CLIENT_ID
 *                         id to use for this client (default: sampleclient
 *                         appended with random number)
 *   -H HOST, --host HOST  mqtt host to connect to (default: localhost)
 *   -p PORT, --port PORT  network port to connect to (default: 1883)
 *   -t TOPIC, --topic TOPIC
 *                         mqtt topic to publish to (default: hello)
 *   -u USERNAME, --username USERNAME
 *                         provide a username
 *   -P PASSWORD, --password PASSWORD
 *                         provide a password
 *   --tls                 enable tls
 *   --cacert CACERT       path to a file containing trusted CA certificates to
 *                         enable encrypted communication
 *   --cert CERT           client certificate for authentication if required by
 *                         server
 *   --key KEY             client private key for authentication if required by
 *                         server
 *   --insecure            disable server certificate verification
 *   --normal ACTION       send normal ecg signal (unit: ms)
 *   --abnormal ACTION     send abnormal ecg signal (unit: ms)
 *   --normal-cycle ACTION
 *                         send normal ecg signal (unit: cycle)
 *   --abnormal-cycle ACTION
 *                         send abnormal ecg signal (unit: cycle)
 */

#define ADDRESS     "ssl://147.46.244.130:8883"
#define CLIENTID    "RaspberryPiAlpha"
#define USERNAME    "demo"
#define PASSWORD    "guest"
#define TOPIC       "hello"
//#define PAYLOAD     "Msg from RaspberryPiAlpha"
#define QOS         0
#define TIMEOUT     10000L
#define CAPATH      "./sp_cert_ca.crt"

typedef struct ecgdatapoint_t {
uint64_t epoch_milliseconds;    /* unit: [ms] */
uint64_t voltage;               /* unit: [uV] */
} __attribute__((__packed__)) ecgdatapoint_t;

#include <arpa/inet.h> // for htonl

static inline uint64_t htonll(uint64_t hostll){
	struct _llexpand {
		uint32_t upper, lower;
	} llexpand;

	llexpand.upper = htonl((uint32_t)((hostll >> 32U) & ((uint64_t)0xffffffff)));
	llexpand.lower = htonl((uint32_t)(hostll & ((uint64_t)0xffffffff)));

	return *(uint64_t *)(&llexpand);
}

typedef struct _dataelement_t {
	struct _dataelement_t * next;
	ecgdatapoint_t data;
} dataelement_t, * Pdataelement_t;

static pthread_mutex_t datalock = PTHREAD_MUTEX_INITIALIZER;
static Pdataelement_t head = NULL, tail = NULL, pool = NULL;

static inline void push_to_head_tail(Pdataelement_t p, Pdataelement_t * Phead, Pdataelement_t * Ptail){
	if(Phead == NULL) return;

	if(Ptail != NULL && *Ptail == NULL){
		p->next = NULL;
		*Ptail = p;
	} else {
		p->next = *Phead;
	}

	*Phead = p;
}

static inline Pdataelement_t pop_from_head_tail(Pdataelement_t * Phead, Pdataelement_t * Ptail){
	Pdataelement_t p = NULL;

	if(Phead == NULL) return NULL;

	if(*Phead != NULL){
		p = *Phead;

		if(Ptail != NULL && *Phead == *Ptail){
			*Phead = *Ptail = NULL;
		} else {
			*Phead = (*Phead)->next;
		}
	}

	return p;
}

static inline Pdataelement_t pop_from_head(){ return pop_from_head_tail(&head, &tail); }
static inline void push_to_head(Pdataelement_t p){ push_to_head_tail(p, &head, &tail); }
static inline Pdataelement_t pop_from_pool(){ return pop_from_head_tail(&pool, NULL); }
static inline void push_to_pool(Pdataelement_t p){ push_to_head_tail(p, &pool, NULL); }

void thread_main_payload_launch(void * arg){
	int rc;

	MQTTAsync client;

	const char * addr = ADDRESS;
	const char * clientid = CLIENTID;
	const char * username = USERNAME;
	const char * password = PASSWORD;
	const char * topic = TOPIC;
	const char * capath = CAPATH;

	const size_t BLOCK_SIZE = 0x400;

	size_t len = 0;
	unsigned char buf[BLOCK_SIZE];

	rc = mqttsender_init(&client, addr, clientid, username, password, capath);
	if(rc < 0){
		printf("Failed to initialize, return code %d\n", rc);
		goto e_cleanup;
	}

	while(1){
		rc = pthread_mutex_lock(&datalock);
		if(rc < 0){
			perror("datalock locking fail");
			continue;
		}

		while(head != NULL && len + sizeof(ecgdatapoint_t) <= BLOCK_SIZE){
			Pdataelement_t onload = pop_from_head();

			memcpy(&buf[len], (void *) &onload->data, sizeof(ecgdatapoint_t));
			len += sizeof(ecgdatapoint_t);

			push_to_pool(onload);
		}

		rc = pthread_mutex_unlock(&datalock);
		if(rc < 0){
			perror("datalock unlock fail");
			continue;
		}

		if(len > BLOCK_SIZE - sizeof(ecgdatapoint_t)){
			for(size_t trycount=0;trycount<5;++trycount){
				rc = mqttsender_send(client, topic, (void *)buf, len);
				if(rc < 0){
					printf("Failed to send, return code %d\n", rc);
					sleep(0);
					continue;
				} else{
					break;
				}
			}
			if(rc < 0){
				printf("Failed to send, return code %d\n", rc);
			}

			len = 0;
		}
	}

	rc = mqttsender_join(client, 1000000);
	if(rc < 0){
		printf("Failed to wait until complete, return code %d\n", rc);
		goto e_cleanup;
	}

e_cleanup:
	rc = mqttsender_end(client);
	if(rc < 0){
		printf("Failed to end client, return code %d\n", rc);
	}

}

void thread_main_getvoltage(void *){
	int rc, fd;
	struct timespec nextstep;

	/* configuration */
	int spimode = SPI_MODE_0;
	int lsb_first = 0; /* false */
	int bits_per_word = 8;
	uint32_t max_speed_hz = 1350000; /* 1.35 (MHz) */
	int channel = 0;
	struct timespec resolution = {
		.tv_sec = 0,
		.tv_nsec = 7000000,
	};

	rc = mcp3004_open("/dev/spidev0.0", spimode, lsb_first, bits_per_word, channel);
	if(rc < 0)
		goto e_exit;

	fd = rc;

	rc = clock_gettime(CLOCK_MONOTONIC, &nextstep);
	if(rc < 0){
		fprintf(stderr, "failed to get monotonic time\n");
