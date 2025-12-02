# Malloc Lab 구현

**팀**: team_one
**작성자**: Kyoungchan Kang (kangkc09@gmail.com)
**점수**: 99/100

## 개요

동적 메모리 할당자(malloc/free/realloc)를 구현한 프로젝트입니다. Segregated Free List와 특정 할당 패턴에 대한 최적화를 통해 높은 성능을 달성했습니다.

## 주요 특징

### 1. Segregated Free List 아키텍처

- **일반 Free List**: 주소 순서로 정렬된 명시적 free list (일반 할당용)
- **특수 크기별 List**: binary-bal.rep 테스트 케이스 최적화를 위한 4개의 전용 리스트
  - `list_16`: 40바이트 블록 (16바이트 페이로드)
  - `list_112`: 120바이트 블록 (112바이트 페이로드)
  - `list_64`: 136바이트 블록 (64바이트 페이로드)
  - `list_448`: 456바이트 블록 (448바이트 페이로드)

### 2. 블록 구조 최적화

#### 헤더 인코딩
각 블록 헤더는 3가지 정보를 하나의 워드에 압축:
```
[size | special_bit | prev_free | alloc]
  28-3     bit 2        bit 1      bit 0
```
- **Size**: 블록 크기 (8바이트 정렬)
- **Special bit (0x4)**: 특수 리스트로 관리되는 블록 표시
- **Prev_free bit (0x2)**: 이전 블록이 free인지 표시
- **Alloc bit (0x1)**: 현재 블록의 할당 여부

#### 할당/해제 블록 구조
- **할당된 블록**: 헤더만 사용 (footer 없음) → 오버헤드 최소화
- **Free 블록**: 헤더 + footer + prev/next 포인터 (양방향 병합을 위해)

### 3. 할당 전략

#### 일반 Malloc (`mm_malloc`)
1. **첫 할당 감지**: 힙이 초기화되지 않았는지 확인
2. **특수 케이스 라우팅**: 16/64/112/448바이트 요청을 특수 할당자로 전달
3. **Best-fit 검색**: 일반 free list에서 가장 작은 적합 블록 찾기
4. **힙 확장**: 적합한 블록이 없으면 힙 확장
5. **배치**: 블록 배치 후 나머지가 24바이트 이상이면 분할

#### 특수 Malloc (`special_malloc`)
- **LIFO 할당**: 크기별 전용 리스트에서 블록을 pop
- **배치 할당**: 큰 청크 단위로 힙 확장 (LOOP_MAX × block_size)
- **사전 분할 블록**: 교대로 크기 쌍을 생성
  - 크기 16: 40바이트 + 120바이트 쌍 생성
  - 크기 64: 136바이트 + 456바이트 쌍 생성
- **동적 증가**: 크기-16 요청에 대해 LOOP_MAX를 2배로 증가시켜 sbrk 호출 감소

### 4. 병합 전략

#### 즉시 병합 (일반)
`mm_free` 시점에 인접한 free 블록을 즉시 병합:
- **케이스 1**: 양쪽 모두 할당됨 → 병합 없음
- **케이스 2**: 다음 블록만 free → 다음 블록과 병합
- **케이스 3**: 이전 블록만 free → 이전 블록과 병합 (특수 블록이 아닌 경우)
- **케이스 4**: 양쪽 모두 free → 3개 블록 모두 병합

#### 지연 병합 (특수)
특수 블록은 binary-bal.rep 최적화를 위해 지연 병합 사용:
- **크기 120**: 이전 16바이트 블록과 병합 → 136바이트
- **크기 456**: 이전 64바이트 블록과 병합 → 520바이트
- 할당 시점이 아닌 해제 시점에만 병합

### 5. Realloc 최적화

비싼 memcpy를 피하기 위한 다양한 전략:

1. **제자리 축소**: 새 크기 ≤ 현재 크기인 경우 블록 재사용
2. **다음 블록 병합**: 다음 free 블록이 충분하면 흡수
3. **힙 끝 확장**: 힙 끝(에필로그)에 있으면 직접 sbrk
4. **이전 블록 병합**: 데이터를 뒤로 이동하고 이전 free 블록과 병합
5. **폴백**: 새 블록 할당, 데이터 복사, 이전 블록 해제

### 6. Free List 관리

#### 일반 Free List (주소 순서)
```
addFreeBlock(bp):
  - 오름차순 주소 순서로 삽입
  - 지역성 및 병합 효율성 향상

deleteFreeBlock(bp):
  - 이중 연결 리스트에서 제거
  - 필요시 head 포인터 업데이트
```

#### 특수 리스트 (LIFO)
```
add_X(bp):
  - 리스트 앞에 push
  - 빠른 O(1) 삽입

pop_X():
  - 리스트 앞에서 pop
  - 빠른 O(1) 제거
```

## 구현 세부사항

### 메모리 레이아웃

