# Memory Allocation

## Allocating Memory on the Heap

### Adjusting the Program Break: brk() and sbrk()

- The **heap** is the region used for **dynamic memory allocation**.
- It starts immediately after the uninitialized data segment (`.bss`).
- It grows **upward** (toward higher addresses) as more memory is allocated.
- The kernel tracks the top of the heap via the **program break**.

#### How Allocation Works at the Lowest Level

- Initially, program break is just past the end of `.bss`.
- Increasing the program break → **virtual memory** is allocated (contiguous addresses become valid).
- **Physical memory pages** are allocated **lazily** — only when the process first accesses (reads/writes) an address in a new page (via page fault mechanism).
- Normal programs use `malloc()`/`free()` (described next), but these are built on top of the primitive system calls below.

```c
#include <unistd.h>

int brk(void *end_data_segment);      // Set program break absolutely
void *sbrk(intptr_t increment);       // Adjust program break relatively
```

1. **`brk(end_data_segment)`**
   - Sets the program break to exactly the address specified.
   - Address is **rounded up** to the next page boundary (virtual memory allocated in pages).
   - Returns 0 on success, –1 on error.

2. **`sbrk(increment)`**
   - **Linux**: Library function wrapping `brk()`.
   - Adds `increment` to current break:
     - `increment > 0`: Grow heap (allocate).
     - `increment < 0`: Shrink heap (deallocate – rarely used directly).
     - `increment == 0`: Return current break without changing it (useful for monitoring heap size).
   - On success: Returns **previous** program break (i.e., pointer to start of newly allocated region if growing).
   - On error: Returns `(void *) -1`.

```c
void *current_break = sbrk(0);           // Get current break
void *new_memory = sbrk(4096);           // Allocate ~4KB
if (new_memory == (void *)-1) {
    // Error
}
// new_memory points to start of new block
// current_break + 4096 ≈ new program break
```

#### Important Limitations and Dangers
- **Never set break below initial value** (just after `.bss`):
  - Can make parts of `.data` or `.bss` inaccessible → likely **segmentation fault** (`SIGSEGV`).
- Upper limit depends on:
  - Process resource limit `RLIMIT_DATA` (Section 36.3)
  - Location of shared libraries, memory mappings (`mmap`), shared memory
- **Not portable**:
  - SUSv2 marked `brk()`/`sbrk()` as **LEGACY**.
  - SUSv3 **removed** them from the standard.
  - Still available on Linux and most UNIX systems, but **avoid in new code** — use `malloc()` family instead.

### Allocating Memory on the Heap: malloc() and free()

#### Advantages of `malloc()` over `brk()`/`sbrk()`
- **Standardized** in the C language (ANSI C / SUSv3) → portable across all UNIX/POSIX systems.
- **Thread-safe** — safe to use in multithreaded programs (glibc implementation uses locks).
- **Fine-grained allocation** — can request memory in **small units** (bytes), not page-sized chunks.
- **Flexible deallocation** — any allocated block can be freed at any time; freed blocks are placed on a **free list** and **recycled** for future allocations.

#### Core Functions

1. **`malloc()`**
   ```c
   #include <stdlib.h>
   void *malloc(size_t size);
   ```
   - Allocates `size` bytes of **uninitialized** memory on the heap.
   - Returns a **properly aligned** pointer (usually 8- or 16-byte boundary) suitable for any C data type.
   - Returns `NULL` on failure (e.g., out of memory) and sets `errno` (typically `ENOMEM`).
   - **Always check for `NULL`**!
   - `malloc(0)`: May return `NULL` **or** a unique pointer that can be passed to `free()` (Linux/glibc returns a small block).

2. **`free()`**
   ```c
   #include <stdlib.h>
   void free(void *ptr);
   ```
   - Deallocates a block previously returned by `malloc()`, `calloc()`, `realloc()`, etc.
   - If `ptr == NULL`, does **nothing** (safe to call).
   - **Undefined behavior** if:
     - `ptr` is not a valid allocated pointer
     - `ptr` is already freed (double free)
     - `ptr` is corrupted (e.g., buffer overflow into header)

#### How `free()` Manages the Program Break
- **Usually does NOT lower the program break** — instead:
  - Adds the freed block to an internal **free list**.
  - Coalesces adjacent free blocks to reduce fragmentation.
  - Recycles these blocks for future `malloc()` calls.
- Reasons:
  - Freed blocks are typically **in the middle** of the heap → can't shrink break 🤷‍♀️.
  - Reduces expensive `sbrk()` system calls.
  - Many programs hold or repeatedly reallocate memory → shrinking wouldn't help.

- **Exception**: When a **large enough contiguous free block** exists at the **top of the heap**, glibc `free()` will call `sbrk()` to **lower the program break**.
  - Threshold: typically ~128 KB (tunable via `mallopt()`).

#### Demonstration Program (Listing 7-1: `free_and_sbrk`)
- Allocates `numAllocs` blocks of `blockSize` bytes.
- Frees blocks in a configurable pattern.
- Prints program break before/after allocation and after freeing.

**Key Observations from Examples**:
1. Freeing **scattered** blocks (e.g., every 2nd block) → **no change** in program break.
2. Freeing **all but the last** blocks → still **no change** (top block not free).
3. Freeing a **contiguous region at the top** → program break **decreases** (glibc shrinks heap).

#### To `free()` or Not to `free()`?
- When a process terminates, **all heap memory is automatically reclaimed** by the kernel.
- Many programs **omit `free()`** calls for memory held until exit:
  - Saves CPU time (especially with many small allocations).
  - Simpler code.

- **Reasons to explicitly `free()`**:
  - Improves **readability and maintainability**.
  - Essential when using **memory leak detectors** (e.g., valgrind, mtrace) — unfreed memory is reported as a leak.
  - Good habit for long-running programs or when memory usage patterns vary.

### Implementation of malloc() and free()
