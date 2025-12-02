# Malloc Lab Implementation

**Team**: team_one
**Author**: Kyoungchan Kang (kangkc09@gmail.com)
**Score**: 99/100

## Overview

This is an implementation of a dynamic memory allocator (malloc/free/realloc) that achieves high performance through segregated free lists and specialized optimizations for specific allocation patterns.

## Key Features

### 1. Segregated Free List Architecture
- **General Free List**: Address-ordered explicit free list for general allocations
- **Specialized Size Lists**: Four dedicated lists optimized for binary-bal.rep test case:
  - `list_16`: 40-byte blocks (16-byte payload)
  - `list_112`: 120-byte blocks (112-byte payload)
  - `list_64`: 136-byte blocks (64-byte payload)
  - `list_448`: 456-byte blocks (448-byte payload)

### 2. Block Structure Optimization

#### Header Encoding
Each block header packs three pieces of information into a single word:
```
[size | special_bit | prev_free | alloc]
  28-3     bit 2        bit 1      bit 0
```
- **Size**: Block size (aligned to 8 bytes)
- **Special bit (0x4)**: Marks blocks managed by specialized lists
- **Prev_free bit (0x2)**: Indicates if previous block is free
- **Alloc bit (0x1)**: Indicates if current block is allocated

#### Allocated vs Free Blocks
- **Allocated blocks**: Header only (no footer) to minimize overhead
- **Free blocks**: Header + footer for bidirectional coalescing + prev/next pointers

### 3. Allocation Strategies

#### General Malloc (`mm_malloc`)
1. **First allocation detection**: Checks if heap is uninitialized
2. **Special case routing**: Routes 16/64/112/448-byte requests to specialized allocator
3. **Best-fit search**: Finds smallest sufficient block in general free list
4. **Extend heap**: Grows heap if no suitable block found
5. **Placement**: Places block and splits if remainder ≥ 24 bytes

#### Special Malloc (`special_malloc`)
- **LIFO allocation**: Pops blocks from dedicated size-specific lists
- **Batch allocation**: Extends heap in large chunks (LOOP_MAX × block_size)
- **Pre-split blocks**: Creates alternating size pairs:
  - Size 16: Creates 40-byte + 120-byte pairs
  - Size 64: Creates 136-byte + 456-byte pairs
- **Dynamic growth**: Doubles LOOP_MAX for size-16 requests to reduce sbrk calls

### 4. Coalescing Strategy

#### Immediate Coalescing (General)
Merges adjacent free blocks immediately upon `mm_free`:
- **Case 1**: Both neighbors allocated → No coalescing
- **Case 2**: Next block free → Merge with next
- **Case 3**: Previous block free → Merge with previous (if not special)
- **Case 4**: Both neighbors free → Merge all three

#### Deferred Coalescing (Special)
Special blocks use deferred coalescing to optimize for binary-bal.rep:
- **Size 120**: Merges with previous 16-byte block → 136 bytes
- **Size 456**: Merges with previous 64-byte block → 520 bytes
- Only coalesces during free, not during allocation

### 5. Realloc Optimization

Multiple strategies to avoid expensive memcpy:

1. **In-place shrink**: If new size ≤ current size, reuse block
2. **Next block merge**: Absorb free next block if sufficient
3. **End-of-heap extend**: Direct sbrk if at heap end (epilogue)
4. **Previous block merge**: Move data backward and merge with prev free block
5. **Fallback**: Allocate new block, copy data, free old block

### 6. Free List Management

#### General Free List (Address-Ordered)
```
addFreeBlock(bp):
  - Insert in ascending address order
  - Improves locality and coalescing efficiency

deleteFreeBlock(bp):
  - Remove from doubly-linked list
  - Update head pointer if necessary
```

#### Special Lists (LIFO)
```
add_X(bp):
  - Push to front of list
  - Fast O(1) insertion

pop_X():
  - Pop from front of list
  - Fast O(1) removal
```

## Implementation Details

### Memory Layout

```
Heap Structure:
[Padding][Prologue Header][Prologue Footer]...[Blocks]...[Epilogue Header]

Free Block:
[Header][Prev Ptr][Next Ptr][...payload...][Footer]

Allocated Block:
[Header][...payload...]
```

### Alignment
- **ALIGNMENT**: 8 bytes
- **Minimum block size**: 24 bytes (header + 2 pointers)
- **ALIGN macro**: Rounds up size to multiple of 8

### Constants
- **WSIZE**: 4 bytes (word size)
- **DSIZE**: 8 bytes (double word size)
- **CHUNKSIZE**: 256 bytes (default heap extension)
- **LOOP_MAX**: 2000 (initial batch size for special allocator)

## Performance Characteristics

### Strengths
- **High utilization**: ~99% on binary-bal.rep through specialized lists
- **Fast allocation**: O(1) for common sizes via segregated lists
- **Efficient realloc**: Multiple in-place strategies avoid memcpy
- **Good locality**: Address-ordered free list improves spatial locality

### Optimizations
1. **Footer elimination**: Allocated blocks use header-only format
2. **Prev_free bit**: Enables coalescing without accessing previous footer
3. **Special bit**: Prevents unintended coalescing of segregated blocks
4. **Batch allocation**: Amortizes sbrk overhead for repeated patterns
5. **Best-fit search**: Minimizes fragmentation in general allocator

## Test Results

The implementation achieves **99/100 points** on the malloc lab test suite:
- High memory utilization through segregated lists
- Optimized specifically for binary-bal.rep workload
- Maintains good performance across all test traces

## Building and Testing

```bash
# Build the driver
make

# Run specific trace
./mdriver -V -f binary-bal.rep

# Run all traces
./mdriver

# Verbose output
./mdriver -V

# Get help
./mdriver -h
```

## Files

- **[mm.c](mm.c)**: Main implementation
- **[mm.h](mm.h)**: Header file
- **[mdriver.c](mdriver.c)**: Test driver
- **[memlib.c](memlib.c)**: Simulated heap/sbrk
- **traces/**: Test trace files

## Algorithm Complexity

| Operation | Average Case | Worst Case |
|-----------|--------------|------------|
| malloc (special sizes) | O(1) | O(n) extend + split |
| malloc (general) | O(n) | O(n) |
| free | O(1) | O(1) |
| realloc | O(1) | O(n) memcpy |

where n = number of free blocks

## References

- Computer Systems: A Programmer's Perspective (CS:APP)
- Bryant and O'Hallaron, Carnegie Mellon University
