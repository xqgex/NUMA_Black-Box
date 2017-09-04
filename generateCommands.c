#include <fcntl.h>	// O_RDWR , O_CREAT , O_TRUNC , open
#include <stdio.h>	// perror , sprintf , printf
#include <stdlib.h>	// RAND_MAX , EXIT_FAILURE , malloc , free , rand , srand , exit
#include <string.h>	// strcpy
#include <sys/mman.h>	// PROT_WRITE , MAP_SHARED , MAP_FAILED , MS_SYNC , mmap , msync , munmap
#include <time.h>	// time
#include <unistd.h>	// lseek , close , write , usleep

#define FILE_LENGTH		65535			/* The size of the mmap file		DEFAULT:65535			*/
#define MMAP_FILENAME		"./gds_commands.bin"	/* Path to the mmap file		DEFAULT:"./gds_commands.bin"	*/

char* COMMANDS[10];

int random_range(unsigned const low, unsigned const high) {/* Return a uniformly random number in the range [low,high]. */
	unsigned const range = high - low + 1;
	return low + (int) (((double) range) * rand () / (RAND_MAX + 1.0));
}
void* generateCommands(void* unused) {
	/* This part MUST end as fast as possible... */
	int fd;
	char* file_memory;
	if ((fd = open(MMAP_FILENAME, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0777)) == -1) { /* Prepare a file large enough to hold an unsigned integer. */
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}
	if (lseek(fd, FILE_LENGTH-1, SEEK_SET) == -1) { /* Stretch the file size to the size of the (mmapped) array of char */
		close(fd);
		perror("Error calling lseek() to 'stretch' the file");
		exit(EXIT_FAILURE);
	}
	if (write(fd, "", 1) == -1) {
		close(fd);
		perror("Error writing last byte of the file");
		exit(EXIT_FAILURE);
	}
	if ((file_memory = (char*)mmap(0, FILE_LENGTH, PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) { /* Create the memory mapping. */
		close(fd);
		perror("Error mmapping the file");
		exit(EXIT_FAILURE);
	}
	close(fd);
	/* Now we can "take our time" to generate the commands */
	int i = 1;
	int command_length = 0;
	int current_length = 0;
	char* command = (char*) malloc(1024 * sizeof(char));
	srand(time(NULL)); /* Seed the random number generator */
	COMMANDS[0] = (char*)"add";	/* add */
	COMMANDS[1] = (char*)"add";	/* add */
	COMMANDS[2] = (char*)"add";	/* add */
	COMMANDS[3] = (char*)"add";	/* add */
	COMMANDS[4] = (char*)"remove";	/* add */
	COMMANDS[5] = (char*)"remove";	/* remove */
	COMMANDS[6] = (char*)"remove";	/* remove */
	COMMANDS[7] = (char*)"remove";	/* remove */
	COMMANDS[8] = (char*)"remove";	/* display */
	COMMANDS[9] = (char*)"reverse";	/* reverse */
	while (current_length < FILE_LENGTH) {
		command_length = sprintf(command,"%s|%d\n",COMMANDS[random_range(0,9)],random_range(0,255)); /* Generate the command: "op|args" */
		if (current_length + command_length < FILE_LENGTH) {
			strcpy(file_memory+current_length,command);
		} else {
			file_memory[current_length] = 0x1B;
			file_memory[current_length+1] = 0x00;
		}
		current_length += command_length;
		if (msync(file_memory, FILE_LENGTH, MS_SYNC) == -1) { /* Write it now to disk */
			perror("Could not sync the file to disk");
		}
		usleep(random_range(0,5));
		i++;
	}
	munmap(file_memory, FILE_LENGTH); /* Release the memory */
	free(command);
	return 0;
}
