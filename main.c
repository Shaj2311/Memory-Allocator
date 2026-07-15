#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//DEBUG
#include <string.h>
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

	char *ptr = malloc(8);
	memset(ptr, 0xFF, 8);
	char *ptr2 = malloc(8);
	memset(ptr2, 0xFF, 8);
	char *ptr3 = malloc(16);
	memset(ptr3, 0xFF, 16);
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
	Block *currBlock = blockListStart;
	while(currBlock)
	{
		//if block not free or too small, skip
		if(!currBlock->isFree || (currBlock->size != bytes && currBlock->size < bytes + sizeof(Block)))
		{
			currBlock = currBlock->next;
			continue;
		}

		//found first block that is big enough (First Fit)

		//if block is exactly the right size, simply allocate it and return
		if(currBlock->size == bytes)
		{
			currBlock->isFree = 0;
			return currBlock->start;
		}

		//if block is larger than required size, split block
		void *currMemStart = (void *)(currBlock + 1);
		Block *remainderBlock = currMemStart + bytes;
		void *remainderMemStart = (void *)(remainderBlock + 1);

		//set metadata of remainder block
		remainderBlock->isFree = 1;
		remainderBlock->size = currBlock->size - bytes;
		remainderBlock->start = remainderMemStart;
		remainderBlock->next = currBlock->next;
		remainderBlock->prev = currBlock;

		//update metadata of current block
		currBlock->isFree = 0;
		currBlock->size = bytes;
		currBlock->start = currMemStart;
		currBlock->next = remainderBlock;

		//update list end if needed
		if(blockListEnd == currBlock)
			blockListEnd = remainderBlock;

		//return current allocated memory
		return currMemStart;
	}

	//Nothing found in free list -> use sbrk to get new chunk

	//If tail block is free, we fetch large chunk and coalesce it with tail block
	if(blockListEnd->isFree)
	{
		//get big chunk of memory
		if(sbrk(SBRK_CUTOFF + sizeof(Block)) == (void *)-1) //here, we only need space for one extra block as we already have a free tail block
		{
			puts("OUT OF MEMORY");
			exit(1);
		}

		//update metadata to merge new chunk with tail block
		blockListEnd->size += (SBRK_CUTOFF + sizeof(Block));
	}
	//If tail block is not free, fetch large chunk and set as new tail block
	else
	{
		void *initialBreak = sbrk(SBRK_CUTOFF + 2 * sizeof(Block)); //here we're saving 2 blocks more than SBRK_CUTOFF in case caller requests something like SBRK_CUTOFF - 1 bytes (so we don't have enough memory to store metadata)
		if(initialBreak == (void *)-1)
		{
			puts("OUT OF MEMORY");
			exit(1);
		}

		Block *newBlock = initialBreak;

		newBlock->start = (void *)(newBlock + 1);
		newBlock->size = SBRK_CUTOFF + sizeof(Block);
		newBlock->isFree = 1;
		newBlock->next = 0;
		newBlock->prev = blockListEnd;

		//update tail block
		blockListEnd = newBlock;
	}

	//Split tail block
	currBlock = blockListEnd;
	Block *remainderBlock = (void *)(currBlock) + sizeof(Block) + bytes;

	//set remainder block metadata
	remainderBlock->isFree = 1;
	remainderBlock->start = remainderBlock + 1;
	remainderBlock->size = currBlock->size - bytes;
	remainderBlock->next = 0;
	remainderBlock->prev = currBlock;

	//update current block metadata
	currBlock->isFree = 0;
	currBlock->size = bytes;
	currBlock->start = currBlock + 1;
	currBlock->next = remainderBlock;

	//update block list end pointer
	blockListEnd = remainderBlock;

	//return current allocation
	return currBlock->start;
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
