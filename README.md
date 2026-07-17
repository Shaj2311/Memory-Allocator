# Memory-Allocator
My (imperfect) implementation of malloc

## Description
This is my implementation of dynamic memory management calls like malloc, calloc, realloc and free.
<br>
This implementation is not perfect and does not support large and complex applications. But it does demonstrate the use of core tools and concepts such as byte allignment, chunk splitting and coalescing, and maintaining a free list.

## Features
- Using mmap to allocate larger sized requests and sbrk and free list management to allocate small requests (large vs small requests are categorized by a cutoff value define as SBRK_CUTOFF)
- Aligning all allocation requests to 16 bytes to ensure fast and safe allocation, retrieval and management
- Allocating small requests using First Fit to avoid computational overhead
- Splitting large blocks in the free list into smaller ones when a small allocation is requested, to reduce internal fragmentation
- Coalescing adjacent free blocks into a single large block of memory to prevent false fragmentation
- Applying heap contraction to return memory back to the operating system whenever there is an unreasonably large block of unused memory at the end of the heap

## Testing
The following commands have been used to test this implementation. These execute normally using this malloc implementation:
* ls -l
* find / -type f
* vim -u NONE

## Try It Out
Clone this repository
```
git clone https://github.com/Shaj2311/Memory-Allocator
cd ./Memory-Allocator
```
Test this implementation by making calls in its main function and calling dbgPrintHeap() to visualize the memory blocks list at every step. Then, compile and run like so:
```
gcc -DMALLOC_TEST main.c -o main
./main
```
Force other programs to use this implementation by compiling as a .so library and running the program like so:
```
gcc -fPIC -shared main.c -o libmymalloc.so -O2
LD_PRELOAD=./mylibmalloc.so [PROGRAM HERE]
```
