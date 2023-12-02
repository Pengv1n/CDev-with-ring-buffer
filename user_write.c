#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include <sys/ioctl.h>

#define IONBLOCK _IO('L', 0) 

int main()
{
    int fd;
    char Kbuff[50];
    //char Ubuff[50];

    fd = open("/dev/circ_cdev0", O_RDWR);
    if(fd < 0) {
        perror("Unable to open the device file\n");
        return -1;
    }

    char Ubuff[50] = "Hellomeis";
	
    ioctl(fd, IONBLOCK, 1);

    write(fd, Ubuff, strlen(Ubuff) + 1);
    write(fd, Ubuff, strlen(Ubuff) + 1);
    write(fd, Ubuff, strlen(Ubuff) + 1);

    //ioctl(fd, IONBLOCK, 1);

    close(fd);  
    return 0;
}
