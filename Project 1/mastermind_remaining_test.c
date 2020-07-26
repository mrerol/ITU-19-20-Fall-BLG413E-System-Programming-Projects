#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "mastermind_ioctl.h"

int main(int argc, char *argv[]){

	int fd = open("/dev/mastermind", O_RDWR);
	
	int remaining = ioctl(fd, MASTERMIND_MMIND_REMAINING);
	
	if(remaining < 0){
		printf("The error has been occuredwith error number: %d\n", remaining);
	}else{
		printf("The number of remaining guesses: %d\n", remaining);
	}
	
	
	

	return 0;
}
