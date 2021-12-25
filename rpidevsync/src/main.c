
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
void argparse_help(const char * argv0){
	if(!argv0)
		argv0 = "ecg2mqtt";

	printf(
" usage: %s [-h] [-r RESOLUTION] [-i CLIENT_ID]\n"
"                        [-H HOST] [-p PORT] [-t TOPIC]\n"
"                        [-u USERNAME] [-P PASSWORD]\n"
"                        [--tls] [--cacert CACERT]\n"
"                        [--cert CERT] [--key KEY] [--insecure]\n"
"                        [--block-size BLOCK_SIZE]\n"
" \n"
" optional arguments:\n"
"   -h, --help            show this help message and exit\n"
"   -r RESOLUTION, --resolution RESOLUTION\n"
"                         time interval between each sample (default: 10) [unit: ms]\n"
"   -i CLIENT_ID, --client-id CLIENT_ID\n"
"                         id to use for this client (default: sampleclient\n"
"                         appended with random number)\n"
"   -H HOST, --host HOST  mqtt host to connect to (default: localhost)\n"
"   -p PORT, --port PORT  network port to connect to (default: 1883)\n"
"   -t TOPIC, --topic TOPIC\n"
"                         mqtt topic to publish to (default: hello)\n"
"   -u USERNAME, --username USERNAME\n"
"                         provide a username\n"
"   -P PASSWORD, --password PASSWORD\n"
"                         provide a password\n"
"   --tls                 enable tls\n"
"   --cacert CACERT       path to a file containing trusted CA certificates to\n"
"                         enable encrypted communication\n"
"   --cert CERT           client certificate for authentication if required by\n"
"                         server (not used for now)\n"
"   --key KEY             client private key for authentication if required by\n"
"                         server (not used for now)\n"
"   --insecure            disable server certificate verification\n"
"   --block-size          maximum packet size of mqtt protocol (default: 0x400) [unit: byte]\n"
	, argv0);

	exit(0);
}

struct argument_t {
	long unsigned sample_interval_ms;	// default: 10
	char * client_id;			// default: null (automatic random generation)
	char * host;				// default: "localhost"
	unsigned int port;			// default: 1883
	char * topic;				// default: "hello"
	char * username;			// default: NULL
	char * password;			// default: NULL
	int enable_tls;				// default: false
	char * cacert;				// default: NULL
	char * cert;				// default: NULL
	char * key;				// default: NULL
	int tls_disable_check;			// default: false
	size_t block_size;
};

void argparse_display_args(const struct argument_t * arg){
	printf("sample interval: %lu(ms)\n", arg->sample_interval_ms);
	printf("client id: %s\n", arg->client_id ? arg->client_id : 
		"(random id will be generated)");
	printf("host: %s\n", arg->host);
	printf("port: %u\n", arg->port);
	printf("username: %s\n", arg->username);
	printf("password: %s\n", arg->password ? "****** (shadowed)" : "(unset)");
	printf("enable tls: %c\n", arg->enable_tls ? 'Y' : 'N');
	printf("cacert: %s\n", arg->cacert ? arg->cacert : "default");
	printf("cert: %s\n", arg->cert ? arg->cert : "no certificate specified");
	printf("key: %s\n", arg->key ? arg->key : "no key specified");
	printf("disable tls certificate check: %c\n", arg->tls_disable_check ? 'Y' : 'N');
	printf("block size: %lu\n", (long unsigned)arg->block_size);
}

static char * strdup_assert(const char * s){
	char * dup = strdup(s);
	if(!dup){
		perror("string duplication failed");
		exit(errno);
	}
	
	return dup;
}

