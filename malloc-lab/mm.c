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

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 워드하나의 크기를 정렬에 맞춘것 그냥 8byte

#define WSIZE 4 // 워드사이즈 4byte
#define DSIZE 8 // 더블 워드사이즈 8byte
#define CHUNKSIZE (1 << 12) // 한 청크 4KB

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8 //정렬 기준: 8byte = 2*word

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + DSIZE + (ALIGNMENT - 1)) & ~0x7) //8byte 기준으로 정렬, 올림, (ALIGNMENT-1)을 더해주고 나누어서 0이 아니라면 무조건 몫이 1이 오름 bias 개념

#define MAX(x,y) ((x) > (y) ? (x) : (y)) // 둘 중에 큰 값

#define PACK(size, alloc) ((size) | (alloc)) // 헤더 만드는 매크로, size와 alloc (아마 필요에 따라서 바꿔야할 수도 있음)

#define GET(p)  (*((unsigned int *)(p))) // 해당 포인터에서 uint(4byte)만큼 긁어오기
#define PUT(p, val) (*((unsigned int *)(p)) = (val)) // val이라는 값을 해당 포인터에 넣기

#define GET_SIZE(p)  (GET(p) & ~0x7) // p라는 포인터가 가리키는 블록(헤더)의 size 긁기 (뒷 비트 3자리 제외)
#define GET_ALLOC(p) (GET(p) & 0x1) // p라는 포인터가 가리키는 헤더의 alloc 유무 (뒷 비트 1자리)

#define HDRP(bp) ((char *)(bp) - WSIZE) // bp는 payload시작지점, HDRP는 헤더 앞을 바라보게함
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 풋터 앞을 바라보게 함. payload에서 워드사이즈만큼 빼면 헤더고 전체 크기만큼 더하면 맨 끝이고, 워드만큼 또 빼면 풋터시작 지점
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 지금 블록의 사이즈만큼 더하면 다음블록의 payload 위치가 나옴
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전 블록의 풋터로가서 사이즈를 구한뒤 지금 위치에서 이전 블록만큼 빼면 이전블록의 payload 위치가 나옴

static char* heap_listp=NULL; // 프롤로그의 payload 위치
static char* last_bp=NULL;
static void* extend_heap(size_t words); // 추가 힙 메모리를 할당받는 함수
static void* coalesce(char* bp); // 병합하는 함수
static void* find_fit(size_t asize); // 알맞는 블록을 찾는 함수(key point)
static void place(char* bp, size_t asize); //알맞는 블록을 찾았으면 그 자리에 데이터를 넣는 함수

