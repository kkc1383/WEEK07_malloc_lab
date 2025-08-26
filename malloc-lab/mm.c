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
    "team_one",
    /* First member's full name */
    "Kyoungchan Kang",
    /* First member's email address */
    "kangkc09@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) //나중에 이거 좀 고치면 될듯
#define ALIGNMENT 8

#define MAX(x,y) ((x)>(y)? (x) : (y))

#define PACK(size, alloc, prev_free) ((size)|(alloc)|(prev_free))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int*)(p)=(val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_FREE(p) (GET(p) & 0x2)
#define SET_PREV_FREE(p,val) (*(unsigned int*)(p)= (GET(p) & ~0x2) | (val)) // 여기서 val은 0x2혹은 0x0 이런식으로 받아야 함

#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 현재 헤더가 완성되어야만 제대로 계산됨

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE( (char *)(bp) - WSIZE ))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE( (char *)(bp) - DSIZE ))

#define ALIGN(size) ((MAX(size,3*DSIZE) + (ALIGNMENT-1)) & ~0x7) // 주어진 값을 정렬기준에만 맞게 올림해줌 그래서 함수에서 ALIGN(size+WSIZE)로 호출해야 함



static void* heap_listp=NULL;
static void* fl_head=NULL;
static void* extend_heap(size_t asize);
static void* coalesce(void* bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

int mm_init(void){
    fl_head=NULL;
    if((heap_listp=mem_sbrk(4*WSIZE))==(void *)-1)
        return -1;
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK(DSIZE,1,0));
    PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1,0));
    PUT(heap_listp+(3*WSIZE),PACK(0,1,0));
    heap_listp+=2*WSIZE;

    if(extend_heap(CHUNKSIZE)==NULL)
        return -1;
    return 0;
}

void* mm_malloc(size_t size){
    size_t asize=ALIGN(size+WSIZE); // 요청받은 size에 헤더값을 더하고 정렬기준에 맞춘 값
    char* bp;

    if((bp=find_fit(asize))!=NULL){
        place(bp,asize);
        return bp;
    }

    size_t extend_size=MAX(asize,CHUNKSIZE); // 여기서 나오는 extend_size는 헤더가 포함된 값
    if((bp=extend_heap(extend_size))==NULL) //extend_heap으로 얻은 블록의 bp값
        return NULL;
    place(bp,asize); // 요청하는 값만큼 할당
    //윗 부분이 버그가 많이 생기는 것 같음
    return bp;
}

static void* extend_heap(size_t asize){ //여기서 size는 header까지 다 포함된 align 된 size를 줘야함
    char *bp;
    
    if((long)(bp=mem_sbrk(asize))==-1)
        return NULL;
    
    size_t epil_prev_free=GET_PREV_FREE(HDRP(bp));
    PUT(HDRP(bp), PACK(asize,0,epil_prev_free)); // 추가로 생성된 freeblock의 헤더
    PUT(HDRP(bp), PACK(asize,0,epil_prev_free)); // 추가로 생성된 freeblock의 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1,2));//새로 생성된 에필로그 블록, 추가 생성된 블락은 free니까 prev_free는 1(0x2)로 세팅
    
    return (!epil_prev_free) ? bp : coalesce(bp); // 추가로 생성된 free block 이전이 free가 아니었으면 그냥 보내고, freeblock이었으면 병합하기
}

void mm_free(void * ptr){
    void* bp=ptr;
    size_t csize =GET_SIZE(HDRP(bp));
    size_t prev_free=GET_PREV_FREE(HDRP(bp));
    PUT(HDRP(bp),PACK(csize,0,prev_free)); // free 해줄 블록의 헤더
    PUT(FTRP(bp), PACK(csize,0,prev_free)); // free 해줄 블록의 풋터
    //addfreeblock은 나중에 coalesce가 해줄거임.
    coalesce(bp);
}

static void* coalesce(void *bp){
    size_t prev_alloc=!GET_PREV_FREE(HDRP(bp)); // 이전 블록이 할당되어있으면 1, free면 0
    size_t next_alloc=GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t csize=GET_SIZE(HDRP(bp));

    if(prev_alloc&&next_alloc){
        //양쪽이 다 alloc이면 그냥 리턴
    }
    else if(prev_alloc&&!next_alloc){ // 다음 블록만 병합하는 경우
        csize+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(csize,0,0)); // 다음 블록과 병합한 free블록의 헤더
        PUT(FTRP(bp), PACK(csize,0,0)); // 다음 블록과 병합한 free 블록의 풋터
    }
    else if(!prev_alloc&&next_alloc){ // 이전 블록만 병합하는 경우
        csize+=GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(csize,0,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(csize,0,0)); //prev_free가 0인이유는 이전블록이 free해서 병합했는데 더 이전 블록도 free 일 수가 없음.
        bp=PREV_BLKP(bp);
    }
    else{ // 이전, 다음 블록 모두 병합하는 경우
        csize+=GET_SIZE(HDRP(NEXT_BLKP(bp)))+GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(csize,0,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(csize,0,0));
        bp=PREV_BLKP(bp);
    }
    // TODO
    // 새로 이사왔으니 다음 블록에게 prev_free 세팅 해주어야함
    SET_PREV_FREE(HDRP(NEXT_BLKP(bp)),0x2);
    // free 블록 하나가 생긴거니까 addfreeblock도 해주어야함.

    return bp;
}
static void *find_fit(size_t asize){
    char* bp;
    //일단 first fit;
    for(bp=heap_listp;GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp))&&(asize<=GET_SIZE(HDRP(bp))))
            return bp;
    }

    // for(bp=fl_head;bp!=NULL;bp=NEXT_FL(bp)){
    //     if(asize<=GET_SIZE(HDRP(bp)))
    //         return bp;
    // }
    return NULL;
}

static void place(void *bp, size_t asize){ // 이미 free한 블록에 place하려고 하기 때문에 이전 블록은 무조건 alloc이다.
    size_t csize=GET_SIZE(HDRP(bp));
    //free블록을 사용하니까 deletefreeblock해야함

    if((csize-asize)>=(3*DSIZE)){ // place하고 남는 블락으로 free를 만들 수 있다면
        PUT(HDRP(bp), PACK(asize,1,0));//place할 블록의 헤더를 설정
        //alloc 블록이라 풋터는 필요없음
        bp=NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0,0));//place하고 남은 블락을 free 처리 함(헤더 설정)
        PUT(FTRP(bp),PACK(csize-asize,0,0));//place하고 남은 블락을 free 처리 함(풋터 설정) 
        //이 free처리한 블락에 대해서
        //다음 블락의 prev_free 1로 만들기
        SET_PREV_FREE(NEXT_BLKP(bp),0x2);
        //add freeblock하기
    }
    else{ // place하고 남은 블락으로 free를 만들 수 없다면
        PUT(HDRP(bp), PACK(csize,1,0));
        //alloc 블록이라 풋터는 필요없음
        SET_PREV_FREE(NEXT_BLKP(bp),0x0);//다음 블락의 prev_free 설정
    }
}

void* mm_realloc(void* ptr, size_t size){
    void* bp=ptr;
    size_t asize=ALIGN(size+WSIZE);
    size_t csize=GET_SIZE(HDRP(bp));

    if(asize<=csize)//확장을 할 필요가 없다면
        return bp; //그냥 그대로 반환한다.
    else{ // 확장을 해야 한다면
        //새로 이사갈 곳을 찾는다.
        void* newbp=malloc(size); 
        if(newbp==NULL)
            return NULL;
        memcpy(newbp,bp,csize-WSIZE);
        mm_free(bp);
        return newbp;
    }
}