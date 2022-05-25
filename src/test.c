#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

struct th_t {
	uint8_t task;
	uint32_t a2d_size;
	uint32_t d2a_size;
};

int fd = -1;
struct th_t* thing = NULL;

void* myturn(void* arg) {
	while(1) {
		ioctl(fd, 0x7002, thing);
		thing->a2d_size = 250;
		thing->d2a_size = 250;
		ioctl(fd, 0x7001, thing);
	}
}

int main() {
	fd = open("/proc/test-ioctl", O_RDWR);
	thing = malloc(sizeof(struct th_t));
	thing->task = 2;
	thing->a2d_size = 125;
	thing->d2a_size = 125;
	/*pthread_t newthread;
	pthread_create(&newthread, NULL, myturn, NULL);
	while(1) {
		ioctl(fd, 0x7002, thing);
		ioctl(fd, 0x7001, thing);
	}*/
	puts("hello");
	close(fd);
}
