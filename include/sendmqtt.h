#ifndef _SENDMQTT_H_
#define _SENDMQTT_H_

#include <stdlib.h>
#include <MQTTAsync.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void * mqttsender_handle_t;

int mqttsender_init(mqttsender_handle_t * Phandle, const char * address, const char * clientid, 
		const char * username, const char * password, 
		int use_tls, const char * capath, int insecure_tls);

int mqttsender_send(mqttsender_handle_t handle, const char * topic, void * payload, size_t payloadlen);

int mqttsender_join(mqttsender_handle_t handle, unsigned long timeout_ms);

int mqttsender_end(mqttsender_handle_t handle);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SENDMQTT_H_ */
