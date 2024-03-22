#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

#include "spinlock.h" 
#include "proc.h"
/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S




/*
打印页表的函数
*/
void vmprint(pagetable_t pagetable)
{
  printf("page table %p\n",pagetable);
  //迭代每一项,对于非空的项目，找到下一级页表，再去迭代地判断该页表的每一项是否为空
  for(int i=0;i<512;i++)
  {
    pte_t pte1=pagetable[i];     //第一级页表
    if(pte1 & PTE_V)  //如果是有效的PTE 则找到对应的二级页表
    {
      printf("..%d: pte %p pa %p\n",i,pte1,PTE2PA(pte1));
      pagetable_t pagetable2=(pagetable_t)PTE2PA(pte1);
      for(int j=0;j<512;j++)
      {
        pte_t pte2=pagetable2[j];
        if(pte2 & PTE_V)
        {
          printf(".. ..%d: pte %p pa %p\n",j,pte2,PTE2PA(pte2));
          pagetable_t pagetable3=(pagetable_t)PTE2PA(pte2);
          for(int k=0;k<512;k++)
          {
            pte_t pte3=pagetable3[k];
            if(pagetable3[k] & PTE_V)
            {
              printf(".. .. ..%d: pte %p pa %p\n",k,pte3,PTE2PA(pte3));
            }
          }
        }
      }
    }
  }
  return;
}




/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()     //为内核进行初始化，也就是分配空间
{
  kernel_pagetable = (pagetable_t) kalloc();      //为最高一级的页表分配内存，并把所有内容初始化为0
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);     //将每一个I/O设备映射到内核  (建立一个PTE表项)

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// 为每个进程创建一个内核页表，并且把这个内核页表的地址返回
pagetable_t
proc_kpt_init(){
  pagetable_t kernelpt = (pagetable_t) kalloc();      //为最高一级的页表分配内存，并把所有内容初始化为0
  if (kernelpt == 0) return 0;
  memset(kernelpt, 0, PGSIZE);

  uvmmap(kernelpt, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kernelpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kernelpt, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  uvmmap(kernelpt, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  uvmmap(kernelpt, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  uvmmap(kernelpt, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  uvmmap(kernelpt, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  return kernelpt;
}




// Switch h/w page table register to the kernel's page table,
// and enable paging.    内核告诉MMU使用刚刚创建好的kernel_pagetable
void   
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}



// 这里不讲STAP切换为内核页表，而是切换为用户进程的内核页表
void   
proc_inithart(pagetable_t proc_kpt)
{
  w_satp(MAKE_SATP(proc_kpt));
  sfence_vma();     //清除快表的缓存
}



// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];  //返回存储va所对应的物理地址的页表表项所在的地址 (并不是直接返回相应物理地址)
}

// Look up a virtual address, return the physical address,   为虚拟空间地址返回一个物理空间地址
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.  对内核的页表进行映射
// only used when booting.   只有当开机启动的时候才使用到
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

/*
   用于进程的内核页表进行映射(具体的工作内容为为va开头，大小为sz的一块虚拟地址空间，创建映射到pa开头的同样大小的一块空间的pte表项)
   原本在内核态进行建立映射的时候，已知内核态的页表基地址，所以直接传递虚拟地址和物理地址以及需要的空间大小进行映射就可以
   但是现在我要在某个进程建立一个映射，那么就需要把该进程的页表的基地址传递进来在相应的<内存块>(即页表在的位置)上进行建立映射,映射到的物理地址都是同一块地方，但是在不同的内存块上都建立了这一映射，就使得多个不同的虚拟地址都可以映射到这同一块物理内存上
*/
void
uvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pagetable, va, sz, pa, perm) != 0)
    panic("uvmmap");
}



// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
// 把一个内核虚拟地址转换为一个物理地址.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;   // 偏移量
  pte_t *pte;        // pte 的表项地址
  uint64 pa;         // 实际物理地址
  pte = walk(myproc()->kernelpt, va, 0);   // 不再是通过内核页表去找相应的表项，而是通过用户进程的内核页表
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
/*
  为va开始的虚拟地址创建一个表项，它指向物理地址pa.
  va和size大小可能不是页面大小对齐的
  如果成功返回0，如果不能够分配页表返回1
*/
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);     // a是去掉后12位的地址，即每个page的开头地址
  last = PGROUNDDOWN(va + size - 1);  //最后一个地址
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)     // 找到应该填物理地址的具体位置
      return -1;
    if(*pte & PTE_V)       // 如果这个pte表项已经是有效的了，说明它被重复映射了
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;     //否则把设置成有效，并且把他的内容改成相应的物理地址(其实是物理页号)
    if(a == last)          
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// 创建一个新的用户页表
// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.   释放页表
// All leaf mappings must already have been removed.    所有的叶子映射都应该已经被移除
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];

    //如果不是有效的，或者不能读、写且不能执行,那么这个页表项就可以被释放掉了
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.  从源地址src赋值len个字节一个虚拟地址va，这个页表是给定的
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.   把len个字节从一个虚拟地址拷贝到目的地址dst
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  // uint64 n, va0, pa0;

  // while(len > 0){
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if(pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if(n > len)
  //     n = len;
  //   memmove(dst, (void *)(pa0 + (srcva - va0)), n);

  //   len -= n;
  //   dst += n;
  //   srcva = va0 + PGSIZE;
  // }
  // return 0;

  // 修改为直接调用copyin_new
  return copyin_new( pagetable, dst, srcva, len);
}

// Copy a null-terminated string from user to kernel.   把一个没有终止符号的字符串从用户拷贝到内核
// Copy bytes to dst from virtual address srcva in a given page table, 对给定页表内的虚拟地址拷贝到目的地址
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  // uint64 n, va0, pa0;
  // int got_null = 0;

  // while(got_null == 0 && max > 0){
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if(pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if(n > max)
  //     n = max;

  //   char *p = (char *) (pa0 + (srcva - va0));
  //   while(n > 0){
  //     if(*p == '\0'){
  //       *dst = '\0';
  //       got_null = 1;
  //       break;
  //     } else {
  //       *dst = *p;
  //     }
  //     --n;
  //     --max;
  //     p++;
  //     dst++;
  //   }

  //   srcva = va0 + PGSIZE;
  // }
  // if(got_null){
  //   return 0;
  // } else {
  //   return -1;
  // }

  return  copyinstr_new( pagetable, dst, srcva,max);
}

/*
   把进程的页表复制一份到进程的内核页表,每当有可能产生新的页的时候,都应该进行一次这样的复制

   //很巧妙的利用walk函数来进行复制,而不是从第一级页表一层一层的下去挨个判断是否已经存在相应的页表项,不存在则进行复制这样去做
*/
void
u2kvmcopy(pagetable_t pagetable, pagetable_t kernelpt, uint64 oldsz, uint64 newsz){
  pte_t *pte_from, *pte_to;
  oldsz = PGROUNDUP(oldsz);   //找到页面的开始
  for (uint64 i = oldsz; i < newsz; i += PGSIZE)
  {
    if((pte_from = walk(pagetable, i, 0)) == 0)    //找到存储该逻辑地址的页表项所在的地址
      panic("u2kvmcopy: src pte does not exist"); 
    if((pte_to = walk(kernelpt, i, 1)) == 0)       // 找到存储该逻辑地址的页表项在内核表中所应该在的地址
      panic("u2kvmcopy: pte walk failed");
    uint64 pa = PTE2PA(*pte_from);     //取出来实际的物理地址
    uint flags = (PTE_FLAGS(*pte_from)) & (~PTE_U);    //把标记位修改为用户不可访问
    *pte_to = PA2PTE(pa) | flags;
  }
}
