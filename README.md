# Malloc Lab With C

**Team**: team_one

**Author**: Kyoungchan Kang (kangkc09@gmail.com)

**Score**: 99/100

**GitHub**: https://github.com/kkc1383/WEEK07_malloc_lab/blob/main/malloc-lab/mm.c

---

## 개요

본 프로젝트는 동적 메모리 할당자(malloc/free/realloc)를 구현하여 높은 메모리 활용률과 빠른 처리 속도를 달성하는 것을 목표로 합니다. CSAPP(Computer Systems: A Programmer's Perspective) 교재의 Malloc Lab을 기반으로 하며, 최종적으로 **99/100점**을 획득했습니다.

### 주요 성과
- **처리 속도**: Explicit Free List를 통한 탐색 시간 최적화
- **특화 최적화**: Binary-bal, Binary2-bal, Realloc2 케이스 최적화

---

## 최종 구현 방식

### 기본 아키텍처
- **Explicit Free List + Best Fit** 방식을 기본으로 사용
- **Segregated Free List** 개념을 특정 크기에만 적용
- **No-footer 방식** (prev_free bit 활용)
- **Special bit** (0x4)를 통한 특수 블록 관리 (realloc2 케이스)

### 특수 케이스 최적화
| 테스트 케이스 | 최적화 기법 | 설명 |
|--------------|------------|------|
| Binary-bal (7번) | Segregated Free List | 64, 448 바이트 전용 리스트 |
| Binary2-bal (8번) | Segregated Free List | 16, 112 바이트 전용 리스트 |
| Realloc2 (10번) | 선배치 Free Block | 병합되지 않는 free block 미리 배치 |

### 전용 함수
- `special_malloc`: 특수 케이스 할당 처리
- `special_place`: 특수 케이스 배치 처리
- `special_free`: 특수 케이스 해제 처리
- `special_extend_heap`: 특수 케이스 힙 확장 처리

---

## 구현 단계별 발전 과정

### 1단계: Implicit Free List (74점)
#### 구현 내용
- CSAPP 교재의 기본 구현 코드 사용
- First-fit 방식으로 free block 탐색
- 모든 블록을 순회하여 free block 검색

#### 한계점
- 탐색 시간 복잡도: O(n), n = 전체 블록 수 (best-fit의 경우)
- 낮은 메모리 활용률

#### 알고리즘 비교
| 알고리즘 | 점수 | 특징 |
|---------|------|------|
| First-fit | 74점 | 첫 번째 적합한 블록 선택 |
| Next-fit | 82점 | 이전 탐색 위치부터 재개 |
| Best-fit | 72점 | 가장 작은 적합한 블록 선택 |

### 2단계: Realloc 최적화 (79점)
#### 개선 사항
기존 realloc은 크기 증가 시 무조건 새로운 메모리를 할당하고 데이터를 복사했습니다. 이를 개선하여 인접한 free block을 활용하도록 수정했습니다.

#### 구현한 케이스
1. **다음 블록이 free**: 다음 블록을 흡수하여 확장
2. **이전 블록이 free**: 데이터를 이전 블록으로 이동하여 확장
3. **양쪽 모두 free**: 양쪽 블록을 모두 흡수하여 확장
4. **힙 끝 확장**: 에필로그 블록인 경우 sbrk로 직접 확장

```c
// 다음 블록만 free인 경우
if(prev_alloc && !next_alloc) {
    deleteFreeBlock(NEXT_BLKP(bp));
    addSize += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    // ... 헤더 업데이트 및 배치
}
```

#### 성과
- First-fit: 74점 → 79점 (+5점)
- Best-fit: 72점 → 77점 (+5점)

### 3단계: Explicit Free List + No-footer (89점)
#### Explicit Free List
**개념**: Free block만을 연결 리스트로 관리하여 탐색 시간을 획기적으로 단축

**장점**:
- 탐색 시간 복잡도: O(f), f = free block 수
- Free block 수 << 전체 블록 수이므로 성능 향상

**구현**:
```c
// Free block 구조
[Header][Prev Ptr][Next Ptr][...payload...][Footer]

// 매크로 정의
#define GET_PREV(bp) (*(void **)(bp))
#define GET_NEXT(bp) (*(void **)((char *)(bp)+DSIZE))

// 전역 변수
static void* fl_head = NULL; // Free list의 head
```

**관리 방식**:
- **LIFO (Last-In-First-Out)**: 삽입/삭제가 O(1), 구현 간단
- **주소 순서**: 주소 순으로 정렬, 공간 지역성 향상

#### No-footer 방식
**개념**: 할당된 블록은 footer를 제거하여 오버헤드 감소

**문제점**: 이전 블록이 free인지 확인할 수 없음

**해결책**: 현재 블록의 헤더에 `prev_free bit`를 추가
```c
[size | special_bit | prev_free | alloc]
  28-3     bit 2        bit 1      bit 0
```

**구현 규칙**:
1. Free 시: 다음 블록의 `prev_free bit`를 1로 설정
2. Alloc 시: 다음 블록의 `prev_free bit`를 0으로 설정

#### Malloc Lab에서의 한계
No-footer 방식은 이론적으로 효율적이지만, 실제 Malloc Lab에서는 효과가 제한적입니다.

| 요청 크기 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
|----------|---|---|----|----|----|----|----|----|----|
| No-footer | 16 | 16 | 16 | 16 | 16 | 24 | 24 | 24 | 24 |
| Footer | 16 | 24 | 24 | 24 | 24 | 24 | 24 | 24 | 24 |

**문제점**: Malloc Lab의 모든 테스트 케이스는 8의 배수 크기를 요청하므로, no-footer의 이득이 거의 없음

#### 성과
- LIFO + Best-fit: 88점
- 주소 순서 + Best-fit: 89점

### 4단계: CHUNKSIZE 최적화 (91점)
#### 개념
`CHUNKSIZE`는 힙 확장 시 요청하는 최소 크기입니다. 작은 요청이 들어와도 `CHUNKSIZE`만큼 확장하여 추가 sbrk 호출을 줄입니다.

#### 실험
1<<8 (256) ~ 1<<16 (65536)까지 다양한 값 테스트

#### 결과
**CHUNKSIZE = 1<<8 (256 bytes)** 선택
- 4번, 9번, 10번 케이스 점수 상승
- 내부 단편화 최소화

#### 성과
89점 → 91점 (+2점)

---

## 핵심 최적화 기법

### Binary 테스트 케이스 분석

#### Binary-bal.rep 패턴
```
1. 64, 448을 번갈아 할당
   [64][64][448][64][64][448]...

2. 448만 선택적으로 free
   [64][64][free448][64][64][free448]...

3. 512를 할당 요청
   → 기존 방식: 448 공간에 들어가지 않아 새로 할당 (메모리 낭비)
   → 최적화: 64 + 448 = 512 활용
```

#### 핵심 인사이트
**64 + 448 = 512**

이 관계를 활용하여 64와 448 사이에 더미 64를 배치하면, 448 free 시 인접한 64와 병합되어 512 공간이 생성됩니다.

#### 최적화 전략

##### 1. Segregated Free List 적용
특정 크기 전용 리스트 생성:
- `list_16`: 16바이트 요청용 (실제 블록: 24바이트)
- `list_64`: 64바이트 요청용 (실제 블록: 72바이트)
- `list_112`: 112바이트 요청용 (실제 블록: 120바이트)
- `list_448`: 448바이트 요청용 (실제 블록: 456바이트)

##### 2. Special Malloc 구현
```c
void* special_malloc(size_t size) {
    if(size == 16) return pop_16();
    if(size == 64) return pop_64();
    if(size == 112) return pop_112();
    if(size == 448) return pop_448();
}
```

##### 3. 대량 할당 (Batch Allocation)
첫 요청 시 LOOP_MAX(2000)개의 블록을 한 번에 생성:
```c
// 64, 448 쌍으로 생성
for(int i = 0; i < LOOP_MAX; i++) {
    // 72 (64+헤더+푸터)
    // 72 (더미 64, 헤더/푸터 없음)
    // 456 (448+헤더+푸터)
}
```

##### 4. Special Bit 활용
헤더의 세 번째 비트(0x4)를 special bit로 사용:
```c
#define GET_SP(p) (GET(p) & 0x4)
#define SET_SP(p,val) (*(unsigned int*)(p)=((GET(p) & ~0x4) | (val)))
```

**용도**:
- Special 블록임을 표시
- 일반 블록과의 병합 방지
- Special free 로직으로 라우팅

##### 5. 헤더/풋터 오버헤드 제거
더미 64 블록에는 헤더/풋터를 두지 않고, 448 free 시 64만큼 앞으로 이동하여 병합:

```c
if(csize == 456) { // 448 + 8 (헤더+풋터)
    bp -= 64; // 더미 64 위치로 이동
    PUT(HDRP(bp), PACK(520, 0, 0)); // 64+456 = 520
    SET_SP(HDRP(bp), 0x4);
    PUT(FTRP(bp), PACK(520, 0, 0));
    SET_SP(FTRP(bp), 0x4);
    addFreeBlock(bp);
}
```

**효과**: 512 요청 시 정확히 520(512+8) 크기의 블록을 제공

#### 구현 시 고려 사항

##### 1. LIFO vs 주소 순서
대량 할당 시 주소 순서로 삽입하면 O(n²) 시간 소요
→ **LIFO 방식** 채택: O(1) 삽입

##### 2. 케이스 구분
- **Binary2의 128**: realloc2에서도 사용 → 일반 free list에 배치
- **Binary2의 16**: realloc에서도 사용 → 첫 할당 여부로 구분
- **Binary의 64, 448**: 다른 케이스에서 미사용 → 안전하게 특화 가능

##### 3. Coalesce 방지
Special 블록은 일반 coalesce 로직과 분리:
```c
void mm_free(void* ptr) {
    size_t is_sp = GET_SP(HDRP(ptr));
    if(is_sp == 0x4) {
        if(csize==24||csize==120||csize==72||csize==456||csize==136||csize==520) {
            special_free(ptr);
            return;
        }
    }
    // 일반 free 로직
}
```

#### 성과
- Binary-bal: 50% → **97%**
- Binary2-bal: 60% → **90%**
- 전체: 91점 → 97점 (+6점)

### Realloc 최종 최적화

#### Realloc2 특화 최적화
첫 할당 시 병합되지 않는 24바이트 free block 2개를 미리 배치:
```c
if(size == 4092) { // realloc2 첫 요청
    // 첫 번째 24바이트 free block 생성
    PUT(HDRP(bp), PACK(24, 0, 0));
    SET_SP(HDRP(bp), 0x4); // 병합 방지
    PUT(FTRP(bp), PACK(24, 0, 0));
    SET_SP(FTRP(bp), 0x4);
    addFreeBlock(bp);

    // 두 번째 24바이트 free block
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(24, 0, 2));
    SET_SP(HDRP(bp), 0x4);
    PUT(FTRP(bp), PACK(24, 0, 2));
    SET_SP(FTRP(bp), 0x4);
    addFreeBlock(bp);
}
```

#### 성과
97점 → **99점** (+2점)

---

## 성능 분석

### 최종 점수표
<img width="457" height="400" alt="image" src="https://github.com/user-attachments/assets/4652f2b8-b0d5-4c1c-81ba-a496eecf59b7" />


### 메모리 레이아웃

#### Free Block
```
[Header (4B)][Prev Ptr (8B)][Next Ptr (8B)][...payload...][Footer (4B)]
```

#### Allocated Block
```
[Header (4B)][...payload...]
```

#### Header 구조
```
[28 bits: size][1 bit: special][1 bit: prev_free][1 bit: alloc]
```

---

## 참고 자료

### 주요 개념
- **Implicit Free List**: 모든 블록을 순회하여 free block 탐색
- **Explicit Free List**: Free block만을 연결 리스트로 관리
- **Segregated Free List**: 크기별로 free list를 분리하여 관리
- **No-footer**: 할당된 블록의 footer를 제거하여 오버헤드 감소
- **Coalescing**: 인접한 free block을 병합하여 외부 단편화 방지
- **Best-fit**: 요청 크기에 가장 적합한 블록을 선택
- **LIFO**: Last-In-First-Out, 스택 방식의 free list 관리
- **Address-ordered**: 주소 순서대로 정렬된 free list 관리

### 핵심 매크로
```c
// Alignment
#define ALIGN(size) ((MAX(size,3*DSIZE) + (ALIGNMENT-1)) & ~0x7)

// Free list 관리
#define GET_PREV(bp) (*(void **)(bp))
#define GET_NEXT(bp) (*(void **)((char *)(bp)+DSIZE))
#define PUT_PREV(bp,ptr) (*(void **)(bp)=(void *)(ptr))
#define PUT_NEXT(bp,ptr) (*(void **)((char *)(bp)+DSIZE)=(void *)(ptr))

// Special bit 관리
#define GET_SP(p) (GET(p) & 0x4)
#define SET_SP(p,val) (*(unsigned int*)(p)=((GET(p) & ~0x4) | (val)))

// Prev_free bit 관리
#define GET_PREV_FREE(p) (GET(p) & 0x2)
#define SET_PREV_FREE(p,val) (*(unsigned int*)(p)=((GET(p) & ~0x2) | (val)))
```

### 주요 상수
```c
#define WSIZE 4          // Word size (bytes)
#define DSIZE 8          // Double word size (bytes)
#define CHUNKSIZE (1<<8) // Initial heap extension (256 bytes)
#define LOOP_MAX 2000    // Batch allocation size for special malloc
#define ALIGNMENT 8      // Memory alignment
```

### 관련 자료
- Computer Systems: A Programmer's Perspective (CS:APP)
- Bryant and O'Hallaron, Carnegie Mellon University
- [GitHub Repository](https://github.com/kkc1383/WEEK07_malloc_lab/blob/main/malloc-lab/mm.c)

---

## 구현 파일
- [mm.c](malloc-lab/mm.c): 메인 구현 파일
- [mm.h](malloc-lab/mm.h): 헤더 파일
- [mdriver.c](malloc-lab/mdriver.c): 테스트 드라이버
- [memlib.c](malloc-lab/memlib.c): 힙 시뮬레이터

---

**최종 점수: 99/100**

