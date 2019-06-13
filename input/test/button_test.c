#include <linux/input.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



int main(int argc, char* const argv[])
{
    int fd = 0;

    struct input_event event;

    int ret = 0;

    fd = open("/dev/input/event1", O_RDONLY);

    while (1)
    {
        printf(">>> read event\n");
        ret = read(fd, &event, sizeof(event));
        printf(">>>type=%d, code=%d, value=%d\n", event.type, event.code, event.value);
        sleep(0.1);
    }

    return 0;
}