#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <time.h>

#define DEFAULT_SPI_MODE		SPI_MODE_0
#define DEFAULT_SPI_LSB_FIRST		0
#define DEFAULT_SPI_BITS_PER_WORD	8
#define DEFAULT_SPI_MAX_SPEED_HZ	1350000 // 1.35 (MHz)

int fd;

int init(){
	int rc, fd;
	uint32_t buf;
	unsigned char * bytebuf = (unsigned char *)(&buf);

	rc = open("/dev/spidev0.0", O_RDWR);
	if(rc < 0){
		perror("failed to open device");
		goto e_exit;
	}

	fd = rc;
	
	rc = ioctl(fd, SPI_IOC_RD_MODE, bytebuf);
	if(rc < 0){
		perror("failed to read mode");
		goto e_cleanup;
	}

	if(buf != SPI_MODE_0){
		*bytebuf = DEFAULT_SPI_MODE;

		rc = ioctl(fd, SPI_IOC_WR_MODE, bytebuf);
		if(rc < 0){
			perror("failed to set mode");
			goto e_cleanup;
		}
	}

	rc = ioctl(fd, SPI_IOC_RD_LSB_FIRST, bytebuf);
	if(rc < 0){
		perror("failed to read LSB mode");
		goto e_cleanup;
	}

	if(!!*bytebuf != !!DEFAULT_SPI_LSB_FIRST){
		*bytebuf = DEFAULT_SPI_LSB_FIRST;

		rc = ioctl(fd, SPI_IOC_WR_LSB_FIRST, bytebuf);
		if(rc < 0){
			perror("failed to set LSB mode");
			goto e_cleanup;
		}
	}

	rc = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, bytebuf);
	if(rc < 0){
		perror("failed to read bits per word configuration");
		goto e_cleanup;
	}

	if(*bytebuf != DEFAULT_SPI_BITS_PER_WORD){
		*bytebuf = DEFAULT_SPI_BITS_PER_WORD;

		rc = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, bytebuf);
		if(rc < 0){
			perror("failed to set bits per word configuration");
			goto e_cleanup;
		}
	}

	rc = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &buf);
	if(rc < 0){
		perror("failed to read max speed configuration");
		goto e_cleanup;
	}

	if(buf != DEFAULT_SPI_MAX_SPEED_HZ){
		buf = DEFAULT_SPI_MAX_SPEED_HZ;

		rc = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &buf);
		if(rc < 0){
			perror("failed to set max speed configuration");
			goto e_cleanup;
		}
	}

	return fd;

e_cleanup:
	if(close(fd) < 0){
		perror("failed to close device");
	}

e_exit:
	return rc;
}

int readvoltage(unsigned channel){
#define	TRANSFER_LEN	3

	int rc, voltage;
	unsigned char tx_buf[TRANSFER_LEN] = {}, rx_buf[TRANSFER_LEN] = {};
	struct spi_ioc_transfer xfer_data = {
		.tx_buf		= (unsigned long) tx_buf,
		.rx_buf		= (unsigned long) rx_buf,
		.len		= (size_t) TRANSFER_LEN,
	};

	if(channel >= 4){
		printf("spi channel %u not available\n", channel);
		return -1;
	}

	// set start bit
	tx_buf[0] = (1 << 0);
	// set module specific read info
	tx_buf[1] = ((1 << 7) | (channel << 4)); // TODO: setup value

	rc = ioctl(fd, SPI_IOC_MESSAGE(1), &xfer_data);

	if(rc < 0){
		perror("spi xfer operation failed");
		return rc;
	}

	voltage = (((rx_buf[1] & 0x03) << 8) | (rx_buf[2]));
	return voltage;

#undef	TRANSFER_LEN
}

int deinit(){
	return close(fd);
}

uint64_t gettime_monotone(){
	int rc;
	struct timespec now;

	rc = clock_gettime(CLOCK_MONOTONIC, &now);
	if(rc < 0){
		perror("failed to fetch time");
		return 0;
	}

	return (uint64_t) now.tv_sec * 1000000000 + now.tv_nsec;
}

int main(){
	int rc, voltage;

	rc = init();

	if(rc < 0){
		perror("failed to open spi device");
		return rc;
	}

	fd = rc;

//	printf("TODO: setup spi signal to read adc value\n");
//	printf("Consult at: https://ww1.microchip.com/downloads/en/DeviceDoc/21295d.pdf\n");

	for(int k=0;k<4;++k){
		uint64_t before, after, elapsed;
		
		before = gettime_monotone();
		voltage = readvoltage(k);
		after = gettime_monotone();

		if(!before || !after) elapsed = 0;
		else elapsed = after - before;
		
		if(voltage >= 0){
			printf("read channel %d voltage %d elapsed time %llu [ns]\n", k, voltage, elapsed);
		}
	}
	

	rc = deinit();
	if(rc < 0){
		perror("failed to close spi device");
		return rc;
	}
}
