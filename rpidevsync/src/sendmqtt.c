#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <MQTTAsync.h>
#include <sendmqtt.h>

static void on_deliveryFailure(void * context, MQTTAsync_failureData * response){
	int rc;

	// called when send failed
	fprintf(stderr, "failture callback for context %p\n", context);

	//TODO: retry failed token
	message_entry_t * msgentry = (message_entry_t *) context;
	connection_entry_t * handle = msgentry->conn;

	// delete from list
	free(msgentry->payload);
	free(msgentry);
	return;
	// temporarily no retry

	// call send message again
	MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
	response_options.context = (void *)msgentry;
	response_options.onSuccess = on_deliverySuccess;
	response_options.onFailure = on_deliveryFailure;

	while(msgentry->trycount < 5) {
		msgentry->trycount++;
		rc = MQTTAsync_send(handle->client, msgentry->topic, 
				msgentry->payloadlen, msgentry->payload, 
				0 /* qos */, 0 /* retained flag */, &response_options);

		if(rc != MQTTASYNC_SUCCESS){
			sleep(0);
		} else {
			msgentry->token = response_options.token;
			return;
		}
	}

	printf("failed message retry count exceeded (5 times)\n");
	printf("dump:");
	if(!msgentry->payloadlen) printf(" (nil)\n");
	else {
		for(size_t k=0;k<msgentry->payloadlen;++k) printf(" %02x", ((unsigned char *)msgentry->payload)[k]);
		putchar('\n');
	}

	free(msgentry->payload);
	free(msgentry);
}

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

int mqttsender_send(mqttsender_handle_t _handle, const char * topic, void * payload, size_t payloadlen){
	int rc;
	message_entry_t * msgentry;
	size_t topiclen;
	void * payloaddup;
	char * topicdup;
	
	MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;

	if(!_handle || !topic || !payload)
		return MQTTASYNC_NULL_PARAMETER;

	connection_entry_t * handle = (connection_entry_t *) _handle;

	msgentry = (message_entry_t *) malloc(sizeof(message_entry_t));
	if(msgentry == NULL)
		return MQTTASYNC_FAILURE;

	topiclen = strlen(topic);
	payloaddup = malloc(payloadlen + topiclen + 1);
	if(payloaddup == NULL){
		rc = MQTTASYNC_FAILURE;
		goto e_cleanup;
	}
	memcpy(payloaddup, payload, payloadlen);

	topicdup = (char *) payloaddup + payloadlen;
	memcpy(topicdup, topic, topiclen + 1);

	response_options.context = (void *)msgentry;
	response_options.onSuccess = on_deliverySuccess;
	response_options.onFailure = on_deliveryFailure;

	msgentry->topic = topicdup;
	msgentry->payload = payloaddup;
	msgentry->payloadlen = payloadlen;
	msgentry->conn = handle;
	msgentry->token = response_options.token;
	msgentry->trycount = 0;

	rc = MQTTAsync_send(handle->client, topic, 
			payloadlen, payload, 
			0 /* qos */, 0 /* retained flag */, &response_options);
	if(rc != MQTTASYNC_SUCCESS)
		goto e_cleanup2;

	return MQTTASYNC_SUCCESS;

e_cleanup2:
	free(payloaddup);

e_cleanup:
	free(msgentry);
	return rc;
}