/*
 * 알아두면 좋을 조합
 GET_SIZE(HDRP(bp))
 GET_ALLOC(HDRP(bp))
  p 계열의 인자를 받는 매크로들은 HDRP,FTRP,NEXT_BLKP,PREV_BLKP를 같이 써야 함
 */


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if((heap_listp=mem_sbrk(4*WSIZE))== (void *)-1) // sbrk 함수를 통해 첫 4byte(패딩,프롤로그,에필로그)를 할당받기, 안되면 리턴
        return -1;
    heap_listp=(char *)heap_listp;
    PUT(heap_listp,0); // 우선 heap_listp가 우리가 이제부터 사용할 힙메모리의 시작지점이므로 맨 앞 1워드 패딩을 먼저 넣음
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // 1워드 패딩 다음에 프롤로그 헤더를 넣음
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // 2워드 패딩 다음에 프롤로그 풋터를 넣음
    PUT(heap_listp + (3*WSIZE), PACK(0,1)); // 3워드 패딩 다음에 에필로그 헤더를 넣음
    heap_listp+=(2*WSIZE); // heap_listp는 원래 프롤로그 헤더 뒤에 있어야 하므로 이동

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL) // 초기세팅이 완료되었으니 1청크만큼 블록을 추가할당받음(이제부터 쓸거니까 미리 받아놓음) 안되면 NULL 리턴
        return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if(!size) return NULL; //일단 size가 0을 할당받으려한다면 NULL 리턴

    size_t asize = ALIGN(size); // 헤더,풋터 합친거 그거랑 원하는 사이즈를 더하고, align에 맞춤
    char *bp; // payload 시작 위치를 가리킴
    if ((bp=find_fit(asize))!=NULL){ // asize만큼 할당받을 블록의 위치를 받음, 만약 없다면 pass 아래로 감
        place(bp,asize); // 그 위치에 bp를 박음
        return bp;
    }

    size_t extendsize=MAX(asize,CHUNKSIZE); // 추가 메모리를 할당받아야 한다면, 적어도 CHUNKSIZE만큼 받고 싶기에 둘중 큰 값을 설정함
    if((bp=extend_heap(extendsize/WSIZE))==NULL) // extend_heap 함수를 호출해서 추가하고 싶은 word크기만큼 넣고, 만약 NULL이 반환되면 NULL(더이상 메모리 할당 안됨)
        return NULL;
    place(bp,asize); // 추가 메모리를 할당받았다면, 추가 메모리 시작지점에 place
    return bp; // 할당이 된 메모리의 시작부분을 반환
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr)); // 메모리 반환을 원하는 지점(payload 시작부분)의 헤더로부터 사이즈 얻기
    
    PUT(HDRP(ptr), PACK(size,0)); // 헤더에 free임을 알리기위해서 alloc_bit를 0으로 새로 갱신
    PUT(FTRP(ptr), PACK(size,0)); // 풋터에 free임을 알리기위해서 alloc_bit를 0으로 새로 갱신
    last_bp=coalesce(ptr); //free를 하였으니 앞뒤 블록을 보고 병합(즉시 연결 방식) //여기서 free이후에 병합한 결과를 last_bp로 만들면 해당 지점이 일단 확실히 free이기도하고 이상한 곳을 가리키지 않게됨

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;   // 이전 포인터
    void *newptr;         // 새로 메모리 할당할 포인터

    size_t originsize = GET_SIZE(HDRP(oldptr)); // 원본 사이즈
    size_t newsize    = ALIGN(size);           // 새 사이즈+정렬필요
    size_t copySize = originsize-DSIZE;

    // size 가 더 작은 경우
    if (newsize <= originsize) {
        return oldptr;
    } else {
        size_t addSize = originsize + GET_SIZE(HDRP(NEXT_BLKP(oldptr))); // 추가 사이즈 -> 헤더 포함 사이즈
        if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && (newsize <= addSize)) { // 가용 블록이고 사이즈 충분
            //합친 블록을 free블록으로 설정해놓고 place 돌림
            PUT(HDRP(oldptr), PACK(addSize, 0)); // 병합한 블록의 헤더를 free로 함
            PUT(FTRP(oldptr), PACK(addSize, 0)); // 병합한 블록의 풋터를 free로 함
            place(oldptr,newsize);
            return oldptr;
        } else {
            newptr = mm_malloc(newsize);
            if (newptr == NULL)
                return NULL;
            memmove(newptr, oldptr, copySize); // memcpy 사용 시, memcpy-param-overlap 발생
            mm_free(oldptr);
            return newptr;
        }
    }
    // if(!ptr) return mm_malloc(size);
    // if(!size) { mm_free(ptr); return NULL;}

    // char* oldptr=ptr;
    // char* newptr=NULL;
    // size_t oldSize=GET_SIZE(HDRP(ptr));
    // size_t csize; // 이건 전체 크기 값
    // size_t asize=ALIGN(size); // 요청받은 size를 align에 맞춘 값 + 헤더 풋터 더한 값
    // if(asize<=oldSize){  //size가 줄어들어야 한다면
    //     if(oldSize-asize>=2*DSIZE){//줄어들고 남은 부분이 분할을 할 수 있을 정도라면
    //         PUT(HDRP(oldptr),PACK(oldSize,0)); // 분할하기전 통 크기 블락을 free처리
    //         PUT(FTRP(oldptr),PACK(oldSize,0)); // 분할하기전 통 크기 블락을 free처리
    //         newptr=oldptr; // newptr이 결과값인데 변한게 없으니까 그냥 oldptr받아오기
    //         place(newptr,asize); // 분할하기 전 통 크기 블락에 원하는 만큼만 넣고 나머지는 free
    //         return newptr;
    //     }
    //     else{ // 분할할수 없다면 그냥 반환
    //         return ptr;
    //     }
    // }
    // else{ //늘어나야 한다면
    //     if(!GET_ALLOC(HDRP(PREV_BLKP(oldptr)))&& asize-oldSize<=GET_SIZE(HDRP(PREV_BLKP(oldptr)))){ // 이전 블록이 free 가능하고, 추가로필요한 블록을 이전블록에서 채워줄수 있을 때
    //         csize=GET_SIZE(HDRP(oldptr))+GET_SIZE(HDRP(PREV_BLKP(oldptr)));
    //         char* prev_bp=PREV_BLKP(oldptr); // memmove하면 해당 위치에 덮어씌워지니까 미리 prev_header의 주소를 저장
    //         memmove(prev_bp,oldptr,oldSize-DSIZE);   
    //         newptr=prev_bp;
    //         PUT(HDRP(newptr),PACK(csize,0));
    //         PUT(FTRP(newptr) ,PACK(csize,0));
    //         place(newptr,asize); 
    //         return newptr;
    //     }
    //     if(!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && asize-oldSize<=GET_SIZE(HDRP(NEXT_BLKP(oldptr)))){ //다음 블록이 free 가능하고, 추가로 필요한 블록을 다음블록과 함께 담아낼 수 있을 때
    //         csize=GET_SIZE(HDRP(oldptr))+GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
    //         newptr=oldptr;
    //         PUT(HDRP(newptr),PACK(csize,0));
    //         PUT(FTRP(newptr) ,PACK(csize,0));
    //         place(newptr,asize);
    //         return newptr;
    //     }
    //     if(!GET_ALLOC(HDRP(NEXT_BLKP(oldptr)))&& !GET_ALLOC(HDRP(PREV_BLKP(oldptr)))&& asize-oldSize<=GET_SIZE(HDRP(NEXT_BLKP(oldptr)))+GET_SIZE(HDRP(PREV_BLKP(oldptr)))){ // 이전,다음블록이 모두 free이고, 이전 다음 블록을 합치면 추가블록을 충당할 수 있다면
    //         csize=GET_SIZE(HDRP(oldptr))+GET_SIZE(HDRP(PREV_BLKP(oldptr)))+GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
    //         char* prev_bp=PREV_BLKP(oldptr); // memmove하면 해당 위치에 덮어씌워지니까 미리 prev_header의 주소를 저장
    //         memmove(prev_bp,oldptr,oldSize-DSIZE);
    //         newptr=prev_bp;
    //         PUT(HDRP(newptr),PACK(csize,0));
    //         PUT(FTRP(newptr) ,PACK(csize,0));
    //         place(newptr,asize);
    //         return newptr;
    //     }
    //     newptr = mm_malloc(size); // 이사갈 블록을 새로 할당받습니다.
    //     if (newptr == NULL) // NULL 받으면 못 받은거라 리턴
    //         return NULL;        
    //     memmove(newptr, oldptr, oldSize-DSIZE); //oldptr에서 copysize만큼의 값을 newptr로 복사
    //     mm_free(oldptr); //oldptr에 해당하는 블록을 free
    //     return newptr;
    // }
}

