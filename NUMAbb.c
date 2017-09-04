#define __USE_GNU 1
#define __USE_UNIX98 1
#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE (200112L)
#include "NUMAbb.h"

/************	Structures Declaration	*********************/
struct s_entries { /* Data structure for log entries (shared log and slots) */
	int		index;			/* The index of the entry in the shared log */
	bool		rounds;			/* Bit that indicate when the shared log wrap itself */
	bool		filled;			/* Reserved bit in the slot used to marks that the slot is filled */
	char*		op;			/* The command */
	void*		args;			/* The command arguments */
	s_entries*	next;			/* Pointer to the next row in the log */
};
struct s_coreMemory { /* Memory that is private foreach core */
	int		myCPUID;		/* Save the CPU ID, from sched_getcpu() */
	int		myNode;			/* Save the node ID we get from init_numabb() */
	int		myCore;			/* Save the core ID we get from init_numabb() */
	bool		response;		/* The response from the commands the thread put in the slot */
	bool		terminate;		/* Remote commands for the thread to finish his work */
	pid_t		myThreadID;		/* Save the thread ID, from pthread_self() */
	s_entries	slot;			/* The location where threads post operations for the combiners */
	GenericDS*	gds;			/* gds (GlobalDataStructure) is a struct with pointers to the API public functions */
	s_coreMemory*	nextCore;		/* Pointer to the next core inside the node */
};
struct s_nodeMemory { /* Memory that is shared among all the node cores */
	int		batchSize;		/* How many cores are in batch */
	int		replicaInitialized;	/* Indicate the status of the replica, 0-uninitialized, 1-initialized, 2-destroyed */
	int		combinerInitialized;	/* Indicate the status of the lock,    0-uninitialized, 1-initialized, 2-destroyed */
	int		rwInitialized;		/* Indicate the status of the lock,    0-uninitialized, 1-initialized, 2-destroyed */
	void*		replica;		/* Each node have his own replica of the data structure */
	s_entries*	localTail;		/* Indicates the next operation in the log to be executed on each local replica (how far in the log the replica has
been updated) */
	s_coreMemory*	firstCore;		/* Pointer to one of the cores belong to that node (for looping purposes) */
	s_coreMemory*	batch[MAX_CORES];	/* 'batch' is an array of pointers to cores, used by the combiner */
	s_nodeMemory*	nextNode;		/* Pointer to the next node */
	pthread_cond_t	combinerCond;		/* The condition to signal threads that wait for the combiner lock */
	pthread_mutex_t	combinerLock;		/* The lock for the thread that is the combiner right now */
	pthread_mutex_t	rLock;			/* Read lock, when it's looked, only read operation can occurs */
	pthread_mutex_t	wLock;			/* Write lock, This lock both read wnd write operations */
};
struct s_sharedMemory { /* Memory that is shared among all nodes and cores */
	int		activeCores;		/* How many cores are currently working */
	int		maxBatch;		/* The maximum size of the batch array */
	int		logMinLowMark;		/* The index where to re-calculate log min */
	int		logMinUpdating;		/* Kind of a 'lock' to indicate the log min update process */
	int		mmapFd;			/* The file descriptor for the mmap file */
	int		mmapPointer;		/* How much we read from the shared memory so far */
	bool		logRounds;		/* Bit that indicate when the shared log wrap itself */
	char*		mmapMap;		/* The shared memory */
	size_t		mmapSize;		/* The size of the shared memory */
	timeval		tval_before;		/* Logfile time - Start of the program */
	timeval		tval_after;		/* Logfile time - Time at each tick */
	timeval		tval_result;		/* Logfile time - Diff from program start */
	s_entries*	sharedLog;		/* Across nodes, threads coordinate through the shared log */
	s_entries*	logMin;			/* The last known safe location to write */
	s_entries*	logTail;		/* Indicates the first unreserved entry in the log (the index of the next available entry) */
	s_entries*	completedTail;		/* Points to the log entry after which there are no completed operations (<=logTail) */
	s_nodeMemory*	firstNode;		/* Pointer to one of the nodes (for looping purposes) */
	pthread_cond_t	globalCond;		/* Global lock shared by all threads, Use only to init & destroy of the threads */
	pthread_mutex_t	globalMtx;		/* Global cond shared by all threads, Use only to init & destroy of the threads */
	pthread_mutex_t	globalMtxDestroy;	/* Global cond shared by all threads, Use only to init & destroy of the threads */
	pthread_mutex_t	mmapLock;		/* lock for the thread that read new commands from the mmap */
};
struct s_threadId { /* This structure is used to pass data into each thread at the initialization */
	int		ready;			/* Indicate when thread finish loading, in case the condition wait gets interrupted */
	s_nodeMemory*	nodeMemory;		/* Pointer to the shared memory of the core parent node */
	s_coreMemory*	coreMemory;		/* Pointer to the private memory that will be used by the core */
};

/************	Variables Declaration	*********************/
char		line_temp[LOGFILE_LINE_SIZE+5];	/* Temp variable for log file lines */
FILE*		LOGFILE_FD;			/* The log file */
s_sharedMemory	SHARED_MEMORY;			/* Shared memory */
s_nodeMemory*	NODE_MEMORY;			/* Node memory (Array, 1 for each node) */
s_coreMemory*	CORE_MEMORY;			/* Core memory (Array, 1 for each core) */

/************	Functions Declaration	*********************/
/************	Used For Support	*********************/
void* bindToCore(void *arg);			/* Each core have a thread attached to her with that function */
s_entries* initSharedLog();			/* Create the shared log */
void destroySharedLog();			/* Destroy the shared log and free resources */
int commandsOpen();				/* Open mmap, On success, zero is returned. On error, -1 */
int commandsRead(char** op,void** args);	/* Read mmap, If found command return 1, otherwise zero is returned. On error, -1 */
int commandsClose();				/* Close mmap, On success, zero is returned. On error, -1 */
void log_to(const char* line);			/* Write to logfile the input line */
void printLog(int from,int to);			/* Print section of the shared log between two indexes */
bool compareEntries(s_entries* a,s_entries* b);	/* Compare relation between two entries at the log, return true if a<b and false o.w */
/************	Functions Declaration	*********************/
/************	Heart Of The BlackBox	*********************/
void updateFromLog(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,s_entries* to);
s_entries* reserveLogEntries(int n);
s_entries* appendToLog(int n,s_coreMemory* batch);
bool combine(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,char* op,void* args);
bool readOnly(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,char* op,void* args);
bool executeConcurrent(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,char* op,void* args);

