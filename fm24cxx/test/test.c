#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>



int main(void)
{

    char* data = "hello world";
    int fd = open("/dev/fm24c02", O_RDWR);
    lseek(fd, 0, SEEK_SET);
    write(fd, data, strlen(data));
    lseek(fd, 0, SEEK_SET);
    char buf[50];
    read(fd, buf, strlen(data));

    printf("read data: %s\n", buf);


    return 0;
}