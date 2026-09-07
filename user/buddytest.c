#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/buddyalloc.h"
#include "user/user.h"
#include "kernel/vm.h"

#ifndef BUDDY_OFF
void buddy_contig();
void buddy_coalescing();
#endif

int
main(int argc, char *argv[])
{
#ifndef BUDDY_OFF
  buddy_contig();
  buddy_coalescing();
  printf("buddytest: all tests succeeded\n");
#endif
  exit(0);
}

char *testname = "???";

void
err(char *why)
{
  printf("buddytest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

void
print_pte(uint64 va)
{
    pte_t pte = (pte_t) pgpte((void *) va);
    printf("va 0x%lx pte 0x%lx pa 0x%lx perm 0x%lx\n", va, pte, PTE2PA(pte), PTE_FLAGS(pte));
}

#ifndef BUDDY_OFF
void
buddy_contig()
{
  // tries to allocate 2^i contiguous pages for i = 0, 1, 2, ..., MAXORDER
  // and checks if the pages mapped are contiguous
  printf("buddy_contig starting\n");
  testname = "buddy_contig";

  for(int order = 1; order <= 9; order++){
    int npages = 1 << order;
    int bytes = npages * PGSIZE;
    char *start = sbrkcontig(bytes);
    if(start == 0 || start == SBRK_ERROR)
      err("contiguous allocation failed");

    uint64 base = PGROUNDUP((uint64)start);
    uint64 previous = 0;
    for(int page = 0; page < npages; page++){
      pte_t pte = (pte_t)pgpte((void *)(base + page * PGSIZE));
      if((pte & PTE_V) == 0)
        err("contiguous page is unmapped");
      if(page > 0 && PTE2PA(pte) != previous + PGSIZE)
        err("physical pages are not contiguous");
      previous = PTE2PA(pte);
    }

    if(sbrk(-bytes) == SBRK_ERROR)
      err("contiguous deallocation failed");
  }

  printf("buddy_contig: OK\n");
}

void
buddy_coalescing()
{
  // first allocates all blocks of size 1, exhausting memory
  // then frees all blocks of size 1
  // tries to allocate 2^i contiguous pages for i = 0, 1, 2, ..., MAXORDER
  // and checks if the pages mapped are contiguous
  printf("buddy_coalescing starting\n");
  testname = "buddy_coalescing";

  int pages = 0;
  while(sbrk(PGSIZE) != SBRK_ERROR)
    pages++;

  if(pages == 0)
    err("could not allocate single pages");
  if(sbrk(-pages * PGSIZE) == SBRK_ERROR)
    err("failed to free single pages");

  for(int order = 1; order <= 9; order++){
    int npages = 1 << order;
    int bytes = npages * PGSIZE;
    char *mem = sbrkcontig(bytes);
    if(mem == 0 || mem == SBRK_ERROR)
      err("coalesced allocation failed");

    uint64 base = PGROUNDUP((uint64)mem);
    uint64 previous = 0;
    for(int page = 0; page < npages; page++){
      pte_t pte = (pte_t)pgpte((void *)(base + page * PGSIZE));
      if((pte & PTE_V) == 0)
        err("coalesced page is unmapped");
      if(page > 0 && PTE2PA(pte) != previous + PGSIZE)
        err("coalesced pages are not contiguous");
      previous = PTE2PA(pte);
    }

    if(sbrk(-bytes) == SBRK_ERROR)
      err("coalesced deallocation failed");
  }

  printf("buddy_coalescing: OK\n");

}
#endif