#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>


int main(void)
{
    int level = 0;
    int fd = open("/dev/led", O_RDWR);
    if (fd < 0)
    {
        perror("led open failed.");
        return -1;
    }
    printf("test write ...\n");
    write(fd, (const void*)&level, sizeof(level));
    sleep(2);

    level = 1;
    write(fd, (const void*)&level, sizeof(level));
    sleep(1);

    printf("test ioctl...\n");
    ioctl(fd, 0);
    sleep(2);
    ioctl(fd, 1);
    close(fd);

    return 0;
}