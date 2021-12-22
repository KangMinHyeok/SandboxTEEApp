#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define DEFAULT_SPI_MODE		SPI_MODE_0
#define DEFAULT_SPI_LSB_FIRST		0
#define DEFAULT_SPI_BITS_PER_WORD	8
#define DEFAULT_SPI_MAX_SPEED_HZ	1350000 // 1.35 (MHz)

int mcp3004_open(const char * devname, int spimode, int lsbfirst, unsigned bits_per_word, uint32_t max_speed_hz){
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

	if(buf != spimode){
		*bytebuf = spimode;

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

	if(!!*bytebuf != !!lsbfirst){
		*bytebuf = lsbfirst;

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

	if(*bytebuf != bits_per_word){
		*bytebuf = bits_per_word;

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

	if(buf != max_speed_hz){
		buf = max_speed_hz;

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
