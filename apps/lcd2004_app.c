#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define CMD_LEN	5

static time_t t;
static struct tm tm;
static const char welcome1[] = "MSE-6co2021-IMD";
static const char welcome2[] = "LCD2004 Driver";
static char * buf[64];

int main(void) {
	int n;

	int my_dev = open("/dev/lcd2004-00", O_RDWR);

	if (my_dev < 0) {
		perror("Fail to open device file: /dev/lcd2004-00.");
	}
	else {
		/* Clear display */
		write(my_dev, "___0", CMD_LEN);

		/* Go to line 1 and write message */
		write(my_dev, "___1", CMD_LEN);
		write(my_dev, welcome1, strlen(welcome1) + 1);

		/* Go to line 2 and write message */
		write(my_dev, "___2", CMD_LEN);
		write(my_dev, welcome2, strlen(welcome2) + 1);

		while(1) {
			/* Get time and format */
			t = time(NULL);
			tm = * localtime(&t);

			/* Go to line 4 and write buffer to device*/
			write(my_dev, "___3", CMD_LEN);
			n = sprintf(buf, "Date: %02d/%02d/%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year);
			write(my_dev, buf, n + 1);

			/* Go to line 4 and write buffer to device*/
			write(my_dev, "___4", CMD_LEN);
			n = sprintf(buf, "Time: %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
			write(my_dev, buf, n + 1);

			sleep(1);
		}
	}

	return 0;
}
