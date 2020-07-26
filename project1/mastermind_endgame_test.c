#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "mastermind_ioctl.h"

int main(int argc, char *argv[]){

	int fd = open("/dev/mastermind", O_RDWR);
	
	int status = ioctl(fd, MASTERMIND_MMIND_ENDGAME);
	
	if(status == 0){
		printf("The new game has been finished!\n");
	}else{
		printf("The error has been occured with error number: %d\n", status);
	}
	

	return 0;
}
