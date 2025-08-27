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
#define CHUNKSIZE (1<<8) //나중에 이거 좀 고치면 될듯
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

#define GET_PREV(bp) (*(void **)(bp))
#define GET_NEXT(bp) (*(void **)((char *)(bp)+DSIZE))
#define PUT_PREV(bp,ptr) (*(void **)(bp)=(void *)(ptr))
#define PUT_NEXT(bp, ptr) (*(void **)((char *)(bp)+DSIZE)=(void *)(ptr))



static void* heap_listp=NULL;
static void* fl_head=NULL;
static void* extend_heap(size_t asize);
static void* coalesce(void* bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void addFreeBlock(void* bp);
static void deleteFreeBlock(void* bp);

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
    // printf("extend %lu \n",asize);
    size_t epil_prev_free=GET_PREV_FREE(HDRP(bp));
    PUT(HDRP(bp), PACK(asize,0,epil_prev_free)); // 추가로 생성된 freeblock의 헤더
    PUT(FTRP(bp), PACK(asize,0,epil_prev_free)); // 추가로 생성된 freeblock의 풋터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1,2));//새로 생성된 에필로그 블록, 추가 생성된 블락은 free니까 prev_free는 1(0x2)로 세팅
    
    return coalesce(bp); // 추가로 생성된 free block 이전이 free가 아니었으면 그냥 보내고, freeblock이었으면 병합하기
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

static void* coalesce(void *bp){ //막 free된 블록이 입력, 합병하고 난 블록이 리턴
    size_t prev_alloc=!GET_PREV_FREE(HDRP(bp)); // 이전 블록이 할당되어있으면 1, free면 0
    size_t next_alloc=GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t csize=GET_SIZE(HDRP(bp));

    if(prev_alloc&&next_alloc){
        //양쪽이 다 alloc이면 그냥 리턴
    }
    else if(prev_alloc&&!next_alloc){ // 다음 블록만 병합하는 경우
        deleteFreeBlock(NEXT_BLKP(bp));
        csize+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(csize,0,0)); // 다음 블록과 병합한 free블록의 헤더
        PUT(FTRP(bp), PACK(csize,0,0)); // 다음 블록과 병합한 free 블록의 풋터
    }
    else if(!prev_alloc&&next_alloc){ // 이전 블록만 병합하는 경우
        deleteFreeBlock(PREV_BLKP(bp));
        csize+=GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(csize,0,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(csize,0,0)); //prev_free가 0인이유는 이전블록이 free해서 병합했는데 더 이전 블록도 free 일 수가 없음.
        bp=PREV_BLKP(bp);
    }
    else{ // 이전, 다음 블록 모두 병합하는 경우
        deleteFreeBlock(PREV_BLKP(bp));
        deleteFreeBlock(NEXT_BLKP(bp));
        csize+=GET_SIZE(HDRP(NEXT_BLKP(bp)))+GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(csize,0,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(csize,0,0));
        bp=PREV_BLKP(bp);
    }
    // TODO
    // 새로 이사왔으니 다음 블록에게 prev_free 세팅 해주어야함
    SET_PREV_FREE(HDRP(NEXT_BLKP(bp)),0x2);
    // free 블록 하나가 생긴거니까 addfreeblock도 해주어야함.
    addFreeBlock(bp);

    return bp;
}
static void *find_fit(size_t asize){
    char* bp;
    // //일단 first fit;
    // for(bp=NEXT_BLKP(heap_listp);GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp)){
    //     if(!GET_ALLOC(HDRP(bp))&&(asize<=GET_SIZE(HDRP(bp))))
    //         return bp;
    // }
    size_t minsize=__SIZE_MAX__;
    char* minbp=NULL;
    for(bp=fl_head;bp!=NULL;bp=GET_NEXT(bp)){
        size_t bpsize=GET_SIZE(HDRP(bp));
        if(asize<=bpsize && bpsize<minsize){
            minsize=bpsize;
            minbp=bp;
        }
    }
    return minbp;
}

