#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>

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

    //scanf("%s", Ubuff);

    /* Write the data into the device */
    write(fd , Ubuff , strlen(Ubuff) + 1);

    close(fd);  
    return 0;
}
