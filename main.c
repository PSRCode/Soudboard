#define _GNU_SOURCE 
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>

static int GPIOExport(int pin)
{
#define BUFFER_MAX 3
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

int main(int argc, char** argv) {
	int n;
	int ret;
	int pin[10] = {4,17,27,22,23,24,25,5,6,12};
	int fd_array[10] = {-1};
	char buf = 0;
	char read_buf[64] = {0};
	struct epoll_event events[10];
	int epfd = epoll_create(10);

	for (int i=0; i < sizeof(pin); i++) {
		int fd = -1;
		char *path = NULL;
		struct epoll_event ev;
		ret = GPIOExport(pin[i]);
		if (ret) {
			goto end;
		}
		ret = asprintf(&path, "/sys/class/gpio/gpio%d/value", pin[i]);
		printf("opening %s\n", path);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd < 0) {
			printf("open returned %d: %s\n", n, strerror(errno));
			free(path);
			goto end;
		}
		ev.events = EPOLLPRI;
		ev.data.fd = fd;
		ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
		if (ret) {
			printf("epoll_ctl returned %d: %s\n", n, strerror(errno));
			free(path);
			goto end;
		}

		printf("open returned %d: %s\n", fd, strerror(errno));
		free(path);
	}

	while(1) {
		ret = epoll_wait(epfd, events, 10, -1);
		if (ret < -1) {
			printf("epoll returned %d: %s\n", n, strerror(errno));
			goto end;
		}
		n = ret;
		for (int i=0; i < n; i++) {
			int active_pin = -1;
			int button_position = -1;
			if ((events[i].events & EPOLLERR) ||
					(events[i].events & EPOLLHUP) ||
					(!(events[i].events & EPOLLIN)))
			{
				/* An error has occured on this fd, or the socket is not
				   ready for reading (why were we notified then?) */
				fprintf (stderr, "epoll error\n");
				close (events[i].data.fd);
				continue;
			}
			/* Find the corresponding pin */

			for (int j=0; j < sizeof(pin); j++) {
				if (events[i].data.fd != pin[j]) {
					continue;
				}
				active_pin = pin[j];
				button_position = j;
			}

			if (active_pin < 0) {
				fprintf(stderr, "pin for fd %d not found\n", events[i].data.fd);
				close (events[i].data.fd);
				continue;
			}

			lseek(events[i].data.fd, 0, SEEK_SET);
			n = read(events[i].data.fd, &read_buf, 64);
			printf("Got event on pin %d position %d\n", active_pin, button_position);
		}
	}

end:
	for (int i=0; i < sizeof(pin); i++) {
		ret = GPIOUnexport(pin[i]);
		if (ret) {
			goto end;
		}
		if (fd_array[i] != -1) {
			close(fd_array[i]);
		}
	}

	return(0);
}
