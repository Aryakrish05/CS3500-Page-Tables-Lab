#define SBRK_EAGER 1
#define SBRK_LAZY  2
#ifdef LAB_PGTBL
#ifndef BUDDY_OFF
#define SBRK_CONTIG 3
#endif
#endif
