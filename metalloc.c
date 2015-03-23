//defines
#define BLOCKSIZE 20480		//Total space for memory block in bytes.
#define SMALL_SPACE 5120	//Amount of bytes dedicated for small allocations
#define MAX_SMALL 512		//Max size of small allocations
//includes
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

//Structs

//The header for each allocated chunk of memory.
typedef struct MemEntry_t{
	struct MemEntry_t *succ;	//pointer to next block header
	struct MemEntry_t *prev;	//pointer to previous block header
	unsigned int isFree :1;		//1 bit to signal if block is free or not. 1 = true, 0 = false.
	unsigned int size;			//size of allocated block.
} MemEntry;


//Globals
static char block[BLOCKSIZE];
static MemEntry * smallRoot;
static MemEntry * bigRoot;
static int initialized = 0;

//Main code
//Leak detection
 void leekHarvest(void){	//Cuz it finds leaks.
    
   long leekSize = 0;
   
   MemEntry * ptr = smallRoot;
   while(ptr != NULL){
      if(!(ptr->isFree))
	leekSize += ptr->size;
      ptr = ptr->succ;
    }
   
    ptr = bigRoot;
    while(ptr != NULL){
      if(!(ptr->isFree))
	leekSize += ptr->size;
      ptr = ptr->succ;
    }
    
    fprintf(stderr,"There are %ld bytes of unfreed memory.\n",leekSize);
    if(leekSize == 0)
      fprintf(stderr,"GOOD ON YOU!\n");
    else
      fprintf(stderr,"President Lincoln frowns upon you. FROWNS!!![|>:C\n");
    
    return;
}

//Malloc code
void* metalloc(unsigned int size, char * file, int line){

	MemEntry * p;
	MemEntry * succ;
	
	//Initializes block of memory to use.
	if (! initialized){
		smallRoot = (MemEntry *)block;					//Ready small root header.
		smallRoot->prev = NULL;	smallRoot->succ = NULL;//Point predecessor/successor of small root header to NULL.
		smallRoot->size = SMALL_SPACE;					//Tell small root header it has size of SMALL_SPACE to work with.
		smallRoot->isFree = 1;							//Tell small root header it is free..
		
		bigRoot = (MemEntry *)(block+SMALL_SPACE + sizeof(MemEntry));		//Ready big root header.
		bigRoot->prev = NULL;	bigRoot->succ = NULL;	//Point predecessor/successor of big root header to NULL.
		bigRoot->size = BLOCKSIZE - SMALL_SPACE - 2 * sizeof(MemEntry);	//Tell big root header it has size of \
									(block - area dedicated to small entries - size of big root itself) \
											to work with.
		bigRoot->isFree = 1;							//Tell big root header it is free..
		
		//atexit(leekHarvest);
		initialized = 1;								//Initialized block of memory.
	}
	
	if (size <= MAX_SMALL)		//IF size is of small allocations...
		p = smallRoot;			//...start at small root.
	else						//ELSE size is of big allocation...
		p = bigRoot;			//...start at big root.
	
	//Loop to find a suitable chunk of block to allocate.
	do {
		if (p->isFree == 0)				//current chunk not free. move to next.
			p = p->succ;
		else if (p->size < size)		//current free chunk is too small to use. move to next.
			p = p->succ;
		else if (p->size <= (size + sizeof(MemEntry))){//current free chunk too small to fit allocated chunk + a new header.
			p->isFree = 0;
			return (char *)p + sizeof(MemEntry);		//return whatever we can.
		}
		else{	//Suitable free chunk found. Prep it.
			succ = (MemEntry *)((char*)p + sizeof(MemEntry) + size);//Ready the new succeeding header
			succ->prev = p;						//Point previous of new succeeding header to current header
			succ->succ = p->succ;			//Point next of new succeeding header to old succeeding header (may be NULL).
			if(p->succ != NULL)							//IF there is an old succeeding header...
				p->succ->prev = succ;					// ...have its previous point to new succeeding header.
			p->succ = succ;						//Point successor of current pointer to new succeeding header.
			succ->size = p->size - sizeof(MemEntry) - size;		//Tell new succeeding header it has rest of memory block to use.
			succ->isFree = 1;							//Tell new succeeding header it is a free chunk
			p->size = size;									//Tell current header how big its chunk is.
			p->isFree=0;									//Tell current header it is not free.
			return (char*)p + sizeof(MemEntry);		//returns the allocated chunk, but not the header of the allocated chunk.
		}
	}while ( p != NULL);
	fprintf(stderr, "No space available to allocate, returning null pointer.\n\
		Error occurred at line %d of file %s.\n",__LINE__,__FILE__);
	return NULL;				//No suitable chunk of memory block to allocate. Return null.
}

