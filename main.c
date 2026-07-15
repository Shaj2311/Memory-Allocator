#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define SBRK_CUTOFF 131072 //128 kB

int PAGE_SIZE = 0;

typedef struct Block
{
	void *start;

	int isFree;
	size_t size;

	struct Block *prev;
	struct Block *next;
} Block;

Block *blockListStart = 0;
Block *blockListEnd = 0;

void *malloc(size_t bytes);
void free(void *ptr);

int main()
{
	PAGE_SIZE = sysconf(_SC_PAGE_SIZE);

	void *ptr = malloc(6);
	void *ptr2 = malloc(131066);
}

void *malloc(size_t bytes)
{
	//if large allocation, use mmap
	if(bytes >= SBRK_CUTOFF)
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
			return 0;

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

	//if small allocation, check free list or use sbrk

	//empty block list
	if(!blockListStart)
	{
		//get large chunk of heap memory
		void *initialBreak = sbrk(SBRK_CUTOFF + 2 * sizeof(Block)); //here we're saving 2 blocks more than SBRK_CUTOFF in case caller requests something like SBRK_CUTOFF - 1 bytes (so we don't have enough memory to store metadata)
		if(initialBreak == (void *)-1)
		{
			puts("OUT OF MEMORY");
			exit(1);
		}

		//split memory into two blocks:
		//memory to be returned now
		//remaining memory to be kept for later
		void *currMem = initialBreak;
		void *remainderMem = initialBreak + sizeof(Block) + bytes;

		//set current block metadata
		void *currMemStart = currMem + sizeof(Block);
		Block *currBlock = (Block *)currMem;

		currBlock->start = currMemStart;
		currBlock->isFree = 0;
		currBlock->size = bytes;
		currBlock->next = remainderMem;
		currBlock->prev = 0;


		//set remainder block metadata
		void *remainderMemStart = remainderMem + sizeof(Block);
		Block *remainderBlock = (Block *)remainderMem;

		remainderBlock->start = remainderMemStart;
		remainderBlock->isFree = 1;
		remainderBlock->size = SBRK_CUTOFF - bytes;
		remainderBlock->next = 0;
		remainderBlock->prev = currBlock;


		//set current block as start of list
		blockListStart = currBlock;
		//set remainder block as end of list
		blockListEnd = remainderBlock;

		//return current block of memory
		return currMemStart;
	}

	//If list not empty, check blocks list for empty block (first fit)
	Block *searchBlock = blockListStart;
	while(searchBlock)
	{
		//if block not free or too small, skip
		if(!searchBlock->isFree || searchBlock->size < bytes)
		{
			searchBlock = searchBlock->next;
			continue;
		}

		//found first block that is big enough (First Fit)

		//if block is exactly the right size, simply allocate it and return
		if(searchBlock->size == bytes)
		{
			searchBlock->isFree = 0;
			return searchBlock->start;
		}
		//TODO: if block is larger than required size, split
	}

	return 0;
}
void free(void *ptr)
{
	//get metadata
	void *blockPtr = ptr - sizeof(Block);

	//check size
	int size = ((Block *)blockPtr)->size;

	//If large allocation, use munmap
	if(size >= SBRK_CUTOFF)
	{
		if(munmap(blockPtr, sizeof(Block) + size))
		{
			puts("Could not free memory");
			exit(1);
		}
		return;
	}
	//TODO: If small allocation, search blocks list for block, mark as free, coalesce
	//If free block at end of list, move program break back?
}