static void* extend_heap(size_t words){
    char *bp; //추가 생성된 힙 메모리 시작부분의 payload의 시작 주소
    size_t size; // 입력받은 word를 2의 배수로 align한 값의 총 바이트 수

    size=(words%2) ? (words+1) * WSIZE : words * WSIZE; // word가 짝수면 그냥 *4byte 홀수면 1더하고 *4byte
    if((long)(bp=mem_sbrk(size))==-1) //필요한 size만큼 sbrk 함수에게 추가 메모리 할당 요청, 그러면 payload시작 주소를 줌, bp가 포인터이므로 size가 같고 정수비교를 위해 long으로 캐스팅 함
        return NULL;
    // 여기서 중요한점 여기서 bp는 힙의 맨 끝이됨 다르게 말하면 원래 에필로그 블록의 끝 주소에 해당함
    // 그래서 그다음 줄의 HDRP(bp)를 통해서 에필로그 블록의 시작부분으로 가게 되는 것이고
    // 그래서 마지막에 에필로그 블록 한칸만큼의 공간이 남아서 그곳에 새로운 에필로그 블록을 생성하는 것임 
    // 그리고 새로 완료하게 되면 new freeblock의 payload부분이 bp가 되는거임

    PUT(HDRP(bp), PACK(size,0)); //추가 메모리 할당받은 곳은 free니까 헤더에 size와 alloc_bit=0을 새김
    PUT(FTRP(bp), PACK(size,0));//추가 메모리 할당받은 곳은 free니까 풋터에 size와 alloc_bit=0을 새김
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // 새로운 에필로그 블록을 생성
    
    return coalesce(bp); //추가로 할당받은 블록을 기준으로 병합, 오른쪽은 당연히 안되겠지만 왼쪽 확인해서 병합
}