static void place(void *bp, size_t asize){ // 이미 free한 블록에 place하려고 하기 때문에 이전 블록은 무조건 alloc이다.
    size_t csize=GET_SIZE(HDRP(bp)); //내가 넣으려는 free block
    //free블록을 사용하니까 deletefreeblock해야함
    
    deleteFreeBlock(bp);
    if((csize-asize)>=(3*DSIZE)){ // place하고 남는 블락으로 free를 만들 수 있다면
        // printf("splited : %lu, %lu\n",csize,asize);
        PUT(HDRP(bp), PACK(asize,1,0));//place할 블록의 헤더를 설정
        //alloc 블록이라 풋터는 필요없음
        bp=NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0,0));//place하고 남은 블락을 free 처리 함(헤더 설정)
        PUT(FTRP(bp),PACK(csize-asize,0,0));//place하고 남은 블락을 free 처리 함(풋터 설정) 
        //이 free처리한 블락에 대해서
        //다음 블락의 prev_free 1로 만들기
        SET_PREV_FREE(HDRP(NEXT_BLKP(bp)),0x2);
        //add freeblock하기
        addFreeBlock(bp);
    }
    else{ // place하고 남은 블락으로 free를 만들 수 없다면
        // printf("just put : %lu, %lu\n",csize,asize);
        PUT(HDRP(bp), PACK(csize,1,0));
        //alloc 블록이라 풋터는 필요없음
        SET_PREV_FREE(HDRP(NEXT_BLKP(bp)),0x0);//다음 블락의 prev_free 설정
    }
}

void* mm_realloc(void* ptr, size_t size){
    void* bp=ptr;
    size_t asize=ALIGN(size+WSIZE); // 요청한 바이트의 요구 바이트 실체
    size_t csize=GET_SIZE(HDRP(bp)); // 지금 내 공간의 크기

    if(asize<=csize)//확장을 할 필요가 없다면
        return bp; //그냥 그대로 반환한다.
    else{ // 확장을 해야 한다면
        size_t prev_alloc=!GET_PREV_FREE(HDRP(bp));
        size_t next_alloc=GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        size_t addSize=csize;
        size_t prev_free=GET_PREV_FREE(HDRP(bp)); //이전 alloc 유무를 알아야 place할때 적용하기때문에
        
        if(!prev_alloc && addSize+GET_SIZE(HDRP(PREV_BLKP(bp)))>=asize){ // 이전,다음블록이 모두 free이고, 이전 다음 블록을 합치면 추가블록을 충당할 수 있다면
            addSize+=GET_SIZE(HDRP(PREV_BLKP(bp)));
            char* prev_bp=PREV_BLKP(bp); // memmove하면 해당 위치에 덮어씌워지니까 미리 prev_header의 주소를 저장
            deleteFreeBlock(prev_bp); // 이전블록을 쓸거니까 free list에서 삭제해주고 
            memmove(prev_bp,bp,csize-WSIZE); // 데이터를 옮긴다.
            if(addSize-asize>=3*DSIZE){
                PUT(HDRP(prev_bp),PACK(asize,1,0));
                //alloc 블록이라 푸터필요없음
                void* next_bp=NEXT_BLKP(prev_bp);
                PUT(HDRP(next_bp),PACK(addSize-asize,0,0));
                PUT(FTRP(next_bp),PACK(addSize-asize,0,0));
                addFreeBlock(next_bp);
                SET_PREV_FREE(HDRP(next_bp),0x2);
            }
            else{
                PUT(HDRP(prev_bp),PACK(addSize,1,0));
                //alloc이라 푸터없음
                SET_PREV_FREE(HDRP(NEXT_BLKP(bp)),0x0);
            }
            return prev_bp;
        }
        if(!next_alloc&& addSize+GET_SIZE(HDRP(NEXT_BLKP(bp)))>=asize){ // 바로 다음 블록을 쓸 수 있다면
            addSize+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
            deleteFreeBlock(NEXT_BLKP(bp)); // 다음 free블록을 쓰게 되었으니 삭제해줘야함
            if(addSize-asize>=3*DSIZE){ // 분할을 할 수 있다면,
                PUT(HDRP(bp), PACK(asize,1,prev_free));
                //alloc이라 풋터없음
                void* next_bp=NEXT_BLKP(bp); // 분할해서 생긴 free block의 bp자리
                PUT(HDRP(next_bp),PACK(addSize-asize,0,0)); //분할해서생긴 freeblock 헤더
                PUT(FTRP(next_bp),PACK(addSize-asize,0,0)); // 분할해서생긴 freeblock 풋터
                addFreeBlock(NEXT_BLKP(bp));//분할해서 생긴 freeblock list에 넣기
                SET_PREV_FREE(HDRP(next_bp),0x2); // 분할해서 생긴 free block 다음은 prev_free가 2일테니
            }
            else{ // 분할 못한다면 그냥 넣기
                PUT(HDRP(bp), PACK(addSize,1,prev_free));
                //alloc이라 풋터 없음
                SET_PREV_FREE(HDRP(NEXT_BLKP(bp)),0x0); //free 블록 뺏어서 할당했으니 0으로 해줘야함
            }
            return bp;
        }
        if(!next_alloc&& !prev_alloc && addSize+GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(HDRP(NEXT_BLKP(bp)))>=asize){ // 이전,다음블록이 모두 free이고, 이전 다음 블록을 합치면 추가블록을 충당할 수 있다면
            addSize+=GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(HDRP(NEXT_BLKP(bp)));
            char* prev_bp=PREV_BLKP(bp); // memmove하면 해당 위치에 덮어씌워지니까 미리 prev_header의 주소를 저장
            deleteFreeBlock(prev_bp); // 이전블록을 쓸거니까 free list에서 삭제해주고 
            deleteFreeBlock(NEXT_BLKP(bp));
            memmove(prev_bp,bp,csize-WSIZE); // 데이터를 옮긴다.
            if(addSize-asize>=3*DSIZE){
                PUT(HDRP(prev_bp),PACK(asize,1,0));
                //alloc 블록이라 푸터필요없음
                void* next_bp=NEXT_BLKP(prev_bp);
                PUT(HDRP(next_bp),PACK(addSize-asize,0,0));
                PUT(FTRP(next_bp),PACK(addSize-asize,0,0));
                addFreeBlock(next_bp);
                SET_PREV_FREE(HDRP(next_bp),0x2);
            }
            else{
                PUT(HDRP(prev_bp),PACK(addSize,1,0x0));
                //alloc이라 푸터없음
                SET_PREV_FREE(HDRP(NEXT_BLKP(bp)),0);
            }
            return prev_bp;
        }
        // 신경써야할건 다음 블록이 free이기 때문에, free list에서 빼주는것
        // 그리고 prev_free에 관한 내용인데,
        //분할한 이후, 다음 블록은 0으로 만들어주어야함.
        //새로 이사갈 곳을 찾는다.
        void* newbp=mm_malloc(size); 
        if(newbp==NULL)
            return NULL;
        memcpy(newbp,bp,csize-WSIZE);
        mm_free(bp);
        return newbp;
    }
}

