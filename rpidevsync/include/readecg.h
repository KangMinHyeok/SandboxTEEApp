#ifndef _READECG_H_
#define _READECG_H_

#include <stdint.h>
#include <linux/spi/spidev.h>

#ifdef __cplusplus
extern "C" {
#endif

int mcp3004_open(const char * devname, int spimode, int lsbfirst, unsigned bits_per_word, uint32_t max_speed_hz);
int mcp3004_readvalue(int fd, unsigned channel);
uint64_t mcp3004_value_to_voltage(uint32_t voltage);
int mcp3004_close(int fd);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _READECG_H_ */
