#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define SBRK_CUTOFF 131072 //128 kB

int PAGE_SIZE = 0;

typedef struct Block
{
	int isFree;
	size_t size;
	struct Block *prev;
	struct Block *next;
} Block;

Block *blockListStart = 0;
Block *blockListEnd = 0;

void *malloc(size_t bytes);
void *calloc(size_t nelem, size_t elsize);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

void dbgPrintHeap();
size_t malloc_usable_size(void *ptr);

#ifdef MALLOC_TEST
int main()
{
	setvbuf(stdout, 0, _IONBF, 0);
	PAGE_SIZE = sysconf(_SC_PAGE_SIZE);

	char *ptr0 = malloc(4);
	dbgPrintHeap();

	char *ptr1 = malloc(8);
	dbgPrintHeap();

	char *ptr2 = realloc(ptr0, 21);
	sprintf(ptr2, "Hello, world!");
	dbgPrintHeap();

	char *ptr3 = calloc(5, sizeof(int));
	for(int i = 0; i < 5; i++)
		ptr3[i] = i + 1;
	dbgPrintHeap();

	free(ptr0);
	dbgPrintHeap();

	free(ptr2);
	dbgPrintHeap();

	free(ptr3);
	dbgPrintHeap();

	free(ptr1);
	dbgPrintHeap();

}
#endif

void *malloc(size_t bytes)
{
	//align request to 16B
	bytes = (bytes + 15) & ~15;

	//if zero size, return
	if(!bytes)
		return 0;

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
		block->size = bytes;
		block->prev = 0;
		block->next = 0;

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
		Block *currBlock = initialBreak;
		Block *remainderBlock = (void *)(currBlock + 1) + bytes;

		//set current block metadata
		currBlock->isFree = 0;
		currBlock->size = bytes;
		currBlock->prev = 0;
		currBlock->next = remainderBlock;

		//set remainder block metadata
		remainderBlock->isFree = 1;
		remainderBlock->size = SBRK_CUTOFF - bytes;
		remainderBlock->prev = currBlock;
		remainderBlock->next = 0;

		//set current block as start of list
		blockListStart = currBlock;
		//set remainder block as end of list
		blockListEnd = remainderBlock;

		//return current block of memory
		return currBlock + 1;
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
			return currBlock + 1;
		}

		//if block is larger than required size, split block
		Block *remainderBlock = (void *)(currBlock + 1) + bytes;

		//set metadata of remainder block
		remainderBlock->isFree = 1;
		remainderBlock->size = currBlock->size - bytes - sizeof(Block);
		remainderBlock->prev = currBlock;
		remainderBlock->next = currBlock->next;

		//update metadata of current block
		currBlock->isFree = 0;
		currBlock->size = bytes;
		currBlock->next = remainderBlock;

		//update list end if needed
		if(blockListEnd == currBlock)
			blockListEnd = remainderBlock;

		//return current allocated memory
		return currBlock + 1;
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

		newBlock->isFree = 1;
		newBlock->size = SBRK_CUTOFF + sizeof(Block);
		newBlock->prev = blockListEnd;
		newBlock->next = 0;

		//update tail block
		blockListEnd->next = newBlock;
		blockListEnd = newBlock;
	}

	//Split tail block
	currBlock = blockListEnd;
	Block *remainderBlock = (void *)(currBlock + 1) + bytes;

	//set remainder block metadata
	remainderBlock->isFree = 1;
	remainderBlock->size = currBlock->size - bytes - sizeof(Block);
	remainderBlock->prev = currBlock;
	remainderBlock->next = 0;

	//update current block metadata
	currBlock->isFree = 0;
	currBlock->size = bytes;
	currBlock->next = remainderBlock;

	//update block list end pointer
	blockListEnd = remainderBlock;

	//return current allocation
	return currBlock + 1;
}

void *calloc(size_t nelem, size_t elsize)
{
	//if zero size, return
	if(!nelem || !elsize)
		return 0;

	//get memory chunk
	void *ptr = malloc(nelem * elsize);
	if(!ptr)
		return 0;

	//wipe to zero (manually)
	char *p = ptr;
	for(size_t i = 0; i < nelem * elsize; i++)
	{
		p[i] = 0;
	}

	//return memory
	return ptr;
}

void *realloc(void *ptr, size_t size)
{
	//if null ptr passed, allocate new memory
	if(!ptr)
	{
		ptr = malloc(size);
		return ptr;
	}

	//if new size <= old size, return pointer (may cause internal fragmentation)
	Block *block = ptr - sizeof(Block);
	if(size <= block->size)
		return ptr;

	//if new size is zero, simply free the block
	if(!size)
	{
		free(ptr);
		return 0;
	}

	//if new size greater than old size,
	//create a new block
	void *newPtr = malloc(size);
	//copy data to new block
	memcpy(newPtr, ptr, block->size);
	//deallocate old block
	free(ptr);
	//return new allocated block
	return newPtr;
}

void free(void *ptr)
{
	//return if null pointer
	if(!ptr)
		return;

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

	//If small allocation, free block in blocks list

	//get block metadata
	Block *currBlock = ptr - sizeof(Block);

	//block found, mark as free
	currBlock->isFree = 1;

	//get neighbors
	Block *nextBlock = currBlock->next;
	Block *prevBlock = currBlock->prev;

	//check if next block is free, coalesce
	if(nextBlock && nextBlock->isFree)
	{
		//remove next block from list
		currBlock->next = nextBlock->next;
		if(nextBlock->next)
			nextBlock->next->prev = currBlock;

		//update current block's metadata
		currBlock->size += sizeof(Block) + nextBlock->size;

		//if deleted block was tail, update tail pointer
		if(blockListEnd == nextBlock)
			blockListEnd = currBlock;
	}

	//check if previous block is free, coalesce
	if(prevBlock && prevBlock->isFree)
	{
		//remove current block from list
		prevBlock->next = currBlock->next;
		if(currBlock->next)
			currBlock->next->prev = prevBlock;

		//update previous block's metadata
		prevBlock->size += sizeof(Block) + currBlock->size;

		//if deleted block was tail, update tail pointer
		if(blockListEnd == currBlock)
			blockListEnd = prevBlock;

		//mark large block as current
		currBlock = prevBlock;
	}


	//if new block is now tail, reduce its size if exceeds SBRK_CUTOFF
	if(blockListEnd == currBlock)
	{
		size_t reduction = blockListEnd->size - SBRK_CUTOFF;

		if(sbrk(-reduction) == (void *)-1)
		{
			puts("Could not free memory");
			exit(1);
		}

		currBlock->size -= reduction;
	}
}

#ifdef MALLOC_TEST
void dbgPrintHeap()
{
	Block *currBlock = blockListStart;
	while(currBlock)
	{
		printf("\033[%dm", currBlock->isFree ? 32 : 31);
		printf("%ld\033[0m |", currBlock->size);

		currBlock = currBlock->next;
	}
	puts("");
}
#endif

size_t malloc_usable_size(void *ptr)
{
	if(!ptr)
		return 0;
	Block *block = ptr - sizeof(Block);
	return block->size;
}