void freealloc(void * p, char * file, int line){

	if(initialized == 0){
		fprintf(stderr, "Memory block uninitialized. Nothing to free.\
			Error occurred at line %d of file %s.\n", __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
		return;
	}

	MemEntry * ptr = smallRoot;				//Search in the small set.
	MemEntry * pred, * succ;
	
	while(ptr != NULL){						//Until we hit null pointer...
		if (((char *)ptr+sizeof(MemEntry)) < (char *)p) { //...If not at the right header...
			ptr = ptr->succ;				//... ... move to next header
		} else if (((char *)ptr+sizeof(MemEntry)) > (char *)p) {			//... else ...
			fprintf(stderr, "Space at %x is somewhere on the inside of the block that was originally allocated, or the allocated memory is being freed twice.\n\
				Use the pointer that was originally returned by malloc when the block was first allocated.\n\
				Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
			fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
			return;
		} else {
			break;							//... ... Found the right header. break loop.
		}
	}
	
	if(ptr == NULL){						//If we haven't found the right header...
		ptr = bigRoot;						//... Look in the big set
		while(ptr != NULL){					//... ... Until we hit null...
			if (((char *)ptr+sizeof(MemEntry)) < (char *)p) { //...If not at the right header...
				ptr = ptr->succ;				//... ... move to next header
			} else if (((char *)ptr+sizeof(MemEntry)) > (char *)p) {			//... else ...
				fprintf(stderr, "Space at %x is somewhere on the inside of the block that was originally allocated, or the allocated memory is being freed twice.\n	\
					Use the pointer originally returned by malloc when the block was first allocated.\n	\
					Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
				fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
				return;
			} else {
				break;							//... ... Found the right header. break loop.
			}
		}
	}
	
	if(ptr == NULL){						//Pointer not found in memory block.
		fprintf(stderr, "%x is not an allocated memory chunk in the memory block.\n	\
			Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
		return;
	}
	
	if(ptr->isFree){									//If memory at p is already free...
		fprintf(stderr, "Space at %x is already free.\n	\
			Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
		return;											//Return.
	}
	pred = ptr->prev;									//Get predecessor of allocated chunk header.
	if(pred != NULL && pred->isFree == 1){				//IF predecessor header exists and has free chunk...
		pred->size += sizeof(MemEntry) + ptr->size; 	//...merge current header + chunk w/ predecessor (current header still exists)
		pred->succ == ptr->succ;						//...Point successor of predecessor to current successor
		if (ptr->succ != NULL )							//...IF current successor exists...
			ptr->succ->prev = pred;					//... ... point predecessor of current successor to predecessor
	}else{												//ELSE predecessor header does NOT exist...
		ptr->isFree = 1;								//... tell current header it is free.
		pred = ptr;		//!!!CURRENT HEADER IS NOW REFERRED TO AS PREDECESSOR, OTHERWISE IS ALREADY MERGED WITH PREDECESSOR!!!
					//!!!CURRENT HEADER IS NOW REFERRED TO AS PREDECESSOR, OTHERWISE IS ALREADY MERGED WITH PREDECESSOR!!!
					//!!!CURRENT HEADER IS NOW REFERRED TO AS PREDECESSOR, OTHERWISE IS ALREADY MERGED WITH PREDECESSOR!!!
	}
	succ = ptr->succ;									//Get successor of allocated chunk header
	if(succ != NULL && succ->isFree == 1){				//IF successor exists and is free...
		pred->size += sizeof(MemEntry) + succ->size; 	//... merge successor + its chunk w/ predecessor
		pred->succ = succ->succ;					//... point successor of predecessor to successor of successor
		if (succ->succ != NULL)							//... IF successor of successor exists...
			succ->succ->prev = pred;					//... ... set its predecessor to predecessor.
	}
	return;		//End of function.
	
}

void* metallurgilloc(int quantity, unsigned int size, char * file, int line){
	char * ptr = (char*)metalloc(quantity * size, __FILE__, __LINE__);		//obtains allocated block.
	if(ptr == NULL){
		fprintf(stderr, "Error occurred in call to calloc at line %d of file %s.\n", line, file);
		return NULL;
	}
	memset(ptr,0,size);						//memset to 0 out allocated block.
	return ptr;								//returns the 0'd out allocated block.	
}

void *alchemalloc(void * p, int size, char * file, int line) {
	MemEntry * ptr = smallRoot;				//Search in the small set.
	while(ptr != NULL){						//Until we hit null pointer...
		if (((char *)ptr+sizeof(MemEntry)) < (char *)p) { //...If not at the right header...
			ptr = ptr->succ;				//... ... move to next header
		} else if (((char *)ptr+sizeof(MemEntry)) > (char *)p) {			//... else ...
			fprintf(stderr, "Space at %x is somewhere on the inside of the block that was originally allocated.\n\
				Use the pointer that was originally returned by malloc when the block was first allocated.\n\
				Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
			fprintf(stderr, "Error occured in call to realloc at line %d of file %s.\n", line, file);
			return;
		} else {
			break;							//... ... Found the right header. break loop.
		}
	}
	
	if(ptr == NULL){						//If we haven't found the right header...
		ptr = bigRoot;						//... Look in the big set
		while(ptr != NULL){					//... ... Until we hit null...
			if (((char *)ptr+sizeof(MemEntry)) < (char *)p) { //...If not at the right header...
				ptr = ptr->succ;				//... ... move to next header
			} else if (((char *)ptr+sizeof(MemEntry)) > (char *)p) {			//... else ...
				fprintf(stderr, "Space at %x is somewhere on the inside of the block that was originally allocated.\n	\
					Use the pointer originally returned by malloc when the block was first allocated.\n	\
					Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
				fprintf(stderr, "Error occured in call to realloc at line %d of file %s.\n", line, file);
				return;
			} else {
				break;							//... ... Found the right header. break loop.
			}
		}
	}
	
	if(ptr == NULL){						//Pointer not found in memory block.
		fprintf(stderr, "%x is not an allocated memory chunk in the memory block.\n	\
			Error occurred at line %d of file %s.\n", p, __LINE__, __FILE__);
		fprintf(stderr, "Error occured in call to free at line %d of file %s.\n", line, file);
		return NULL;
	}

	freealloc(p, __FILE__, __LINE__);
	char * new = (char*)metalloc(size, __FILE__, __LINE__);		//obtains allocated block.
	if(new == NULL){
		fprintf(stderr, "Error occurred in call to realloc at line %d of file %s.\n", line, file);
		return NULL;
	}
	char * old = (char *)p;
	int i = 0;
	while(i < size) {
		new[i] = old[i];
		i++;
	}
}