static void addFreeBlock(void* bp){
    if(fl_head==NULL){ // free list에 아무것도 없다면
        PUT_PREV(bp,NULL); // 시작 노드의 이전도 없고
        PUT_NEXT(bp,NULL); // 다음도 없음
        fl_head=bp; // newptr을 헤드로 임명
    }
    else{ //자기 주소를 찾아가야함.
        if(bp<=fl_head){ // 주소 맨 앞에 와야할 경우
            PUT_PREV(bp,NULL); // 맨 앞에 올 노드니까 이전은 없고
            PUT_NEXT(bp,fl_head); // 다음 노드가 원래 헤드였던 노드
            PUT_PREV(fl_head,bp); // 그리고 원래 헤드였던 노드의 이전이 추가한 블락이고
            fl_head=bp;// 추가한 블락을 헤드로 임명
        }
        else{
            void* iter;
            void* prev;
            for(iter=fl_head;iter!=NULL;iter=GET_NEXT(iter)){
                prev=iter;
                if(GET_NEXT(iter)!=NULL && bp<GET_NEXT(iter)){ //나보다 큰 주소를 만났으면, 그게 iter의 next, iter과 iternext 사이에 들어오면 됨
                    PUT_PREV(bp,iter);
                    PUT_NEXT(bp,GET_NEXT(iter));
                    PUT_PREV(GET_NEXT(iter),bp);
                    PUT_NEXT(iter,bp);
                    return;
                }
            }
            //맨 끝에 와야하는 경우
            PUT_PREV(bp,prev);
            PUT_NEXT(bp,NULL);
            PUT_NEXT(prev,bp);
            return;
        }
    }
}

static void deleteFreeBlock(void* bp){
    if(!fl_head||!bp) return; // 삭제할건데 아무 노드도 없으면 그냥 리턴
    if(fl_head==bp){ // 헤드 노드를 삭제 할 경우
        fl_head=GET_NEXT(bp);// 헤드 다음의 노드가 헤드가 될테니
        if(!fl_head) return; // 리스트가 하나밖에 없는 경우
        else PUT_PREV(fl_head,NULL); // 새로운 헤드의 이전은 없어야 하니 널로 설정
    }
    else{
        PUT_NEXT(GET_PREV(bp),GET_NEXT(bp)); // 이전노드의 다음이 삭제할 노드의 다음
        if(GET_NEXT(bp)) PUT_PREV(GET_NEXT(bp),GET_PREV(bp)); // 다음 노드의 이전이 삭제할 노드의 이전
    }
}