#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define SBRK_CUTOFF 131072 //128 kB

int PAGE_SIZE = 0;

typedef struct
{
	void *start;

	int isFree;
	size_t size;

	struct Block *prev;
	struct Block *next;
} Block;

void *malloc(size_t bytes);
void free(void *ptr);

int main()
{
	PAGE_SIZE = sysconf(_SC_PAGE_SIZE);

	void *ptr = malloc(200000);
	free(ptr);
}

void *malloc(size_t bytes)
{
	//if large allocation, use mmap
	if(bytes > SBRK_CUTOFF)
	{
		//round up memory size to nearest multiple of page size
		//(round down (bytes + PAGE_SIZE - 1))
		bytes = (bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		//get memory
		void *ptr = mmap(
				0,				//No address specified
				sizeof(Block) + bytes,		//Metadata block + requested bytes
				PROT_READ | PROT_WRITE,		//Read + Write
				MAP_PRIVATE | MAP_ANONYMOUS,	//Private to this process
				-1,					//Anonymous = No file
				0
				);

		//return NULL ptr on failure
		if(ptr == MAP_FAILED)
		{
			return 0;
		}

		//get pointer pointing to start of user's allocated memory
		void *startPtr = ptr + sizeof(Block);

		//fill metadata section
		Block *block = (Block *)ptr;
		block->isFree = 0;
		block->next = 0;
		block->prev = 0;
		block->size = bytes;
		block->start = startPtr;

		return startPtr;

	}

	//TODO: if small allocation, check free list or use sbrk
	return 0;
}
void free(void *ptr)
{
	//get metadata
	void *blockPtr = ptr - sizeof(Block);

	//check size
	int size = ((Block *)blockPtr)->size;

	//If large allocation, use munmap
	if(size > SBRK_CUTOFF)
	{
		if(munmap(blockPtr, sizeof(Block) + size))
		{
			puts("Could not free memory");
			exit(1);
		}
		return;
	}
	//TODO: If small allocation, ...
}
