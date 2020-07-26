#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "mastermind_ioctl.h"

int main(int argc, char *argv[]){

	if(argc == 1){
		printf("Missing argument! Enter magic number as parameter!\n");
		return -1;
	}
	
	int fd = open("/dev/mastermind", O_RDWR);
	
	int status = ioctl(fd, MASTERMIND_MMIND_NEWGAME, argv[1]);

	if(status == 0){
		printf("The new game has been started with magic number: %s\n", argv[1]);
	}else{
		printf("The error has been occured with error number: %d\n", status);
	}
	
	

	return 0;
}
