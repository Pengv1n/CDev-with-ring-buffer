#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include <sys/ioctl.h>

#define GET_LAST_OP _IO('L', 1)

int main()
{
    int fd;
    char Ubuff[50], Kbuff[50];

    fd = open("/dev/circ_cdev0", O_RDWR);
    if(fd < 0) {
        perror("Unable to open the device file\n");
        return -1;
    }

    /* Read the data back from the device */
    memset(Kbuff , 0 ,sizeof(Kbuff));
    //read(fd , Kbuff , sizeof(Kbuff));
    //printf("Data from kernel : %s\n", Kbuff);

    ioctl(fd, GET_LAST_OP, Kbuff);
    printf("%s\n", Kbuff);

    close(fd);
    return 0;
}
