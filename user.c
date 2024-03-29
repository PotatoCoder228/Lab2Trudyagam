#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define WR_VALUE _IOW('w', 0xEEE, int32_t *)
#define RD_VALUE _IOR('r', 0xFFF, int32_t *)

int main(int argc, char **argv) {
  int fd = open("/dev/etx_device", O_RDWR);
  uint64_t speed[4] = {};
  if (fd != -1) {
    for (int i = 0; i < 10; i++) {
      ioctl(fd, RD_VALUE, &speed);
      printf("Reading iops: %lu\n", speed[0]);
      printf("Reading speed [KBs]: %lu\n", speed[1]);
      printf("Writing iops: %lu\n", speed[2]);
      printf("Writing speed [KBs]: %lu\n", speed[3]);
      printf("<------------------------------------------->\n");
    }
    close(fd);
  } else
    printf("%s. %s\n", "Can't open the file", strerror(errno));
  return 0;
}