```
힙 구조:
[패딩][프롤로그 헤더][프롤로그 풋터]...[블록들]...[에필로그 헤더]

Free 블록:
[헤더][Prev 포인터][Next 포인터][...페이로드...][풋터]

할당된 블록:
[헤더][...페이로드...]
```

### 정렬
- **ALIGNMENT**: 8바이트
- **최소 블록 크기**: 24바이트 (헤더 + 포인터 2개)
- **ALIGN 매크로**: 크기를 8의 배수로 올림

### 상수
- **WSIZE**: 4바이트 (워드 크기)
- **DSIZE**: 8바이트 (더블 워드 크기)
- **CHUNKSIZE**: 256바이트 (기본 힙 확장 크기)
- **LOOP_MAX**: 2000 (특수 할당자의 초기 배치 크기)

## 성능 특성

### 강점
- **높은 활용률**: 특수 리스트를 통해 binary-bal.rep에서 ~99% 달성
- **빠른 할당**: 크기별 리스트를 통한 일반 크기의 O(1) 할당
- **효율적인 realloc**: 여러 제자리 전략으로 memcpy 회피
- **좋은 지역성**: 주소 순서 free list로 공간 지역성 향상

### 최적화 기법
1. **Footer 제거**: 할당된 블록은 헤더만 사용
2. **Prev_free 비트**: 이전 footer에 접근하지 않고도 병합 가능
3. **Special 비트**: 특수 블록의 의도하지 않은 병합 방지
4. **배치 할당**: 반복 패턴에 대한 sbrk 오버헤드 분산
5. **Best-fit 검색**: 일반 할당자의 단편화 최소화

## 테스트 결과

malloc lab 테스트 스위트에서 **99/100점** 달성:
- 특수 리스트를 통한 높은 메모리 활용률
- binary-bal.rep 워크로드에 특화된 최적화
- 모든 테스트 trace에서 좋은 성능 유지

## 빌드 및 테스트

```bash
# 드라이버 빌드
make

# 특정 trace 실행
./mdriver -V -f binary-bal.rep

# 모든 trace 실행
./mdriver

# 상세 출력
./mdriver -V

# 도움말
./mdriver -h
```

## 파일 구조

```
malloc_lab_docker/
├── .devcontainer/
│   ├── devcontainer.json      # VSCode 컨테이너 환경 설정
│   └── Dockerfile             # C 개발 환경 이미지 정의
│
├── .vscode/
│   ├── launch.json            # 디버깅 설정 (F5 실행용)
│   └── tasks.json             # 컴파일 자동화 설정
│
├── malloc-lab/
│   ├── mm.c                   # 메인 구현
│   ├── mm.h                   # 헤더 파일
│   ├── mdriver.c              # 테스트 드라이버
│   ├── memlib.c               # 시뮬레이션된 heap/sbrk
│   ├── traces/                # 테스트 trace 파일들
│   ├── Makefile               # 빌드 파일
│   └── README.md              # 과제 설명 (영문)
│
└── README.md                  # 이 파일
```

## 알고리즘 복잡도

| 연산 | 평균 케이스 | 최악 케이스 |
|------|------------|------------|
| malloc (특수 크기) | O(1) | O(n) 확장 + 분할 |
| malloc (일반) | O(n) | O(n) |
| free | O(1) | O(1) |
| realloc | O(1) | O(n) memcpy |

여기서 n = free 블록의 개수

## 핵심 최적화 포인트

### 1. Binary-bal.rep 특화 최적화
- 16, 64, 112, 448바이트 요청을 별도로 관리
- 배치 할당으로 메모리 단편화 최소화
- LIFO 방식으로 캐시 지역성 향상

### 2. Footer 제거를 통한 오버헤드 감소
- 할당된 블록은 헤더만 사용
- Prev_free 비트로 이전 블록의 free 여부 추적
- 메모리 오버헤드를 50% 감소

### 3. 효율적인 Realloc
- 5가지 전략으로 대부분의 경우 memcpy 회피
- 힙 끝에서의 realloc은 직접 확장
- 이전 블록 활용으로 복사 최소화

## 환경 설정 (Docker)

### 요구사항
- Docker Desktop
- VSCode + Dev Containers 확장

### 시작하기

1. **프로젝트 클론**
```bash
git clone --depth=1 https://github.com/krafton-jungle/malloc_lab_docker.git
cd malloc_lab_docker
```

2. **VSCode에서 열기**
```
Ctrl+Shift+P (또는 Cmd+Shift+P)
→ "Dev Containers: Reopen in Container" 선택
```

3. **빌드 및 테스트**
```bash
cd malloc-lab
make
./mdriver -V
```

4. **디버깅**
- 소스코드에 브레이크포인트 설정
- `F5` 키를 눌러 디버깅 시작

## 참고 자료

- Computer Systems: A Programmer's Perspective (CS:APP)
- Bryant and O'Hallaron, Carnegie Mellon University
- [malloc-lab/README.md](malloc-lab/README.md) - 과제 상세 설명 (영문)

## 라이선스

```
#####################################################################
# CS:APP Malloc Lab
# Handout files for students
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################
```
