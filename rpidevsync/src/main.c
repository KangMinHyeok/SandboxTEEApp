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
