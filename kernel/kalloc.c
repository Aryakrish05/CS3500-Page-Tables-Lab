// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

#ifdef BUDDY_INCOMPLETE
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
#endif

void
kinit()
{
#ifdef BUDDY_INCOMPLETE
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
#endif
#ifndef BUDDY_INCOMPLETE
  buddyinit((uint64)end, PHYSTOP);
#endif
}

#ifdef BUDDY_INCOMPLETE
//assumption - this is called only in kinit()
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    kfree(p);
}
}
#endif

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
#ifdef BUDDY_INCOMPLETE
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
#endif

#ifndef BUDDY_INCOMPLETE
  buddyfree(pa, 1);
#endif
}

void 
superfree(void* pa){
#ifdef BUDDY_INCOMPLETE
  // YOUR CODE HERE (needed if buddyalloc is not implemented)
  return;
#endif

#ifndef BUDDY_INCOMPLETE
  if(((uint64)pa % SUPERPGSIZE) != 0)
    panic("superfree: not superpage aligned");
  buddyfree(pa, SUPERPGSIZE / PGSIZE);
#endif
}

#ifndef BUDDY_INCOMPLETE
// Frees buddy allocated npages contiguous pages with start at pa
// npages should be a power of 2
void
kfree_contig(void* pa,int npages){
  buddyfree(pa, npages);
}
#endif

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
#ifdef BUDDY_INCOMPLETE
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
#endif

#ifndef BUDDY_INCOMPLETE
  void *pa = buddyalloc(0);
  if(pa)
    memset(pa, 5, PGSIZE); // fill with junk
  return pa;
#endif
}

void*
superalloc(void)
{
#ifdef BUDDY_INCOMPLETE
  // YOUR CODE HERE (needed if buddyalloc is not implemented)
  return 0;
#endif

#ifndef BUDDY_INCOMPLETE
  uint8 order = 0;
  while(((uint64)1 << order) * PGSIZE < SUPERPGSIZE)
    order++;

  void *pa = buddyalloc(order);
  if(pa)
    memset(pa, 5, SUPERPGSIZE); // fill with junk
  return pa;
#endif
}

#ifndef BUDDY_INCOMPLETE
// Allocate npages contiguous pages. npages must be a power of two
// no larger than the maximum buddy block.
void* 
kalloc_contig(int npages)
{
  if(npages <= 0 || (npages & (npages - 1)) != 0 ||
     npages > SUPERPGSIZE / PGSIZE)
    panic("kalloc_contig: invalid page count");

  uint8 order = 0;
  while((1 << order) < npages)
    order++;

  return buddyalloc(order);
}
#endif
