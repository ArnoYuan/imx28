#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>


int main(void)
{
    int fd = open("/dev/button", O_RDONLY);
    int i = 0;
    char buf[20];

    int ret = read(fd, buf, sizeof(buf));
    if (ret <= 0)
    {
        printf("no key pressed, ret=%d\n", ret);
        return ret;
    }
    printf("key pressed:\n");
    for (i = 0; i < ret; i++)
    {
        printf("%d ", buf[i]);
    }
    printf("\n");

    return 0;
}