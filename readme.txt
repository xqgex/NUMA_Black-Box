Black-box Concurrent Data Structures for NUMA Architectures

Based on: http://cs.brown.edu/~irina/papers/asplos2017-final.pdf
By: Irina Calciu, Siddhartha Sen, Mahesh Balakrishnan, Marcos K. Aguilera

             Node #1                              Node #2             
╔═══════╦═══════╦═══════╦═══════╗    ╔═══════╦═══════╦═══════╦═══════╗
║ Core  ║ Core  ║ Core  ║ Core  ┣━━━━┫ Core  ║ Core  ║ Core  ║ Core  ║
╟───────╫───────╫───────╫───────╊━━━━╉───────╫───────╫───────╫───────╢
║ Cache ║ Cache ║ Cache ║ Cache ┣━━━━┫ Cache ║ Cache ║ Cache ║ Cache ║
╟───────╨───────╨───────╨───────╢    ╟───────╨───────╨───────╨───────╢
║             Cache             ║    ║             Cache             ║
╚═════════════┳━┳━┳═════════════╝    ╚═════════════┳━┳━┳═════════════╝
              ┃ ┃ ┃                                ┃ ┃ ┃              
┌─────────────┺━┻━┹─────────────┐    ┌─────────────┺━┻━┹─────────────┐
│            Memory             │    │            Memory             │
└───────────────────────────────┘    └───────────────────────────────┘

/************************************************************/
/************	My implementation	*********************/
/************************************************************/
This is the general scheme of my implementation:
                                              ┌────────────────────────┐
                                              │      libgenericDS      │
                                              ├────────────────────────┤
                                              │ genericDataStructure.h │
                                              │ genericDataStructure.c │
                                              └────────────────────────┘
                                                          ▲             
                                                          ║             
                               Shared Memory              ▼             
┌──────────────────────┐    ┌──────────────────┐    ┌────────────┐      
│ libgenerateCommands  │═══▶│ gds_commands.bin │═══▶│ libNUMAbb  │      
├──────────────────────┤    └──────────────────┘    ├────────────┤      
│  generateCommands.c  │                            │  NUMAbb.h  │      
│                      │                            │  NUMAbb.c  │      
└──────────────────────┘                            └────────────┘      
            ▲                                             ▲             
            ║                  ┌────────────┐             ║             
            ╚══════════════════│ myprogram  │═════════════╝             
                               ├────────────┤                           
                               │   main.c   │                           
                               └────────────┘                           
The code was designed to work on unix system and was written in C.
In order to run the program there is only one file that need to be run,
But the files are still acting like a separate libraries.
There is more work that need to be done, Feel free to fork my project.

Run the program:
	Build and run the program by running the command: "sh makefile.sh"

Limitation from the current implementation:
	1) The mmap size is a limitaion on the amount of commands.
	2) Variable MAX_CORES limit the program to work with up-to 512 cores.
	3) Maximum 'op' and 'args' size is 1024 chars each.

Problems:
	Unfortunately, My code isn't perfect yet, 

More upgrades that can be done:
	Section 5.2
		To avoid small inefficient batches, the combiner in NR waits
		if the batch size is smaller than a parameter MIN BATCH. Rather
		than idle waiting, the combiner refreshes the local replica from
		the log, though it might need to refresh again after finally
		adding the batch to the log
	Section 5.5 - Better readers-writer lock
		The distributed readers-writer lock uses a per-reader
		lock to reduce reader overhead; the writer must acquire the
		locks from all readers. We modify this algorithm to reduce
		writer overhead as well, by adding an additional writer lock.
		To enter the critical section, the writer must acquire the
		writer lock and wait for all the readers locks to be released,
		without acquiring them; to exit, it releases its lock. A reader
		waits if the writer lock is taken, then acquires its local lock,
		and checks the writer lock again; if this lock is taken, the
		reader releases its local lock and restarts; otherwise, it enters
		the critical section; to exit, it releases the local lock. With
		this scheme, the writer and readers incur just one atomic
		write each on distinct cache lines to enter the critical sec-
		tion. Readers may starve if writers keep coming, but this is
		unlikely with NR , as often only one thread wishes to be a
		writer at a time (the combiner) and that thread has signif-
		icant work outside the critical section.
	Section 5.7
		Padded the data and aligned the cache to avoid false sharing.
	General
		What prevent two combiners (on separate nodes) to write the log at the same time?
		I think there is need to create something that will prevent it.

Build errors:
	In case of compilation errors, Make sure you have all the dependency packages on your computer,
	Use "apt-get install" / "yum install" / "dnf install" To install: (On Ubunto devel => dev)
		1) libnuma-devel	Development files for libnuma
		2) numactl		Library for tuning for Non Uniform Memory Access machines
		3) numactl-libs		libnuma libraries
		4) numactl-devel	Development package for building Applications that use numa
		5) glibc-devel		Object files for development using standard C libraries.
		6) glibc-static		C library static libraries for -static linking.
		7) libstdc++-devel	Header files and libraries for C++ development
		8) libstdc++-static	Static libraries for the GNU standard C++ library

