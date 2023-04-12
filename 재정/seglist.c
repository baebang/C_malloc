/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
    "sw_jungle_jaejung",
    /* First member's full name */
    "Kimjaejung",
    /* First member's email address */
    "wowjd2524@gmail.com",
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

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size | alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp - WSIZE))
#define FTRP(bp) ((char *)(bp + GET_SIZE(HDRP(bp)) - DSIZE))

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


//explicit을 위한 매크로 (이전값, 이후 값)
#define PRED_FREEP(bp) (*(void**)(bp)) 
#define SUCC_FREEP(bp) (*(void**)(bp+WSIZE))

// 분리 가용 리스트 배열 index 수
#define NUM_LISTS 32 

// heap 의 첫번째 포인터
static char* heap_listp;
// explicit을 위한 free 포인터 설정
static char* free_listp[NUM_LISTS];

// alloction을 위한 함수 선언
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* explicit */
void remove_block(void *bp);
void insert_freeBlock(void *bp);



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    for (int i=0; i < NUM_LISTS; i++){
        free_listp[i] = NULL;
    }
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1) // prev,와 next를 사용하지 않으므로 4사이즈 할당
        return -1; //유효성 검사

    PUT(heap_listp, 0);                              // heap의 첫 패딩 - free(0) 값 넣어준다                                    
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE*2, 1));     // heap의 Prolog 헤더                 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE*2, 1));     // heap의 Prolog 푸터              
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));         // heap의 Epilog  

    heap_listp += (2 * WSIZE);
    
    // extend_heap 을 통해 heap을 요청
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){ 
        return -1;
    }
    return 0;
}


static void *extend_heap(size_t words) 
{ 
    char *bp;
    size_t size;
    
    // double word allignment 짝수 개만큼의 size를 반환한다 (홀수면 짝수로 변환)
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    // 변환한 사이즈만큼 메모리 확보에 실패하면 NULL이라는 주소값을 반환해 실패했음을 알린다
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
   
    return coalesce(bp);
}

/* 
 * find_index - 블럭 사이즈를 기반으로 free_array배열에 들어가게 될 index를 반환하는 함수
 */
static int find_index(size_t asize){
    int index;
    for(index = 0;index<NUM_LISTS;index++){
        if(asize <= (1<< index)){
            return index;
        }
    }
    return index;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // 생성할 size
    size_t asize;
    // chunksize를 넘길 경우									 
	size_t extendsize;													 
	char* bp;


    // 만약 입력받은 사이즈가 0 이면 무시
	if (size <= 0)														 
		return NULL;

    // 만약 입력받은 사이즈가 dsize보다 작아도 최소 size인 16으로 생성
	if (size <= DSIZE)													
		asize = 2 * DSIZE;
	else
        // 8의 배수(Dsize)로 생성
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);		 
        
    /* free list 탐색하기 */
    // 들어갈 free 블록이 있다면 해당 위치에 넣어준다
	if ((bp = find_fit(asize)) != NULL) {								 
		place(bp, asize);
		return bp;
	}

    /* 들어갈 수 있는 fit 존재하지 않을 경우, 추가 메모리를 할당 받고 해당 위치에 넣는다*/
	extendsize = MAX(asize, CHUNKSIZE);
    // 들어갈 사이즈가 힙 리스트를 초과한다면 sbrk를 통해서 NULL을 리턴
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
	place(bp, asize);

	return bp;
}

/*
 * first fit으로 구현
 */
static void *find_fit(size_t asize) 
{
    int index = find_index(asize);
    void *bp;

    for(int i = index; i < NUM_LISTS; i ++) {
        for (bp = free_listp[i]; bp != NULL; bp = SUCC_FREEP(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize)
                return bp;
        }
    }
    return NULL;

}

static void place(void *bp, size_t asize) {
    size_t current_size = GET_SIZE(HDRP(bp));
    
    // 차이가 최소블럭크기 미만의 오차로 딱 맞다면 헤더, 푸터만 갱신해주면 됨
    // 해당 내용은 지워준다.
    remove_block(bp);
    if((current_size - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(current_size-asize, 0));
        PUT(FTRP(bp), PACK(current_size-asize, 0));
        insert_freeBlock(bp);
    }
    else {
        PUT(HDRP(bp), PACK(current_size, 1));
        PUT(FTRP(bp), PACK(current_size, 1));
    }
}


/*
* insert_freeBlock(bp) - 새로운 블록을 넣고 그에따른 pred포인터와 succ의 포인터를 변경해준다.
*/ 
void insert_freeBlock(void *bp)
{
    int index = find_index(GET_SIZE(HDRP(bp)));

    if(free_listp[index] == NULL){
        SUCC_FREEP(bp) = NULL;
        PRED_FREEP(bp) = NULL;
        free_listp[index] = bp;
    }
    else{
        PRED_FREEP(bp) = NULL;
        SUCC_FREEP(bp) = free_listp[index];
        PRED_FREEP(free_listp[index]) = bp;
        free_listp[index] = bp;
    }
}

/*
* remove_block(bp) - 기존의 연결을 끊고 새로 free한 블록과의 연결관계를 형성
*/
void remove_block(void *bp)
{
    int index = find_index(GET_SIZE(HDRP(bp)));

    if(free_listp[index] != bp){
        if(SUCC_FREEP(bp) != NULL){
            PRED_FREEP(SUCC_FREEP(bp)) = PRED_FREEP(bp);
            SUCC_FREEP(PRED_FREEP(bp)) = SUCC_FREEP(bp);
        }
        else{
            SUCC_FREEP(PRED_FREEP(bp)) = NULL;
        }
    }
    else{
        if(SUCC_FREEP(bp) != NULL){
            PRED_FREEP(SUCC_FREEP(bp)) = NULL;
            free_listp[index] = SUCC_FREEP(bp);
        }
        else{
            free_listp[index] = NULL;
        }
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * coalesce
 */
static void *coalesce(void *bp) {
    // 이전 footer로부터 할당 정보를 가져온다 
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    // 다음 header로부터 할당 정보를 가져온다        
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현 사이즈 정보
    size_t size = GET_SIZE(HDRP(bp));
    size_t check = PREV_BLKP(bp);

    //  case1. 위 아래 둘다 할당블록인 경우
    if (prev_alloc && next_alloc) {
        insert_freeBlock(bp);
        return bp;
    }
    //  case2. 위 블록이 가용상태인 경우 0 1
    else if (prev_alloc && !next_alloc) {
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    //  case3. 아래 블록이 가용상태인 경우 1 0
    else if (!prev_alloc && next_alloc) {
        remove_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
		
    }
    //  case4. 위 아래 블록이 가용상태인 경우
    else if(!prev_alloc && !next_alloc) {
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		
    } 
    // 연결되어진 새로운 free 블록을 free 리스트에 추가한다.
    insert_freeBlock(bp);
    return bp;
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void* old_bp = bp;
	void* new_bp;
	size_t copySize;

    // 다른데다가 다시 할당 받기
	new_bp = mm_malloc(size);

    // 실패하면 NULL 리턴
	if (new_bp == NULL) 
		return NULL;

    // 원래 블록의 사이즈
	copySize = GET_SIZE(HDRP(old_bp));			
    // 요청한 사이즈가 작다면 작은사이즈로 카피						  
	if (size < copySize)												  
		copySize = size;
	memcpy(new_bp, old_bp, copySize);
    // 기존 사이즈는 삭제
	mm_free(old_bp);													 
	return new_bp;
}