void argparse_parse_args(int argc, char *argv[], struct argument_t * arg){
	int ret;

	// initialize arguments
	arg->sample_interval_ms = 10;
	arg->client_id = NULL;
	arg->host = "localhost";
	arg->port = 1883;
	arg->topic = "hello";
	arg->username = NULL;
	arg->password = NULL;
	arg->enable_tls = 0;
	arg->cacert = NULL;
	arg->cert = NULL;
	arg->key = NULL;
	arg->tls_disable_check = 0;
	arg->block_size = 0x400;

	const char * optstring = "hr:i:H:p:t:u:P:";

	const struct option longopts[] = {
		{
			.name = "help",
			.has_arg = 0,
			.flag = NULL,
			.val = 'h'
		},
		{
			.name = "resolution",
			.has_arg = 1,
			.flag = NULL,
			.val = 'r'
		},
		{
			.name = "client-id",
			.has_arg = 1,
			.flag = NULL,
			.val = 'i'
		},
		{
			.name = "host",
			.has_arg = 1,
			.flag = NULL,
			.val = 'H'
		},
		{
			.name = "port",
			.has_arg = 1,
			.flag = NULL,
			.val = 'p'
		},
		{
			.name = "topic",
			.has_arg = 1,
			.flag = NULL,
			.val = 't'
		},
		{
			.name = "username",
			.has_arg = 1,
			.flag = NULL,
			.val = 'u'
		},
		{
			.name = "password",
			.has_arg = 1,
			.flag = NULL,
			.val = 'P'
		},
		{
			.name = "tls",
			.has_arg = 0,
			.flag = NULL,
			.val = 'T'
		},
		{
			.name = "cacert",
			.has_arg = 1,
			.flag = NULL,
			.val = 'C'
		},
		{
			.name = "cert",
			.has_arg = 1,
			.flag = NULL,
			.val = 'c'
		},
		{
			.name = "key",
			.has_arg = 1,
			.flag = NULL,
			.val = 'k'
		},
		{
			.name = "insecure",
			.has_arg = 0,
			.flag = NULL,
			.val = 'I'
		},
		{
			.name = "block-size",
			.has_arg = 1,
			.flag = NULL,
			.val = 'B'
		},
		{ // last element filled with zeros
			.name = NULL,
			.has_arg = 0,
			.flag = NULL,
			.val = 0
		}


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
		goto e_cleanup;
	}

	while(1){
		struct timespec now;
		ecgdatapoint_t element;
		Pdataelement_t p = NULL;

		rc = mcp3004_readvalue(fd, channel);
		if(rc < 0){
			goto e_step;
		}

		rc = clock_gettime(CLOCK_REALTIME, &now);
		if(rc < 0){
			perror("Failed to fetch realtime\n");
			goto e_step;
		}

		data.epoch_milliseconds = htonll((uint64_t)now.tv_sec * 1000 + now.tv_nsec / (uint64_t)1e6);
		data.voltage = htonll(mcp3004_value_to_voltage((uint32_t)rc));

		rc = pthread_mutex_lock(&datalock);
		if(rc < 0){
			perror("datalock locking fail");
			goto e_cleanup_step;
		}

		if(pool == NULL){
			p = malloc(sizeof(dataelement_t));
			if(p == NULL){
				fprintf(stderr, "failed to allocate dataelement_t\n");
				goto e_cleanup_step;
			}
		} else {
			p = pop_from_pool();
		}

		memcpy(&p->data, &element, sizeof(ecgdatapoint_t));
		push_to_head(p);

e_cleanup_step:

		rc = pthread_mutex_unlock(&datalock);
		if(rc < 0){
			perror("datalock locking fail");
			goto e_step;
		}

e_step:

		nextstep.tv_nsec += resolution.tv_nsec;
		if(nextstep.tv_nsec >= 1000000000){
			nextstep.tv_nsec -= 1000000000;
			nextstep.tv_sec++;
		}

		rc = clock_nanosleep(&CLOCK_MONOTONIC, TIMER_ABSTIME, &nextstep, NULL);
		if(rc < 0){
			fprintf(stderr, "clock_nanosleep returned error code %d\n", rc);
		}
	}


e_cleanup:
	if(fd >= 0){
		mcp3004_close(fd);
		fd = -1;
	}

e_exit:
	return;

}

typedef struct _option_t {
	const char * addr;
	const char * clientid;
	const char * username;
	const char * password;
	const char * topic;
	const char * capath;
} option_t;

#include <time.h>

int main(int argc, char * argv[]){
	int rc;
	MQTTAsync client;

	const char * addr = ADDRESS;
	const char * clientid = CLIENTID;
	const char * username = USERNAME;
	const char * password = PASSWORD;
	const char * topic = TOPIC;
	const char * capath = CAPATH;

	rc = mqttsender_init(&client, addr, clientid, username, password, capath);
	if(rc < 0){
		printf("Failed to initialize, return code %d\n", rc);
		goto e_cleanup;
	}

	for(int k=0;k<0x100;++k){
		struct timespec now;
		rc = clock_gettime(CLOCK_REALTIME, &now);
		if(rc < 0){
			perror("Failed to fetch realtime\n");
			continue;
		}

		ecgdatapoint_t data;
		data.epoch_milliseconds = htonll((uint64_t)now.tv_sec * 1000 + now.tv_nsec / (uint64_t)1e6);
		data.voltage = htonll((now.tv_sec % 4 * 1000) + 2000);

		void * payload = (void *)(&data);
		size_t payloadlen = sizeof(data);

		rc = mqttsender_send(client, topic, payload, payloadlen);
		if(rc < 0){
			printf("Failed to send, return code %d\n", rc);
		}

//		struct timespec sleepinterval;
//		sleepinterval.tv_sec = 0;
//		sleepinterval.tv_nsec = 3000000;
//		nanosleep(&sleepinterval, NULL);
	}

	rc = mqttsender_join(client, 1000000);
	if(rc < 0){
		printf("Failed to wait until complete, return code %d\n", rc);
		goto e_cleanup;
	}

e_cleanup:
	rc = mqttsender_end(client);

//e_exit:
	return 0;
}
