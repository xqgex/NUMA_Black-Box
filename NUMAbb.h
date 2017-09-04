#ifndef NUMABB_HEADER
#define NUMABB_HEADER

#include <cassert>	// assert
#include <errno.h>	// errno
#include <fcntl.h>	// O_RDONLY , open
#include <pthread.h>	// pthread_cond_init , pthread_cond_wait , pthread_cond_signal , pthread_cond_broadcast , 
			// pthread_cond_destroy , pthread_mutex_init , pthread_mutex_lock , pthread_mutex_trylock ,
			// pthread_mutex_unlock , pthread_mutex_destroy , pthread_create , pthread_join , pthread_setaffinity_np
			// pthread_self , pthread_exit , sched_getcpu , cpu_set_t , cpus , CPU_ZERO , CPU_SET
#include <stdbool.h>
#include <stdio.h>	// sprintf , printf
#include <stdlib.h>	// malloc , free
#include <sys/mman.h>	// PROT_READ , MAP_SHARED , MAP_FAILED , mmap , munmap
#include <sys/stat.h>	// fstat
#include <sys/time.h>	// gettimeofday , timersub
#include <unistd.h>	// close , usleep

#include "genericDataStructure.h"

#define MAX_CORES		512			/* The total maximum # of cores in the machine (At all nodes combined)	DEFAULT:512			*/
#define NUM_ENTRIES_IN_LOG	1024*512		/* The size of the shared log						DEFAULT:1024*512		*/
#define MMAP_FILENAME		"./gds_commands.bin"	/* Path to the mmap file						DEFAULT:"./gds_commands.bin"	*/
#define LOG_MIN_SAFE_ZONE	24			/* logMin will be at index (NUM_ENTRIES_IN_LOG - LOG_MIN_SAFE_ZONE)	DEFAULT:24			*/
#define MAX_OP_SIZE		1024			/* Maximum length of 'op' variables in chars				DEFAULT:1024			*/
#define MAX_ARGS_SIZE		1024			/* Maximum length of 'args' variables in chars				DEFAULT:1024			*/
#define LOGFILE_PATH		"./NUMAbb.log"		/* Path to the logfile							DEFAULT:"./NUMAbb.log"		*/
#define LOGFILE_ERROR		true			/* Print to logfile fatal errors					DEFAULT:true			*/
#define LOGFILE_INFORMATION	true			/* Print to logfile general information					DEFAULT:true			*/
#define LOGFILE_PERFORMANCE	false			/* Print to logfile performance information				DEFAULT:false			*/
#define LOGFILE_FUNCTIONS	false			/* Print to logfile when functions start/finish				DEFAULT:false			*/
#define LOGFILE_NOISE		false			/* Print to logfile noisy information about the running of the program	DEFAULT:false			*/
#define LOGFILE_LINE_SIZE	2048			/* The maximum length of line at the performance log			DEFAULT:2048			*/

void* init_numabb(void* args);				/* The function that handle everything :), args is int array of [# physical cpu, # cores per node, hyperthreading] */

#endif
