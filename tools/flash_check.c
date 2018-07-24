#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define BUFLEN 65536

int main(int argc, char **argv)
{
    int handle, result;
    char buffer[BUFLEN];
    char test_buffer[BUFLEN];

    if(argc != 2)
    {
        //printf("Usage: %s /dev/mtd# (# must be 0, 1, 2, or 3)\n", argv[0]);
        return -1;
    }

    if(strncmp(argv[1], "/dev/mtd0", 9) != 0)
    {
        if(strncmp(argv[1], "/dev/mtd1", 9) != 0)
        {
            if(strncmp(argv[1], "/dev/mtd2", 9) != 0)
            {
                if(strncmp(argv[1], "/dev/mtd3", 9) != 0)
                {
                    //printf("Usage: %s /dev/mtd# (# must be 0, 1, 2, or 3)\n", argv[0]);
                    return -1;
                }
            }
        }
    }

    /* Open /dev/mtd */
    if((handle = open(argv[1], O_RDONLY)) <= 0)
    {
        //printf("Failed to open file, %d:%s\n", errno, strerror(errno));
        return -1;
    }

    memset(test_buffer, 0xff, BUFLEN);

    while((result = read(handle, buffer, BUFLEN)) > 0)
    {
        if(memcmp(buffer, test_buffer, result) != 0)
        {
            //printf("Memory not clear\n");
            close(handle);
            return -1;
        }
    }

    /* Got to this point, memory must be clear */
    //printf("Memory clear\n");
    close(handle);
    return 0;
}
