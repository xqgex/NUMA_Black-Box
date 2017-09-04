#include <stdio.h>	// printf
#include <stdlib.h>	// malloc , free

#include "NUMAbb.h"
#include "generateCommands.c"

char* cleanString(char* oldstr) { /* Return string without spaces and new line chars (\n) */
	char* newstr = (char*) malloc(strlen(oldstr)+1); /* The memory will be free at str2int() */
	char* op = oldstr;
	char* np = newstr;
	do {
		if ((*op != ' ')&&(*op != '\n')) {
			*np++ = *op;
		}
	} while (*op++);
	return newstr;
}
int str2int(char* str) { /* Convert string to integer */
	int dec = 0;
	int i = 0;
	int len = 0;
	len = strlen(str);
	for (i=0;i<len;i++) {
		if ((48 <= str[i])&&(str[i] <= 57)) { /* If it's an integer, 48<=char<=57 */
			dec = dec*10 + (str[i]-'0');
		}
	}
	free(str); /* Free the memory allocated at cleanString() */
	return dec;
}
int unique_elements(int* arr, int len) { /* Count unique elements in an array of integers */
	int count = 0;
	int i, j;
	for (i=0;i<len;i++) {
		for (j=0; j<i; j++) {
			if (arr[i] == arr[j]) {
				break;
			}
		}
		if (i == j) { /* No duplicate element found between index 0 to i */
			count++;
		}
	}
	return count;
}
void analyze_pc(int* results) { /* results = [# physical cpu, # cores per node, hyperthreading] */
	char* ptr;
	FILE* fp;
	char* line = NULL;
	size_t len = 0;
	int cpu0cores = 0;
	int coresIDArray[MAX_CORES];
	int coresIDCount = 0;
	results[0] = results[1] = results[2] = 0; /* Double check */
	if ((fp = fopen("/proc/cpuinfo", "rb")) == NULL) {
		return;
	}
	while (getline(&line, &len, fp) != -1) {
		strtok_r(line, ":", &ptr); /* Split line by ':' */
		if ((strncmp(line,"physical id",11) == 0)&&(coresIDCount < 1024)) {
			coresIDArray[coresIDCount] = str2int(cleanString(ptr));
			coresIDCount++;
		} else if ((results[1] == 0)&&(strncmp(line,"siblings",8) == 0)) {
			results[1] = str2int(cleanString(ptr));
		} else if ((cpu0cores == 0)&&(strncmp(line,"cpu cores",9) == 0)) {
			cpu0cores = str2int(cleanString(ptr));
		}
	}
	fclose(fp);
	if (line) {
		free(line);
	}
	results[0] = unique_elements(coresIDArray,coresIDCount);
	results[2] = (cpu0cores != results[1]); /* if '!=' then hyperthreading is ON */
}
int main(void) {
	int cpu_status[3]; /* cpu_status = [# physical cpu, # cores per node, hyperthreading] */
	pthread_t thread[2];
	cpu_status[0] = cpu_status[1] = cpu_status[2] = 0;
	printf("\e[1;1H\e[2J"); /* Clear screen */
	printf("Analyzing your machine...\n");
	analyze_pc(cpu_status);
	printf("\n");
	printf("Nodes (physical cpu's):        %d\n", cpu_status[0]);
	printf("Cores per node:                %d\n", cpu_status[1]);
	if (cpu_status[2]) {
		printf("Hyperthreading:                ON\n");
	} else {
		printf("Hyperthreading:                OFF\n");
	}
	printf("\n");
	printf("Example:\n");
	printf("             Node #1                              Node #2             \n");
	printf("╔═══════╦═══════╦═══════╦═══════╗    ╔═══════╦═══════╦═══════╦═══════╗\n");
	printf("║ Core  ║ Core  ║ Core  ║ Core  ┣━━━━┫ Core  ║ Core  ║ Core  ║ Core  ║\n");
	printf("╟───────╫───────╫───────╫───────╊━━━━╉───────╫───────╫───────╫───────╢\n");
	printf("║ Cache ║ Cache ║ Cache ║ Cache ┣━━━━┫ Cache ║ Cache ║ Cache ║ Cache ║\n");
	printf("╟───────╨───────╨───────╨───────╢    ╟───────╨───────╨───────╨───────╢\n");
	printf("║             Cache             ║    ║             Cache             ║\n");
	printf("╚═════════════┳━┳━┳═════════════╝    ╚═════════════┳━┳━┳═════════════╝\n");
	printf("              ┃ ┃ ┃                                ┃ ┃ ┃              \n");
	printf("┌─────────────┺━┻━┹─────────────┐    ┌─────────────┺━┻━┹─────────────┐\n");
	printf("│            Memory             │    │            Memory             │\n");
	printf("└───────────────────────────────┘    └───────────────────────────────┘\n");
	printf("\n");
	printf("Press Any Key to Start Simulation\n");
	getchar();
	if (pthread_create(&thread[0], NULL, generateCommands, NULL) == 0) { /* On success, returns 0; on error, it returns an error number, and the contents of *thread are undefined. */ /* https://linux.die.net/man/3/pthread_create */
	} else {
		perror("can't create thread");
	}
	if (pthread_create(&thread[1], NULL, init_numabb, (void*)&cpu_status) == 0) { /* On success, returns 0; on error, it returns an error number, and the contents of *thread are undefined. */ /* https://linux.die.net/man/3/pthread_create */
	} else {
		perror("can't create thread");
	}
	pthread_join(thread[0], NULL); /* On success, returns 0; on error, it returns an error number. */ /* https://linux.die.net/man/3/pthread_join */
	pthread_join(thread[1], NULL); /* On success, returns 0; on error, it returns an error number. */ /* https://linux.die.net/man/3/pthread_join */
	return 0;
}