static void* coalesce(char* bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //왼쪽블록의 푸터로부터 가져온 alloc_bit 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //오른쪽블록의 헤더로부터 가져온 alloc_bit
    size_t size = GET_SIZE(HDRP(bp)); //지금 내 블록의 사이즈

    if(prev_alloc&& next_alloc){ // 왼쪽 오른쪽 둘다 allocated이면 그냥 bp 반환
        return bp;
    }
    else if(prev_alloc && !next_alloc){ // 오른쪽만 free면 오른쪽 free 블록과 병합
        size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));//오른쪽 블록의 사이즈를 더함
        PUT(HDRP(bp), PACK(size, 0)); // 합친 size를 나타내는 헤더를 설정
        PUT(FTRP(bp), PACK(size,0)); // 그 헤더를 기반으로 풋터위치 찾고 풋터 설정
    }
    else if(!prev_alloc && next_alloc){
        size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp=PREV_BLKP(bp);
    }
    else{
        size+=GET_SIZE(HDRP(PREV_BLKP(bp)))+
            GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0)); // 이전 블록의 시작부분에 헤더를 설정
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0)); // 다음 블록의 풋터부분에 풋터 설정
        bp=PREV_BLKP(bp);
    }
    return (void *)bp;
}

static void* find_fit(size_t asize){
    if(!last_bp) last_bp=NEXT_BLKP(heap_listp);
    // last_bp를 아예 전역변수로 둬버렸음. 지역 정적변수는 재귀함수에만 사용
    char* bp;
    for(bp=last_bp;GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp)){ // 최근에 봤던 bp부터 검색 //GET_SIZE(HDRP(bp)) 는 언젠가 에필로그 블록을 만남 거기에서 끝내라는 뜻
        if(!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp)))){ // 일단 free인지 확인, 그다음 원하는 asize를 담을 수 있는지 확인
            last_bp=bp;//last_bp를 bp다음 블락으로 잡을까 생각했는데, 힙의 범위를 넘어설 수도 있는 위험이 있어서 그냥 현재 bp값을 저장하는 것으로 // 라고 생각했는데 효율과 일관성을 위해서 다음 블록을 사용한다고 하네요
            return (void *)bp; // 찾았으면 바로 리턴
        }
    }

    for(bp=NEXT_BLKP(heap_listp);bp!=last_bp;bp=NEXT_BLKP(bp)){ // 최근에 봤던 것부터 봤는데 없으니까 혹시 몰라 처음부터 최근에 봤던 곳 까지만 봄
        if(!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp)))){
            last_bp=bp;
            return (void *)bp; 
        }
    }
    return NULL; // 못찾았으면 null
}

static void place(char* bp, size_t asize){ // 이 함수는 블록을 받아서 값 입력을 했는데, 남는 공간이 있을 수도 있으니까 그거 분할하는 과정도 포함되어 있음.
    size_t csize=GET_SIZE(HDRP(bp)); // 내가 할당받은 블록의 사이즈 (align 되어 있음)

    // 근데 남는 블락이 홀수면 어떡함? 이걸 그대로 냅둬버리면 align이 깨지지 않나? 왜냐면 이거부터 시작할테니까 그런데 애초에 할당받는것도 align이 된 것이고, 내가 넣으려던 값 asize도 align이 되어서 절대로 차이가 홀수가 나올 수가 없음.
    if((csize- asize)>=(2*DSIZE)){ // 남는 크기가 2word 이상이면 남는 부분 free하기
        PUT(HDRP(bp), PACK(asize,1)); // 헤더 설정
        PUT(FTRP(bp), PACK(asize,1)); // 풋터 설정
        bp=NEXT_BLKP(bp);// asize기준 남는 블락의 payload쪽으로 이동
        PUT(HDRP(bp),PACK(csize-asize,0)); // 남는크기만큼 free block의 헤더 설정
        PUT(FTRP(bp),PACK(csize-asize,0)); // 남는 크기만큼 free block의 풋터 설정
        last_bp=coalesce(bp);
    }
    else{ //남는 공간이 없으면 그냥 넣기
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
        last_bp=NEXT_BLKP(bp);//블락 설정할때마다 last_bp 옮기기
    }
}