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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// //使用自旋锁
struct {
  int ref_cnt[(PHYSTOP - KERNBASE) >> 12 ];
  struct spinlock lock;
} cows;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&cows.lock, "cows");   //初始化自旋锁
  freerange(end, (void*)PHYSTOP);   //在最一开始的时候，会把所有的物理块都进行kfree掉
}



/*
   freerange是调用kfree进行把所有物理块都清空一下子的，所以这里还需要考虑一个初始状态
   首先kfree进行的操作是，如果引用数目减一之后为0的话才会进行释放，否则只直接把引用数目减去1
   那如果一开始引用数目就是0呢？？这个时候就需要特殊处理一下了，
   一开始freerange的时候引用数目为0，但是也还是需要挨个释放并且把这些空前空间全部都串在一起，穿在一个链表上，否则一开始会找不到空闲空间！那么也就无法分配了
*/
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    acquire(&cows.lock);
    cows.ref_cnt[((uint64)p -KERNBASE)/ PGSIZE] = 1;
    release(&cows.lock);
    kfree(p);
  }
}



// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
 void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");


  // // Fill with junk to catch dangling refs.
  // memset(pa, 1, PGSIZE);

  // r = (struct run*)pa;

  // acquire(&kmem.lock);
  // r->next = kmem.freelist;
  // kmem.freelist = r;
  // release(&kmem.lock);

  // 只有当引用值为0的时候，才真正释放页面 , 否则只单纯的将引用数目减一
  acquire(&cows.lock);
  if(--cows.ref_cnt[((uint64)pa-KERNBASE) / PGSIZE] == 0) {
    release(&cows.lock);

    r = (struct run*)pa;
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);

  } else {
    release(&cows.lock);
  }
}

// // Allocate one 4096-byte page of physical memory.
// // Returns a pointer that the kernel can use.
// // Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    kmem.freelist = r->next;
    //在一开始kalloc一个页面的时候,需要对引用值赋值为1
    acquire(&cows.lock);
    cows.ref_cnt[((uint64)r-KERNBASE) / PGSIZE] =1;
    release(&cows.lock);
  } 
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}




//MY
// // 判断某个虚拟地址是不是写时复制页面   0表示是，-1表示不是
uint64 cowpage(pagetable_t pagetable, uint64 va)
{
    
   if(va >= MAXVA)
    return -1;
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0)
    return -1;
  if((*pte & PTE_V) == 0)
    return -1;
  return (*pte & PTE_F ? 0 : -1);
}

// // 给某个虚拟地址为va映射到物理地址为pa的页面kalloc一个页面，并且建立映射,分配一个写时复制的页
// // 要求va必须是页面对齐的   alloccowpage
void*  cowalloc(pagetable_t pagetable, uint64 va)
{

    if(cowpage(pagetable,va)<0)
    return 0;

    va = PGROUNDDOWN(va);


    uint64  pa = walkaddr(pagetable,va);
    if(pa==0)
      return 0;   
   
    //这里是要给一个cow页面进行分配新的物理空间，但是如果引用数目已经是1的话，就不需要再进行新的页面分配了
    pte_t *pte = walk(pagetable,va,0);
    if(cows.ref_cnt[(uint64)(pa-KERNBASE)/PGSIZE]==1)
    {
          *pte |= PTE_W;
          *pte &=  ~PTE_F;
          return  (void*)pa;
    }
    else
    {
        //进行新的页面分配，并且把原来的引用数目减去1
        char *mem = kalloc();
        if( mem == 0)
          return 0;    
        
        // 复制整个旧页面的内容 
        memmove(mem, (char*)pa, PGSIZE);

        // 需要先去掉有效位，因为在mappages会首先去判断当前的表项是不是有效的，如果有效的话会认为remap，就不会进行新的表项的映射了
        *pte &= ~PTE_V;          
        if(mappages(pagetable, va, PGSIZE, (uint64)mem,  (PTE_FLAGS(*pte) | PTE_W) & ~PTE_F) != 0) {
          kfree(mem);
          *pte |= PTE_V;
          return 0;
        }
        kfree((char*)pa);    //物理内存 引用数目减1
       return mem;
    }
}

// // 某个页面的引用数+1   inscowcnt 
uint64 kaddrefcnt(uint64 pa)
{
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return -1;

    acquire(&cows.lock);
    cows.ref_cnt[(uint64)(pa-KERNBASE) / PGSIZE] +=1;
    release(&cows.lock);
    return  cows.ref_cnt[(uint64)(pa-KERNBASE) / PGSIZE];
}
