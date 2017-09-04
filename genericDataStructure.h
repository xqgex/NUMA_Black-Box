#ifndef GENERICDATASTRUCTURE_HEADER
#define GENERICDATASTRUCTURE_HEADER

#include <stdio.h>	// printf
#include <stdlib.h>	// malloc , free
#include <string.h>	// strncmp , strlen

/* THE API:
 *
 * To work with an arbitrary data structure, our approach expects
 * a sequential implementation of the data structure as a class
 * with three generic methods:
 * 1) The Create method creates an instance of the data structure,
 *    returning its pointer.
 * 2) The Execute method takes a data structure pointer, an
 *    operation, and its arguments; it executes the operation on
 *    the data structure, returning the result.
 * 3) The IsReadOnly method indicates if an operation is read-only;
 *    we use this information for read-only optimizations in NR.
 */
struct GenericDS {
	void* (*Create)();
	bool (*Execute)(void* ptr,char* op,void* args);
	bool (*IsReadOnly)(void* ptr,char* op);
};

GenericDS* initializeDataStructure();

#endif
