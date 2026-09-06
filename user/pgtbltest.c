#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"
#include "kernel/vm.h"

#define SZ (8 * SUPERPGSIZE)

void print_pgtbl();
void print_kpgtbl();
void ugetpid_test();
void superpg_fork();
void superpg_free();
#ifndef BUDDY_INCOMPLETE
void buddy_contig();
void buddy_coalescing();
#endif


int
main(int argc, char *argv[])
{
  print_pgtbl();
  ugetpid_test();
  print_kpgtbl();
  superpg_fork();
  superpg_free();
#ifndef BUDDY_INCOMPLETE
  buddy_contig();
  buddy_coalescing();
#endif
  printf("pgtbltest: all tests succeeded\n");
  exit(0);
}

char *testname = "???";

void
err(char *why)
{
  printf("pgtbltest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

void
print_pte(uint64 va)
{
    pte_t pte = (pte_t) pgpte((void *) va);
    printf("va 0x%lx pte 0x%lx pa 0x%lx perm 0x%lx\n", va, pte, PTE2PA(pte), PTE_FLAGS(pte));
}

void
print_pgtbl()
{
  printf("print_pgtbl starting\n");
  for (uint64 i = 0; i < 10; i++) {
    print_pte(i * PGSIZE);
  }
  uint64 top = MAXVA/PGSIZE;
  for (uint64 i = top-10; i < top; i++) {
    print_pte(i * PGSIZE);
  }
  printf("print_pgtbl: OK\n");
}

void
ugetpid_test()
{
  int i;

  printf("ugetpid_test starting\n");
  testname = "ugetpid_test";

  if(getpid() != ugetpid())
    err("mismatched PID #1");

  for (i = 0; i < 64; i++) {
    int ret = fork();
    if (ret != 0) {
      wait(&ret);
      if (ret != 0)
        exit(1);
      continue;
    }
    if (getpid() != ugetpid())
      err("mismatched PID #2");
    exit(0);
  }
  printf("ugetpid_test: OK\n");
}

void
print_kpgtbl()
{
  printf("print_kpgtbl starting\n");
  kpgtbl();
  printf("print_kpgtbl: OK\n");
}


void
supercheck(char *end)
{
  pte_t last_pte = 0;
  uint64 a = (uint64) end;
  uint64 s = SUPERPGROUNDUP(a);

  for (; a < s; a += PGSIZE) {
    pte_t pte = (pte_t) pgpte((void *) a);
    if (pte == 0) {
      err("no pte");
    }
  }

  for (uint64 p = s;  p < s + 512 * PGSIZE; p += PGSIZE) {
    pte_t pte = (pte_t) pgpte((void *) p);
    if(pte == 0)
      err("no pte");
    if ((uint64) last_pte != 0 && pte != last_pte) {
        err("pte different");
    }
    if((pte & PTE_V) == 0 || (pte & PTE_R) == 0 || (pte & PTE_W) == 0){
      err("pte wrong");
    }
    last_pte = pte;
  }

  for(int i = 0; i < 512 * PGSIZE; i += PGSIZE){
    *(int*)(s+i) = i;
  }

  for(int i = 0; i < 512 * PGSIZE; i += PGSIZE){
    if(*(int*)(s+i) != i)
      err("wrong value");
  }
}

void
superpg_fork()
{
  int pid;
  
  printf("superpg_fork starting\n");
  testname = "superpg_fork";
  
  char *end = sbrk(SZ);
  if (end == 0 || end == SBRK_ERROR)
    err("sbrk failed");

  // check if parent has super pages
  supercheck(end);
  if((pid = fork()) < 0) {
    err("fork");
  } else if(pid == 0) {
    // check if child's address space has super pages
    supercheck(end);
    exit(0);
  } else {
    int status;
    wait(&status);
    if (status != 0) {
      exit(0);
    }
  }

  // free super pages
  sbrk(-SZ);
  if((pid = fork()) < 0) {
    err("fork");
  } else if(pid == 0) {
    // reference freed memory; this should result in page fault and
    // the kernel should kill the child.
    * (end + 1) = '9'; 
  } else {
    int status;
    wait(&status);
    if (status == 0) {
      err("child was able to reference free memory\n");
      exit(1);
    }
  }  
  printf("superpg_fork: OK\n");  
}

void
superpg_free()
{
  int pid;
  
  printf("superpg_free starting\n");
  testname = "superpg_free";

  char *end = sbrk(SZ);
  if (end == 0 || end == SBRK_ERROR)
    err("sbrk failed");

  // free pages beyond a super page
  char *a = sbrk(0);
  uint64 s = SUPERPGROUNDDOWN((uint64) a);
  sbrk(-((uint64) a-s));
  a = sbrk(0);

  pte_t pte1 = (pte_t) pgpte((void *) a-PGSIZE);
  pte_t pte2 = (pte_t) pgpte((void *) a-2*PGSIZE);
  if (pte1 != pte2) {
    err("not a super page");
  }
  
  // write to the last 8192-byte section of a super page
  * (a - PGSIZE + 1) = '8';
  * (a - 2*PGSIZE + 1) = '9';

  // free last 4096 bytes of a super page
  sbrk(-PGSIZE);
  a = sbrk(0);

  if (*(a - PGSIZE + 1) != '9') {
    err("lost content after freeing part of super page");
  }

  if((pid = fork()) < 0) {
    err("fork");
  } else if(pid == 0) {
     // the memory at address a shouldn't be in the child's address
     // space, since the parent freed it. The following reference
     // should result in page fault and the kernel should kill the
     // child.
    if (* (a + 1) == '9') {
      exit(0);
    }
  } else {
    int status;
    wait(&status);
    if (status == 0) {
      err("child was able to reference free memory\n");
      exit(1);
    }
  }

  pte1 = (pte_t) pgpte((void *) a);
  if(pte1 != 0) {
    err("pte for freed memory is valid");
  }

  s = SUPERPGROUNDDOWN((uint64) a);
  for (; (uint64) a > s; a -= PGSIZE) {
    a = sbrk(-PGSIZE);
    pte1 = (pte_t) pgpte(sbrk(0));
    if(pte1 != 0) {
      err("page hasn't been freed");
    }
  }
  
  printf("superpg_free: OK\n");  
}

#ifndef BUDDY_INCOMPLETE
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
