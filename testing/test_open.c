#include "stdio.h"
#include "stdlib.h"
#include "fcntl.h"
#include <unistd.h>
#include "assert.h"

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("usage: %s <filename> <delay(s)>\n", argv[0]);
        exit(1);
    }

    char *filename = argv[1];
    int delay = atoi(argv[2]);

    printf("open fd1\n");
    int fd1 = open(filename, O_RDWR);
    assert(fd1 > 0);

    sleep(delay);

    printf("open fd2\n");
    int fd2 = open(filename, O_RDWR);
    assert(fd2 > 0);

    sleep(delay);

    printf("write fd1\n");
    int ret = write(fd1, "abcde", 5);
    assert(ret == 5);

    sleep(delay);

    printf("write fd2\n");
    ret = write(fd2, "lmnopq", 6);
    assert(ret == 6);

    sleep(delay);

    printf("close fd1\n");
    ret = close(fd1);
    assert(ret == 0);

    sleep(delay);

    printf("write fd2 again\n");
    ret = write(fd2, "1234567", 7);
    assert(ret == 7);

    sleep(delay);

    printf("close fd2\n");
    ret = close(fd2);
    assert(ret == 0);
}