#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <MQTTAsync.h>
#include <sendmqtt.h>

static inline int generate_random_clientid(char * clientid, size_t clientid_len){
	const char * randdev = "/dev/urandom";

	int rc, fd;
	uint32_t randnum = 0;

	rc = open(randdev, O_RDONLY);

	if(rc < 0){
		perror("random device open failed");
		goto e_exit;
	}

	fd = rc;

	rc = read(fd, (void *)(&randnum), sizeof(randnum));

	if(rc <= 0){
		perror("random device read failed");
		goto e_cleanup;
	}

	rc = close(fd);
	if(rc < 0){
		perror("random device close failed");
	}

	rc = snprintf(clientid, clientid_len, "ecgdev_%u", randnum);
	if(rc < 0){
		printf("clientid formatting error\n");
		goto e_exit;
	}

	return 0;

e_cleanup:
	close(fd);

e_exit:
	return rc;
}

int mqttsender_init(mqttsender_handle_t * Phandle, const char * address, const char * clientid, 
		const char * username, const char * password, 
		int use_tls, const char * capath, int insecure_tls){
	int rc;
	char clientid_gen[0x20];

	if(!Phandle || !address){
		return MQTTASYNC_NULL_PARAMETER;
	}

	if(!clientid){
		rc = generate_random_clientid(clientid_gen, sizeof(clientid_gen));
		if(rc < 0)
			return rc;

		clientid = clientid_gen;
	}

	*Phandle = NULL;

	connection_entry_t * handle = (connection_entry_t *) malloc(sizeof(connection_entry_t));
	if(handle == NULL){
		return MQTTASYNC_FAILURE;
	}

	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;

	rc = MQTTAsync_create(&handle->client, address, clientid,
			MQTTCLIENT_PERSISTENCE_NONE, NULL);
	if(rc != MQTTASYNC_SUCCESS) {
		MQTTAsync_destroy(&handle->client);
		goto e_cleanup;
	}

	if(username){
		conn_opts.username = username;
		if(password){
			conn_opts.password = password;
		}
	}

	// automatically reconnect when connection lost
	conn_opts.automaticReconnect = 1;

	if(use_tls){
		conn_opts.ssl = &ssl_opts;

		if(capath)
			ssl_opts.trustStore = capath;

		if(insecure_tls)
			ssl_opts.enableServerCertAuth = 0;
	}

	rc = MQTTAsync_setConnectionLostCallback(handle->client, NULL, on_connectionLost);
	if(rc != MQTTASYNC_SUCCESS) {
		printf("Failed to set connection lost callback, return code %d\n", rc);
		MQTTAsync_destroy(&handle->client);
		goto e_cleanup;
	}

//	rc = MQTTAsync_setDeliveryCompleteCallback(handle->client, NULL, on_deliveryComplete);
//	if(rc != MQTTASYNC_SUCCESS) {
//		printf("Failed to set delivery complete callback, return code %d\n", rc);
//		MQTTAsync_destroy(&handle->client);
//		goto e_cleanup;
//	}

	rc = MQTTAsync_connect(handle->client, &conn_opts);
	if(rc != MQTTASYNC_SUCCESS) {
		printf("Failed to connect, return code %d\n", rc);
		MQTTAsync_destroy(&handle->client);
		goto e_cleanup;
	}

	while(!MQTTAsync_isConnected(handle->client)) ;

	*Phandle = (void *)handle;

	return MQTTASYNC_SUCCESS;

e_cleanup:
	free(handle);
	return rc;
}
