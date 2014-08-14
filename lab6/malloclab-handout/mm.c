/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * Segregated Free list,Explicit
 * Boundary-tag coalescing
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SGRG_LIST_NUM 10

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))           
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    

/* Read and write a double word at address p*/
#define GET_DW(p)         (*(unsigned long *)(p))
#define PUT_DW(p, val)    (*(unsigned long *)(p) = (unsigned long)(val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   
#define GET_ALLOC(p) (GET(p) & 0x1)                    

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                     
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 
/* $end mallocmacros */

/*global variables*/
static char *sgrg_free_listp = NULL;
static char *heap_listp = NULL;

/*private function*/
static char *get_root_by(size_t size) 
{
  int base = 32;
  char *current_free_listp = sgrg_free_listp;

  if(size <= 24)
    return current_free_listp;
  current_free_listp += DSIZE;

  while(((int)size - base) > 0) // need to change size to int type why?
    {
      base *= 2;
      current_free_listp += DSIZE;
    }
  return current_free_listp;
}
static void *extend_heap(size_t words)
{
  char *bp,*current_free_list;
  size_t size;

  /*Allocate an even number of words to maintain alignment*/
  size = ALIGN(words)*WSIZE;  //becarreful the size is in BYTE, the words is in WORDS
  if((long)(bp = mem_sbrk(size)) == -1)
    return NULL;

  /*Initialize the new free block header/footer and the epilogue header*/
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
  printf("bp %lx\n",bp);
  /*Initialize the new free block pred succ pointer
   *Get the right root according to the size;
   */
  current_free_list = get_root_by(size);
  //printf("root %lx \n", current_free_list);
  PUT_DW(current_free_list, bp);
  return NULL;
}

/* 
 * mm_init - initialize the malloc package.
 * 1,Need a array of pointer to point to the classed free list:
 *  Policy: By power of two
 *  How big is the array?
 *    {,24} 24 is smallest free block size
 *    {25,32} {33, 64} {64, 128} ..,{1025,2048} {2049, 4096} {4097, inf}
 *
 * 2,Use the Boundry tag coalsce
 *  Biodirection,so need a head tag and foot tag
 *  word size, the last bit is a/f
 *
 * 3,Do I need a Prologue block and Epilogue block
 *  Yes,we need these block to eliminate the conditions
 *  during the coalescing
 *
 * 4,Initial the empty heap with a free block of what size?
 *  One page PAGE_SIZE 4KB (1 << 12)
 *
 * 5,Add pointer to the new free block
 */
int mm_init(void)
{
  /*Create the initial free list and empty heap*/
  int i;
  if((sgrg_free_listp = mem_sbrk(SGRG_LIST_NUM*DSIZE + 4*WSIZE)) == (void *)-1)
    return -1;
  
  for(i = 0; i < SGRG_LIST_NUM; i++)
    PUT_DW(sgrg_free_listp + i*DSIZE, 0);
    
  heap_listp = sgrg_free_listp + SGRG_LIST_NUM*DSIZE;
  //printf("heap %lx\n", heap_listp);
  
  PUT(heap_listp, 0);                          /* Alignment padding */
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
  PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
  PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
  heap_listp += (2*WSIZE);                    
  
  /*Extend the empty heap with a free block of CHUNKSIZE bytes*/
 
  if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
    return -1;
  
  return 0;

}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}



static int mm_check(void)
{
  /*check the initialized empty heap*/
  int i;
  for(i = 0; i < SGRG_LIST_NUM; i++)
    printf("%d %lx %lx\n", i, sgrg_free_listp+i*DSIZE, GET_DW(sgrg_free_listp+i*DSIZE));
  char *end_wordp = mem_heap_hi(); // returns the last byte in the heap
  printf("%lx\n", end_wordp - 3);
  char *i_p = NULL;
  i = 0;

  /*print the heap
  for(i_p = heap_listp; i_p <= end_wordp - 3; i_p += 4)
    {
    printf("%lx %x %d\n", heap_listp + i*WSIZE, GET(i_p)
	   ,GET_SIZE(i_p));
    i++;
    }
  
  printf("%x\n",*((unsigned int *)(end_wordp - 3)));
  */
    
  return 0;
}


int main()
{
  
  mem_init();
  mm_init();
  mm_check();
 
  return 0;
}