/************************************************************/
/************	Public Functions	*********************/
/************************************************************/
void* init_numabb(void* args) { /* The function that handle everything :), args is int array of [# physical cpu, # cores per node, hyperthreading] */
	if ((LOGFILE_FD = fopen(LOGFILE_PATH,"a")) == NULL) { /* Upon successful completion return a FILE pointer. Otherwise, NULL is returned and errno is set to indicate the error. */ /* https://linux.die.net/man/3/fopen */
		printf("Error open log file: %s.\n",strerror(errno));
		return (void*)-1;
	}
	if (gettimeofday(&SHARED_MEMORY.tval_before, NULL) == -1) { /* Logfile time - Start of the program */ /* return 0 for success, or -1 for failure (in which case errno is set appropriately). */ /* https://linux.die.net/man/2/gettimeofday */
		printf("Error getting local time: %s.\n",strerror(errno));
		return (void*)-1;
	}
	if (LOGFILE_INFORMATION) {
		log_to("=============================================================");
		log_to("Markers: (--) probed, (**) from config file, (==) default setting,");
		log_to("(++) from command line, (!!) notice, (II) informational,");
		log_to("(WW) warning, (EE) error, (NI) not implemented, (\?\?) unknown.");
		log_to("=============================================================");
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - init start");}
	int* cpu_status = (int*)args;
	/* Function variables */
	int i,j;
	int pthreadReturnValue;
	char localLineTemp[LOGFILE_LINE_SIZE+5];
	pthread_t thread[cpu_status[0]*cpu_status[1]];
	cpu_set_t cpus;
	s_coreMemory* loopNextCore;
	s_nodeMemory* loopNextNode = NULL;
	s_threadId threadDetails; /* Struct to pass info to the thread, One structure for all the threads */
	/* init variables */
	if ((pthreadReturnValue = pthread_cond_init(&SHARED_MEMORY.globalCond, NULL)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_init */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Error initializing condition (globalCond): %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	if ((pthreadReturnValue = pthread_mutex_init(&SHARED_MEMORY.globalMtx, NULL)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_init */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Error initializing mutex (globalMtx): %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	if ((pthreadReturnValue = pthread_mutex_init(&SHARED_MEMORY.globalMtxDestroy, NULL)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_init */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Error initializing mutex (globalMtxDestroy): %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	/* Setup shared memory */
	SHARED_MEMORY.activeCores = 0;					/* How many cores are currently working */
	SHARED_MEMORY.maxBatch = cpu_status[1];				/* The maximum size of the batch array */
	SHARED_MEMORY.logMinLowMark = NUM_ENTRIES_IN_LOG - LOG_MIN_SAFE_ZONE - SHARED_MEMORY.maxBatch;
	SHARED_MEMORY.logMinUpdating = 0;				/* Kind of a 'lock' to indicate the log min update process */
	SHARED_MEMORY.mmapFd = -1;					/* The file descriptor for the mmap file */
	SHARED_MEMORY.logRounds = true;					/* Bit that indicate when the shared log wrap itself */
	SHARED_MEMORY.sharedLog = initSharedLog();			/* Across nodes, threads coordinate through the shared log */
	SHARED_MEMORY.logMin = SHARED_MEMORY.sharedLog;			/* The last known safe location to write */
	SHARED_MEMORY.logTail = SHARED_MEMORY.sharedLog;		/* Indicates the first unreserved entry in the log */
	SHARED_MEMORY.completedTail = SHARED_MEMORY.sharedLog;		/* Points to the log entry after which there are no completed operations (<=logTail) */
	/* Create array in size of # nodes & cores */
	NODE_MEMORY = (s_nodeMemory*) malloc(cpu_status[0] * sizeof(s_nodeMemory));
	CORE_MEMORY = (s_coreMemory*) malloc(cpu_status[0] * cpu_status[1] * sizeof(s_coreMemory));
	/* Init those variables (Node & core memory) */
	for (i=0;i<cpu_status[0];i++) { /* Foreach node */
		loopNextCore = NULL;
		((NODE_MEMORY + i)->batchSize) = 0;				/* How many cores are in batch */
		((NODE_MEMORY + i)->replica) = NULL;				/* Each node have his own replica of the data structure */
		((NODE_MEMORY + i)->localTail) = SHARED_MEMORY.sharedLog;	/* Indicates the next operation in the log to be executed on each local replica */
		((NODE_MEMORY + i)->replicaInitialized) = 0;			/* Indicate the status of the replica, 0-uninitialized, 1-initialized, 2-destroyed */
		((NODE_MEMORY + i)->combinerInitialized) = 0;			/* Indicate the status of the lock, 0-uninitialized, 1-initialized, 2-destroyed */
		((NODE_MEMORY + i)->rwInitialized) = 0;				/* Indicate the status of the lock, 0-uninitialized, 1-initialized, 2-destroyed */
		((NODE_MEMORY + i)->nextNode) = loopNextNode;			/* Pointer to the next node */
		for (j=0;j<cpu_status[1];j++) { /* Foreach core */
			(NODE_MEMORY + i)->batch[j] = NULL;				/* 'batch' is an array of pointers to cores, used by the combiner */
			((CORE_MEMORY + (i*cpu_status[1])+j)->myCPUID) = -1;		/* Save the CPU ID, from sched_getcpu() */
			((CORE_MEMORY + (i*cpu_status[1])+j)->myNode) = -1;		/* Save the node ID we get from init_numabb() */
			((CORE_MEMORY + (i*cpu_status[1])+j)->myCore) = -1;		/* Save the core ID we get from init_numabb() */
			((CORE_MEMORY + (i*cpu_status[1])+j)->response) = false;	/* The response from the commands the thread put in the slot */
			((CORE_MEMORY + (i*cpu_status[1])+j)->terminate) = false;	/* Remote commands for the thread to finish his work */
			((CORE_MEMORY + (i*cpu_status[1])+j)->myThreadID) = -1;		/* Save the thread ID, from pthread_self() */
			((CORE_MEMORY + (i*cpu_status[1])+j)->slot.filled) = false;	/* The location where threads post operations for the combiners */
			((CORE_MEMORY + (i*cpu_status[1])+j)->slot.op) = (char*) malloc(MAX_OP_SIZE * sizeof(char));	/* The location where threads post operations for the combiners */
			((CORE_MEMORY + (i*cpu_status[1])+j)->slot.args) = (char*) malloc(MAX_ARGS_SIZE * sizeof(char));/* The location where threads post operations for the combiners */
			((CORE_MEMORY + (i*cpu_status[1])+j)->gds) = NULL;		/* gds (GlobalDataStructure) is a struct with pointers to the API public functions */
			((CORE_MEMORY + (i*cpu_status[1])+j)->nextCore) = loopNextCore;	/* Pointer to the next core inside the node */
			loopNextCore = (CORE_MEMORY + (i*cpu_status[1])+j);
		}
		((NODE_MEMORY + i)->firstCore) = loopNextCore;	/* Pointer to one of the cores belong to that node (for looping purposes) */
		loopNextNode = (NODE_MEMORY + i);
	}
	SHARED_MEMORY.firstNode = loopNextNode; /* Pointer to one of the nodes (for looping purposes) */
	if (commandsOpen() == -1) { /* Open mmap, On success, zero is returned. On error, -1 */
		if (LOGFILE_ERROR) {log_to("(EE) Program terminated after mmap opening error.");}
		return (void*)-1;
	}
	/* Create threads */
	for (i=0;i<cpu_status[0];i++) { /* Foreach node */
		for (j=0;j<cpu_status[1];j++) { /* Foreach core */
			threadDetails.ready = 0;
			(CORE_MEMORY + (i*cpu_status[1])+j)->myNode = i;
			(CORE_MEMORY + (i*cpu_status[1])+j)->myCore = j;
			threadDetails.nodeMemory = NODE_MEMORY + i;
			threadDetails.coreMemory = CORE_MEMORY + (i*cpu_status[1])+j;
			if ((pthreadReturnValue = pthread_create(&thread[(i*cpu_status[1])+j], NULL, bindToCore, (void*)&threadDetails)) == 0) { /* On success, returns 0; on error, it returns an error number, and the contents of *thread are undefined. */ /* https://linux.die.net/man/3/pthread_create */
				CPU_ZERO(&cpus); /* Clears set, so that it contains no CPUs. */ /* https://linux.die.net/man/3/cpu_zero */
				CPU_SET((i*cpu_status[1])+j, &cpus); /* Add CPU cpu to set. */ /* https://linux.die.net/man/3/cpu_set */
				if ((pthreadReturnValue = pthread_setaffinity_np(thread[(i*cpu_status[1])+j], sizeof(cpu_set_t), &cpus)) != 0) { /* On success, return 0; on error, return a nonzero error number. */ /* https://linux.die.net/man/3/pthread_setaffinity_np */
					if (LOGFILE_ERROR) {
						sprintf(localLineTemp,"(EE) Pthread setaffinity failed: %s",strerror(pthreadReturnValue));
						log_to(localLineTemp);
					}
					return (void*)-1;
				}
				while (!threadDetails.ready) {/* Wait on the condition until the ready flag is set */
					if ((pthreadReturnValue = pthread_cond_wait(&SHARED_MEMORY.globalCond, &SHARED_MEMORY.globalMtx)) != 0) { /* Upon successful completion, a value of zero shall be returned; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_wait */
						if (LOGFILE_ERROR) {
							sprintf(localLineTemp,"(EE) Pthread condition (globalCond) wait failed: %s",strerror(pthreadReturnValue));
							log_to(localLineTemp);
						}
						return (void*)-1;
					}
				}
				if (LOGFILE_INFORMATION) {
					sprintf(localLineTemp,"(II) Thread created successfully at CPU:%d, Node:%d, Core:%d",(i*cpu_status[1])+j,i,j);
					log_to(localLineTemp);
				}
			} else {
				if (LOGFILE_ERROR) {
					sprintf(localLineTemp,"(EE) Can't create thread at CPU:%d, Node:%d, Core:%d: %s",(i*cpu_status[1])+j,i,j,strerror(pthreadReturnValue));
					log_to(localLineTemp);
				}
				return (void*)-1;
			}
		}
	}
	/* let the threads do their job */
	for (i=0;i<cpu_status[0];i++) { /* Foreach node */
		for (j=0;j<cpu_status[1];j++) { /* Foreach core */
			if ((pthreadReturnValue = pthread_join(thread[(i*cpu_status[1])+j], NULL)) != 0) { /* On success, returns 0; on error, it returns an error number. */ /* https://linux.die.net/man/3/pthread_join */
				if (LOGFILE_ERROR) {
					sprintf(localLineTemp,"(EE) Pthread join failed at CPU:%d, Node:%d, Core:%d: %s",(i*cpu_status[1])+j,i,j,strerror(pthreadReturnValue));
					log_to(localLineTemp);
				}
				return (void*)-1;
			}
		}
	}
	/* Free resources */
	if (LOGFILE_INFORMATION) {log_to("(II) Work done, Freeing resources");}
	if ((pthreadReturnValue = pthread_mutex_destroy(&SHARED_MEMORY.globalMtx)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_destroy */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Pthread mutex (globalMtx) destroy failed: %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	if ((pthreadReturnValue = pthread_cond_destroy(&SHARED_MEMORY.globalCond)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_destroy */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Pthread condition (globalCond) destroy failed: %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	if (commandsClose() == -1) { /* Close mmap, On success, zero is returned. On error, -1 */
		if (LOGFILE_ERROR) {log_to("(EE) Program terminated after mmap closing error.");}
		return (void*)-1;
	}
	assert(SHARED_MEMORY.activeCores == 0); /* https://linux.die.net/man/3/assert */
	destroySharedLog();
	for (i=0;i<cpu_status[0];i++) { /* Foreach node */
		for (j=0;j<cpu_status[1];j++) { /* Foreach core */
			free((CORE_MEMORY + (i*cpu_status[1])+j)->slot.op);
			free((CORE_MEMORY + (i*cpu_status[1])+j)->slot.args);
		}
	}
	free(NODE_MEMORY);
	free(CORE_MEMORY);
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - init finish");}
	fclose(LOGFILE_FD); /* Close the logfile */
	return (void*)0;
}
/************************************************************/
/************	Private Functions	*********************/
/************	Used For Support	*********************/
/************************************************************/
void* bindToCore(void* arg) { /* Each core have a thread attached to her with that function */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - bindToCore start");}
	/* Private variables */
	int pthreadReturnValue;
	int commandsReadResponse;
	char localLineTemp[LOGFILE_LINE_SIZE+5];
	s_nodeMemory* nodeMemory;
	s_coreMemory* coreMemory;
	s_threadId* thId = (s_threadId*)arg;
	/************************************************************/
	/********** This lock is shared among ALL threads! **********/
	/*********** \  /    \  /    \  /    \  /    \  / ***********/
	/************ \/ LOCK \/ LOCK \/ LOCK \/ LOCK \/ ************/
	/************************************************************/
	if ((pthreadReturnValue = pthread_mutex_lock(&SHARED_MEMORY.globalMtx)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_lock */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Pthread mutex (globalMtx) lock failed: %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	nodeMemory = thId->nodeMemory;
	coreMemory = thId->coreMemory;
	/****	Core level operation:	****/
	if ((coreMemory->myCPUID = sched_getcpu()) == -1) { /* On success returns a nonnegative CPU number. On error, -1 is returned and errno is set to indicate the error. */ /* https://linux.die.net/man/3/sched_getcpu */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Failed to get CPU on which thread (Node:%d Core:%d) is running: %s",coreMemory->myNode,coreMemory->myCore,strerror(errno));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	coreMemory->myThreadID = pthread_self(); /* This function always succeeds, returning the calling thread's ID. */ /* https://linux.die.net/man/3/pthread_self */
	 __sync_add_and_fetch(&SHARED_MEMORY.activeCores, 1); /* Atomic operation */ /* https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html */
	/****	Node level operation:	****/
	if (nodeMemory->combinerInitialized == 0) { /* Attempting to initialize an already initialized mutex results in undefined behavior. */
		if ((pthreadReturnValue = pthread_cond_init(&nodeMemory->combinerCond, NULL)) != 0) { /* init condition */ /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_init */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Error initializing condition (combinerCond): %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		if ((pthreadReturnValue = pthread_mutex_init(&nodeMemory->combinerLock, NULL)) != 0) { /* init mutex */ /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_init */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Pthread mutex (combinerLock) init failed: %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		nodeMemory->combinerInitialized = 1; /* Mark lock as initialized */
	}
	if (nodeMemory->rwInitialized == 0) { /* Attempting to initialize an already initialized mutex results in undefined behavior. */
		if ((pthreadReturnValue = pthread_mutex_init(&nodeMemory->rLock, NULL)) != 0) { /* init mutex */ /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_init */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Pthread mutex (rLock) init failed: %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		if ((pthreadReturnValue = pthread_mutex_init(&nodeMemory->wLock, NULL)) != 0) { /* init mutex */ /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_init */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Pthread mutex (wLock) init failed: %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		nodeMemory->rwInitialized = 1; /* Mark it as initialized */
	}
	coreMemory->gds = initializeDataStructure(); /* gds (GlobalDataStructure) is a struct with pointers to the API public functions */
	if (nodeMemory->replicaInitialized == 0) { /* If the replica status is uninitialized -> initialize it */
		nodeMemory->replica = coreMemory->gds->Create(); /* Create an instance of the data structure and save it as the local replica */
		nodeMemory->replicaInitialized = 1; /* Mark it as initialized */
	}
	/****	Done			****/
	thId->ready = 1;
	if ((pthreadReturnValue = pthread_cond_signal(&SHARED_MEMORY.globalCond)) != 0) { /* Signal main thread that we're ready */ /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_signal */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Pthread condition (globalCond) signal failed: %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	if ((pthreadReturnValue = pthread_mutex_unlock(&SHARED_MEMORY.globalMtx)) != 0) { /* Then unlock when we're done. */ /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_unlock */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Pthread mutex (globalMtx) unlock failed: %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	/************************************************************/
	/************ /\ LOCK /\ LOCK /\ LOCK /\ LOCK /\ ************/
	/*********** /  \    /  \    /  \    /  \    /  \ ***********/
	/************************************************************/
	while (!coreMemory->terminate) {
		if (coreMemory->slot.filled == false) {
			commandsReadResponse = commandsRead(&coreMemory->slot.op,&coreMemory->slot.args); /* Read mmap, If found command return 1, otherwise zero is returned. On error, -1 */
			if (commandsReadResponse == 1) {
				coreMemory->slot.filled = true;
				if (LOGFILE_NOISE) {
					sprintf(localLineTemp,"(II) core %d got op:%s args:%s",coreMemory->myCPUID,coreMemory->slot.op,coreMemory->slot.args);
					log_to(localLineTemp);
				}
				executeConcurrent(nodeMemory,coreMemory,coreMemory->slot.op,coreMemory->slot.args); // TODO - What to do with the return value?
			} else if (commandsReadResponse == -1) {
				if (LOGFILE_ERROR) {log_to("(EE) Thread terminated after command read error.");}
				return (void*)-1;
			} else {
				usleep(100); // TODO - How to determine this number?
			}
		}
	}
	if (LOGFILE_INFORMATION) {
		sprintf(localLineTemp,"Thread %d finish working",coreMemory->myCPUID);
		log_to(localLineTemp);
		sprintf(localLineTemp,"CORE(%p)(%d) = ID:%ld, CPU:%d, Slot(%p):[%d,%s,%s], nextCore:%p, Response:%d, GDS:%p",(void*)coreMemory,coreMemory->myCore,(long)coreMemory->myThreadID,coreMemory->myCPUID,(void*)&(coreMemory->slot),coreMemory->slot.filled,(char*)coreMemory->slot.op,(char*)coreMemory->slot.args,(void*)coreMemory->nextCore,coreMemory->response,(void*)coreMemory->gds);
		log_to(localLineTemp);
		sprintf(localLineTemp,"NODE(%p)(%d) = replica(%p):%d, localTail:%p, firstCore:%p, Locks status: Combiner:%d, RW:%d",(void*)nodeMemory,coreMemory->myNode,nodeMemory->replica,nodeMemory->replicaInitialized,(void*)nodeMemory->localTail,(void*)nodeMemory->firstCore,nodeMemory->combinerInitialized,nodeMemory->rwInitialized);
		log_to(localLineTemp);
	}
	/* Clean up */
	__sync_sub_and_fetch(&SHARED_MEMORY.activeCores, 1);  /* Atomic operation */ /* https://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html */
	while (SHARED_MEMORY.activeCores > 0) { /* Wait till all threads finished */
		if ((pthreadReturnValue = pthread_cond_broadcast(&nodeMemory->combinerCond)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_broadcast */
			if (LOGFILE_ERROR) {
				sprintf(line_temp,"(EE) Pthread condition (combinerCond) broadcast failed: %s",strerror(pthreadReturnValue));
				log_to(line_temp);
			}
			return (void*)-1;
		}
		usleep(50000);
	}
	/************************************************************/
	/********** This lock is shared among ALL threads! **********/
	/*********** \  /    \  /    \  /    \  /    \  / ***********/
	/************ \/ LOCK \/ LOCK \/ LOCK \/ LOCK \/ ************/
	/************************************************************/
	if ((pthreadReturnValue = pthread_mutex_lock(&SHARED_MEMORY.globalMtxDestroy)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_lock */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Pthread mutex (globalMtxDestroy) lock failed: %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	if (nodeMemory->replicaInitialized == 1) { /* If the replica status is initialized -> destroy it */
		coreMemory->gds->Execute(nodeMemory->replica, (char*)"destroy", NULL);
		nodeMemory->replicaInitialized = 2; /* Mark it as destroyed */
	}
	if (nodeMemory->rwInitialized == 0) { /* Attempting to destroy a locked mutex results in undefined behavior. */
		if ((pthreadReturnValue = pthread_mutex_destroy(&nodeMemory->wLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_destroy */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Pthread mutex (wLock) destroy failed: %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		if ((pthreadReturnValue = pthread_mutex_destroy(&nodeMemory->rLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_destroy */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Pthread mutex (rLock) destroy failed: %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		nodeMemory->combinerInitialized = 2; /* Mark lock as destroyed */
	}
	if (nodeMemory->combinerInitialized == 1) { /* Attempting to destroy a locked mutex results in undefined behavior. */
		if ((pthreadReturnValue = pthread_mutex_destroy(&nodeMemory->combinerLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_destroy */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Pthread mutex (combinerLock) destroy failed: %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		if ((pthreadReturnValue = pthread_cond_destroy(&nodeMemory->combinerCond)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_destroy */
			if (LOGFILE_ERROR) {
				sprintf(localLineTemp,"(EE) Pthread condition (combinerCond) destroy failed: %s",strerror(pthreadReturnValue));
				log_to(localLineTemp);
			}
			return (void*)-1;
		}
		nodeMemory->combinerInitialized = 2; /* Mark lock as destroyed */
	}
	free(coreMemory->gds);
	if ((pthreadReturnValue = pthread_mutex_unlock(&SHARED_MEMORY.globalMtxDestroy)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_unlock */
		if (LOGFILE_ERROR) {
			sprintf(localLineTemp,"(EE) Pthread mutex (globalMtxDestroy) unlock failed: %s",strerror(pthreadReturnValue));
			log_to(localLineTemp);
		}
		return (void*)-1;
	}
	/************************************************************/
	/************ /\ LOCK /\ LOCK /\ LOCK /\ LOCK /\ ************/
	/*********** /  \    /  \    /  \    /  \    /  \ ***********/
	/************************************************************/
	if (LOGFILE_FUNCTIONS) {
		sprintf(localLineTemp,"(II) NUMA Black Box - bindToCore finish CPU:%d Node:%d Core:%d",coreMemory->myCPUID,coreMemory->myNode,coreMemory->myCore);
		log_to(localLineTemp);

	}
	pthread_exit(0);
}
s_entries* initSharedLog() { /* Create the shared log */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - initSharedLog start");}
	int i;
	s_entries* firstEntry = NULL;
	s_entries* lastEntry = NULL;
	for (i=0;i<NUM_ENTRIES_IN_LOG;i++) {
		s_entries* newEntry = (s_entries*) malloc(sizeof(s_entries));
		newEntry->index = i;						/* The index of the entry in the shared log */
		newEntry->rounds = false;					/* Bit that indicate when the shared log wrap itself */
		newEntry->filled = false;					/* Reserved bit in the slot used to marks that the slot is filled */
		newEntry->op = (char*) malloc(MAX_OP_SIZE * sizeof(char));	/* The command */
		newEntry->args = (char*) malloc(MAX_ARGS_SIZE * sizeof(char));	/* The command arguments */
		if (lastEntry != NULL) { /* Not the first entry */
			lastEntry->next = newEntry;				/* Pointer to the next row in the log */
		} else { /* The first entry in the log */
			firstEntry = newEntry;
		}
		lastEntry = newEntry;
	}
	lastEntry->next = firstEntry; /* Wrap the log (circular buffer) */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - initSharedLog finish");}
	return firstEntry; /* Return the head of the log */
}
void destroySharedLog() { /* Destroy the shared log and free resources */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - destroySharedLog start");}
	int i;
	s_entries* currEntry = SHARED_MEMORY.sharedLog;
	s_entries* nextEntry = NULL;
	for (i=0;i<NUM_ENTRIES_IN_LOG;i++) {
		nextEntry = currEntry->next;
		free(currEntry->op);
		free(currEntry->args);
		free(currEntry);
		currEntry = nextEntry;
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - destroySharedLog finish");}
}
int commandsOpen() { /* Open mmap, On success, zero is returned. On error, -1 */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - commandsOpen start");}
	char* mmapMap;
	struct stat fileInfo = {0};
	if ((SHARED_MEMORY.mmapFd = open(MMAP_FILENAME,O_RDONLY)) == -1) { /* Open the file */ /* Return the new file descriptor, or -1 if an error occurred (in which case, errno is set appropriately). */ /* https://linux.die.net/man/2/open */
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Opening file (mmapFd) for reading: %s",strerror(errno));
			log_to(line_temp);
		}
		return -1;
	}
	if (fstat(SHARED_MEMORY.mmapFd,&fileInfo) == -1) { /* Get the size of the file */ /* On success, zero is returned. On error, -1 is returned, and errno is set appropriately. */ /* https://linux.die.net/man/2/fstat */
		close(SHARED_MEMORY.mmapFd);
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Unable to determine file (mmapFd) size: %s",strerror(errno));
			log_to(line_temp);
		}
		return -1;
	}
	if (fileInfo.st_size == 0) {
		close(SHARED_MEMORY.mmapFd);
		if (LOGFILE_ERROR) {log_to("(EE) File (mmapFd) is empty, nothing to do.");}
		return -1;
	}
	if ((mmapMap = (char*)mmap(0,fileInfo.st_size,PROT_READ,MAP_SHARED,SHARED_MEMORY.mmapFd,0)) == MAP_FAILED) { /* Create the mmap */ /* On success, mmap() returns a pointer to the mapped area. On error, the value MAP_FAILED (that is, (void *) -1) is returned, and errno is set appropriately. On success, munmap() returns 0, on failure -1, and errno is set (probably to EINVAL). */ /* https://linux.die.net/man/2/mmap */
		close(SHARED_MEMORY.mmapFd);
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) mmapping the file: %s",strerror(errno));
			log_to(line_temp);
		}
		return -1;
	}
	SHARED_MEMORY.mmapPointer = 0;
	SHARED_MEMORY.mmapMap = (char*) malloc(fileInfo.st_size * sizeof(char));
	SHARED_MEMORY.mmapMap = mmapMap;
	SHARED_MEMORY.mmapSize = fileInfo.st_size;
	if (pthread_mutex_init(&SHARED_MEMORY.mmapLock, NULL) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_init */
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Error initializing mutex (mmapLock): %s",strerror(errno));
			log_to(line_temp);
		}
		return -1;
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - commandsOpen finish");}
	return 0;
}
int commandsRead(char** op,void** args) { /* Read mmap, If found command return 1, otherwise zero is returned. On error, -1 */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - commandsRead start");}
	int pthreadReturnValue;
	int indxSeperator = 0;
	int found = 0;
	if ((pthreadReturnValue = pthread_mutex_lock(&SHARED_MEMORY.mmapLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_lock */
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Pthread mutex (mmapLock) lock failed: %s",strerror(pthreadReturnValue));
			log_to(line_temp);
		}
		return -1;
	}
	int indxCommand = SHARED_MEMORY.mmapPointer;
	while (SHARED_MEMORY.mmapMap[SHARED_MEMORY.mmapPointer] != '\0') {
		if (SHARED_MEMORY.mmapMap[SHARED_MEMORY.mmapPointer] == '|') {
			indxSeperator = SHARED_MEMORY.mmapPointer;
		} else if (SHARED_MEMORY.mmapMap[SHARED_MEMORY.mmapPointer] == '\n') {
			strncpy(*op,SHARED_MEMORY.mmapMap+indxCommand,indxSeperator-indxCommand); /* https://linux.die.net/man/3/strncpy */
			strncpy((char*)*args,SHARED_MEMORY.mmapMap+indxSeperator+1,SHARED_MEMORY.mmapPointer-indxSeperator-1); /* https://linux.die.net/man/3/strncpy */
			(*op)[indxSeperator-indxCommand] = '\0';
			((char*)(*args))[SHARED_MEMORY.mmapPointer-indxSeperator-1] = '\0';
			SHARED_MEMORY.mmapPointer++;
			found = 1;
			break;
		} else if ((int)SHARED_MEMORY.mmapMap[SHARED_MEMORY.mmapPointer] == 0x1B) { /* At EOF => raise the termination flag for all the threads */
			s_nodeMemory* loopNextNode = SHARED_MEMORY.firstNode;
			s_coreMemory* loopNextCore = NULL;
			while (loopNextNode != NULL) { /* Foreach node */
				loopNextCore = loopNextNode->firstCore;
				while (loopNextCore != NULL) { /* Foreach core */
					loopNextCore->terminate = true; /* Tell the thread to exit */
					if (LOGFILE_INFORMATION) {
						sprintf(line_temp,"(II) Kill signal sent to CPU:%d Node:%d Core:%d",loopNextCore->myCPUID,loopNextCore->myNode,loopNextCore->myCore);
						log_to(line_temp);
					}
					loopNextCore = loopNextCore->nextCore;
				}
				if ((pthreadReturnValue = pthread_cond_broadcast(&loopNextNode->combinerCond)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_broadcast */
					if (LOGFILE_ERROR) {
						sprintf(line_temp,"(EE) Pthread condition (combinerCond) broadcast failed: %s",strerror(pthreadReturnValue));
						log_to(line_temp);
					}
					return -1;
				}
				loopNextNode = loopNextNode->nextNode;
			}
		}
		SHARED_MEMORY.mmapPointer++;
	}
	if ((pthreadReturnValue = pthread_mutex_unlock(&SHARED_MEMORY.mmapLock)) != 0) { /* Unlock the mutex when done. */ /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_unlock */
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Pthread mutex (mmapLock) unlock failed: %s",strerror(pthreadReturnValue));
			log_to(line_temp);
		}
		return -1;
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - commandsRead finish");}
	return found;
}
int commandsClose() { /* Close mmap, On success, zero is returned. On error, -1 */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - commandsClose start");}
	int pthreadReturnValue;
	if ((pthreadReturnValue = pthread_mutex_destroy(&SHARED_MEMORY.mmapLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_destroy */
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Pthread mutex (mmapLock) destroy failed: %s",strerror(pthreadReturnValue));
			log_to(line_temp);
		}
		return -1;
	}
	if (munmap(SHARED_MEMORY.mmapMap,SHARED_MEMORY.mmapSize) == -1) { /* Upon successful completion, munmap() shall return 0; otherwise, it shall return -1 and set errno to indicate the error. */
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Error un-mmapping the file: %s",strerror(errno));
			log_to(line_temp);
		}
		return -1;
	}
	if (close(SHARED_MEMORY.mmapFd) == -1) { /* Returns zero on success. On error, -1 is returned, and errno is set appropriately. */ /* https://linux.die.net/man/2/close */
		if (LOGFILE_ERROR) {
			sprintf(line_temp,"(EE) Error closing mmap file: %s",strerror(errno));
			log_to(line_temp);
		}
		return -1;
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - commandsClose finish");}
	return 0;
}
void log_to(const char* line) { /* Write to logfile the input line */
	if (gettimeofday(&SHARED_MEMORY.tval_after, NULL) == -1) { /* return 0 for success, or -1 for failure (in which case errno is set appropriately). */ /* https://linux.die.net/man/2/gettimeofday */
		fprintf(LOGFILE_FD, "??????????: Error getting local time: %s\n",strerror(errno));
		fprintf(LOGFILE_FD, "??????????: %s\n",line);
	} else {
		timersub(&SHARED_MEMORY.tval_after, &SHARED_MEMORY.tval_before, &SHARED_MEMORY.tval_result); /* https://linux.die.net/man/3/timersub */
		if (line != NULL) {
			fprintf(LOGFILE_FD, "%ld.%06ld: %s\n",(long int)SHARED_MEMORY.tval_result.tv_sec,(long int)SHARED_MEMORY.tval_result.tv_usec,line);
			//printf("%ld.%06ld: %s\n",(long int)SHARED_MEMORY.tval_result.tv_sec,(long int)SHARED_MEMORY.tval_result.tv_usec,line); /* Uncoment this line and comment the line above to print the log output to the screen */
		}
	}
}
void printLog(int from,int to) { /* Print section of the shared log between two indexes */
	int i;
	s_entries* loopEntry = SHARED_MEMORY.sharedLog;
	for (i=from;i<to;i++) {
		printf("[%p] index:%d rounds:%d filled:%d op[%p]:%s args[%p]:%s\n",loopEntry,loopEntry->index,loopEntry->rounds,loopEntry->filled,loopEntry->op,loopEntry->op,loopEntry->args,(char*)loopEntry->args);
		loopEntry = loopEntry->next;
	}
}
bool compareEntries(s_entries* a,s_entries* b) { /* Compare relation between two entries at the log, return true if a<b and false o.w */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - compareEntries start");}
	if (a->rounds == b->rounds) { /* Both entries are on the same round */
		if (LOGFILE_FUNCTIONS) {
			sprintf(line_temp,"(II) NUMA Black Box - compareEntries return %d < %d",a->index,b->index);
			log_to(line_temp);
		}
		return (a->index < b->index);
	} else if (SHARED_MEMORY.logRounds == a->rounds) { /* 'a' is the newest */
		if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - compareEntries return false");}
		return false;
	} else { /* 'b' is the newest */
		if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - compareEntries return true");}
		return true;
	}
}
/************************************************************/
/************	Private Functions	*********************/
/************	Heart Of The BlackBox	*********************/
/************************************************************/
void updateFromLog(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,s_entries* to) {
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - updateFromLog start");}
	s_entries* loopEntry = nodeMemory->localTail;
	while (compareEntries(loopEntry,to)) { /* Updates the local replica by replaying the entries from localTail to right before the entries it allocated */
		while (loopEntry->filled == false) { /* In doing so the combiner may find empty entries allocated by other threads */
			usleep(50); /* In that case, it waits until the entry is filled (identified by a bit in the entry) */ // TODO How to determine this number?
		}
		coreMemory->gds->Execute(nodeMemory->replica,loopEntry->op,loopEntry->args);
		loopEntry = loopEntry->next;
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - updateFromLog finish");}
}
s_entries* reserveLogEntries(int n) {
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - reserveLogEntries start");}
	int i;
	s_entries* val = SHARED_MEMORY.logTail;
	s_entries* newval = val;
	for (i=0;i<n;i++) { /* Move n entries foreward */
		newval = newval->next;
	}
	/* __atomic_compare_exchange(type *ptr, type *expected, type *desired, bool weak, int success_memorder, int failure_memorder)  */
	while (!__atomic_compare_exchange(&SHARED_MEMORY.logTail, &val, &newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {} /* REPEAT CAS(logTail <= val+n) until SUCCESS */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - reserveLogEntries finish");}
	return val; /* Return the original tail */
}
s_entries* appendToLog(int n,s_coreMemory** batch) {
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - appendToLog start");}
	int i,j;
	const int zero = 0;
	const int one = 1;
	s_entries* start = reserveLogEntries(n);
	s_entries* loopEntry = start;
	s_entries* originalLogMin = NULL;
	s_nodeMemory* loopNextNode = NULL;
	for (i=0;i<n;i++) { /* Writes the buffer entries with the operations and arguments */
		if (loopEntry->index == 0) { /* We completed a new round */
			SHARED_MEMORY.logRounds = !SHARED_MEMORY.logRounds;
		}
		loopEntry->rounds = SHARED_MEMORY.logRounds; /* Each log entry has a bit that alternates when the log wraps around to indicate empty entries. */
		loopEntry->filled = true;
		strcpy(loopEntry->op,batch[i]->slot.op);
		strcpy((char*)loopEntry->args,(char*)batch[i]->slot.args);
		loopEntry = loopEntry->next;
		if (loopEntry->index == SHARED_MEMORY.logMinLowMark) { /* When a thread reaches a low mark in the log, which is max batch entries before logMin */
			while (!__atomic_compare_exchange(&SHARED_MEMORY.logMinUpdating, &zero, &one, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
			originalLogMin = SHARED_MEMORY.logMin; /* Save the pointer to the original log min */
			SHARED_MEMORY.logMin = SHARED_MEMORY.firstNode->localTail;
			loopNextNode = SHARED_MEMORY.firstNode->nextNode; /* Start from node #2 */
			while (loopNextNode != NULL) { /* Foreach node */
				if (compareEntries(loopNextNode->localTail,SHARED_MEMORY.logMin)) {  /* Compare relation between two entries at the log, return true if a<b and false o.w */
					SHARED_MEMORY.logMin = loopNextNode->localTail; /* Updates logMin to the smallest localTail of all nodes */
				}
				loopNextNode = loopNextNode->nextNode; /* Move to the next core */
			}
			if (LOGFILE_INFORMATION) {
				sprintf(line_temp,"(II) Move LogMin from %d to %d",originalLogMin->index,SHARED_MEMORY.logMin->index);
				log_to(line_temp);
			}
			while (originalLogMin->index != SHARED_MEMORY.logMin->index) {
				originalLogMin->rounds = false;		/* Bit that indicate when the shared log wrap itself */
				originalLogMin->filled = false;		/* Reserved bit in the slot used to marks that the slot is filled */
				originalLogMin->op = NULL;		/* The command */
				originalLogMin->args = NULL;		/* The command arguments */
				originalLogMin = originalLogMin->next;
			}
			SHARED_MEMORY.logMinLowMark = SHARED_MEMORY.logMin->index - LOG_MIN_SAFE_ZONE - SHARED_MEMORY.maxBatch;
			if (SHARED_MEMORY.logMinLowMark < 0) {
				SHARED_MEMORY.logMinLowMark += NUM_ENTRIES_IN_LOG;
			}
			while (!__atomic_compare_exchange(&SHARED_MEMORY.logMinUpdating, &one, &zero, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
		} else { /* Meanwhile, other threads wait for logMin to change */
			/* __atomic_compare_exchange(type *ptr, type *expected, type *desired, bool weak, int success_memorder, int failure_memorder)  */
			while (!__atomic_compare_exchange(&SHARED_MEMORY.logMinUpdating, &zero, &zero, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {}
		}
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - appendToLog finish");}
	return start;
}
bool combine(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,char* op,void* args) { /* Each combiner executes the operations of its node. The combiners coordinate to write the log */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - combine start");}
	int i;
	int pthreadReturnValue;
	s_entries* startEntry = NULL;
	s_entries* endEntry = NULL;
	s_entries* savedCompletedTail = NULL;
	s_coreMemory* loopNextCore = NULL;
	coreMemory->response = false;
	strcpy(coreMemory->slot.op,op); /* to execute an operation, a thread posts its operation in a reserved slot ...*/
	strcpy((char*)coreMemory->slot.args,(char*)args); /* ... and tries to become the combiner by acquiring the combiner lock */
	while (1) {
		if (pthread_mutex_trylock(&nodeMemory->combinerLock) == 0) { /* We are the combiner */ /* The function shall return zero if a lock on the mutex object referenced by mutex is acquired. Otherwise, an error number is returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_lock */
			loopNextCore = nodeMemory->firstCore; /* The first core of the node */
			while (loopNextCore != NULL) { /* all threads t on node with t.slot.op != @ */
				if (loopNextCore->slot.filled) { /* The combiner reads the slots of the threads in the node */
					nodeMemory->batch[nodeMemory->batchSize] = loopNextCore;
					loopNextCore->slot.filled = false; /* Marks filled slots by setting a reserved bit in the slot */
					nodeMemory->batchSize++; /* And remembers the number of slots filled */
				}
				loopNextCore = loopNextCore->nextCore; /* Move to the next core */
			}
			startEntry = appendToLog(nodeMemory->batchSize,nodeMemory->batch); /* Allocates space by using a CAS to advance logTail by the batch size */
			pthread_mutex_lock(&nodeMemory->wLock); /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_lock */
			pthread_mutex_lock(&nodeMemory->rLock); /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_lock */
			savedCompletedTail = SHARED_MEMORY.completedTail;
			updateFromLog(nodeMemory,coreMemory,startEntry); /* After a combiner refreshes its local replica */
			endEntry = startEntry;
			for (i=0;i<nodeMemory->batchSize;i++) {
				endEntry = endEntry->next;
			}
			nodeMemory->localTail = endEntry;
			while (( /* it updates completedTail using a CAS to its last batch entry if it is smaller */
				!compareEntries(endEntry,SHARED_MEMORY.completedTail)
			)&&(
				/* __atomic_compare_exchange(type *ptr, type *expected, type *desired, bool weak, int success_memorder, int failure_memorder)  */
				!__atomic_compare_exchange(&SHARED_MEMORY.completedTail, &savedCompletedTail, &endEntry, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
			)) {} /* REPEAT CAS(completedTail <= endEntry) until (SUCCESS or (endEntry<CompletedTail) ) */
			for (i=0;i<nodeMemory->batchSize;i++) { /* the combiner executes the operations in the slots it marked before, conveying the results to each thread that submitted an operation */
				nodeMemory->batch[i]->response = coreMemory->gds->Execute(nodeMemory->replica,nodeMemory->batch[i]->slot.op,nodeMemory->batch[i]->slot.args);
				nodeMemory->batch[i] = NULL;
			}
			nodeMemory->batchSize = 0;
			if (LOGFILE_PERFORMANCE){
				sprintf(line_temp,"(!!) Global activeCores:%d logTail:%d completedTail:%d logMin:%d logMinLowMark:%d. Node[0] localTail:%d, Node[1] localTail:%d",SHARED_MEMORY.activeCores,SHARED_MEMORY.logTail->index,SHARED_MEMORY.completedTail->index,SHARED_MEMORY.logMin->index,SHARED_MEMORY.logMinLowMark,SHARED_MEMORY.firstNode->nextNode->localTail->index,SHARED_MEMORY.firstNode->localTail->index);
				log_to(line_temp);
			}
			if ((pthreadReturnValue = pthread_mutex_unlock(&nodeMemory->rLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_unlock */
				if (LOGFILE_ERROR) {
					sprintf(line_temp,"(EE) Pthread mutex (rLock) unlock failed: %s",strerror(pthreadReturnValue));
					log_to(line_temp);
				}
			}
			if ((pthreadReturnValue = pthread_mutex_unlock(&nodeMemory->wLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_unlock */
				if (LOGFILE_ERROR) {
					sprintf(line_temp,"(EE) Pthread mutex (wLock) unlock failed: %s",strerror(pthreadReturnValue));
					log_to(line_temp);
				}
			}
			if ((pthreadReturnValue = pthread_cond_broadcast(&nodeMemory->combinerCond)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_broadcast */
				if (LOGFILE_ERROR) {
					sprintf(line_temp,"(EE) Pthread condition (combinerCond) broadcast failed: %s",strerror(pthreadReturnValue));
					log_to(line_temp);
				}
			}
			if ((pthreadReturnValue = pthread_mutex_unlock(&nodeMemory->combinerLock)) != 0) { /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_unlock */
				if (LOGFILE_ERROR) {
					sprintf(line_temp,"(EE) Pthread mutex (combinerLock) unlock failed: %s",strerror(pthreadReturnValue));
					log_to(line_temp);
				}
			}
			return coreMemory->response;
		} else { /* Not the combiner */
			if ((pthreadReturnValue = pthread_cond_wait(&nodeMemory->combinerCond, &nodeMemory->combinerLock)) != 0) { /* Upon successful completion, a value of zero shall be returned; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_cond_wait */
				if (LOGFILE_ERROR) {
					sprintf(line_temp,"(EE) Pthread condition (combinerCond) wait failed: %s",strerror(pthreadReturnValue));
					log_to(line_temp);
				}
			}
			if (coreMemory->response != false) {
				return coreMemory->response;
			} else {
				return false;
			}
		}
	}
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - combine finish");}
}
bool readOnly(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,char* op,void* args) { /* Read operations run in parallel and locally, if the replica is fresh. */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - readOnly start");}
	s_entries* readTail = SHARED_MEMORY.completedTail; /* A reader reads completedTail when it starts, storing it in a local variable readTail. */
	while (compareEntries(nodeMemory->localTail,readTail)) { /* Reader might acquire writer lock and update */
		usleep(50); /* If the reader sees that a combiner exists, it just waits until localTail ≥ readTail */ // TODO How to determine this number?
	}
	pthread_mutex_lock(&nodeMemory->wLock); /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_lock */
	bool response = coreMemory->gds->Execute(nodeMemory->replica,op,args); /* otherwise, the reader acquires the readers-writer lock in writer mode and refreshes the replica itself */
	pthread_mutex_unlock(&nodeMemory->wLock); /* If successful, the function shall return zero; otherwise, an error number shall be returned to indicate the error. */ /* https://linux.die.net/man/3/pthread_mutex_unlock */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - readOnly finish");}
	return response;
}
bool executeConcurrent(s_nodeMemory* nodeMemory,s_coreMemory* coreMemory,char* op,void* args) { /* Our technique provides a new method ExecuteConcurrent that can be called concurrently from different threads. */
	if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - executeConcurrent start");}
	if (coreMemory->gds->IsReadOnly(nodeMemory->replica,op)) {
		if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - executeConcurrent call readOnly");}
		return readOnly(nodeMemory,coreMemory,op,args); /* For efficiency, NR handles read operations by reading directly from the local replica. */
	} else {
		if (LOGFILE_FUNCTIONS) {log_to("(II) NUMA Black Box - executeConcurrent call combine");}
		return combine(nodeMemory,coreMemory,op,args);
	}
}
