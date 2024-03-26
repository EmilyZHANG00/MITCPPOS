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


// 为每个CPU设置一个链表
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];



void
kinit()
{

  //对每个链表的锁进行初始化
  for(int i=0;i<NCPU;i++)
  {
     initlock(&kmem[i].lock, "kmem");
  }

  //收集从end开始的所有空闲物理页 分配给CPU0
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off() ;         //关闭中断
  int id=cpuid();
  pop_off();     //打开中断

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// void *
// kalloc(void)
// {
//   struct run *r;

//   acquire(&kmem.lock);
//   r = kmem.freelist;
//   if(r)
//     kmem.freelist = r->next;
//   release(&kmem.lock);

//   if(r)
//     memset((char*)r, 5, PGSIZE); // fill with junk
//   return (void*)r;
// }


void *
kalloc(void)
{
  struct run *r=0;

  push_off() ;         //关闭中断
  int id=cpuid();
  pop_off();     //打开中断

  acquire(&kmem[id].lock);
  if(kmem[id].freelist)   // 当前cpu的空闲链表不为空
  {
      r = kmem[id].freelist;
      kmem[id].freelist = r->next;
      release(&kmem[id].lock);
  }
  else // 空闲链表为空,遍历其它cpu的链表
  {
     release(&kmem[id].lock);
     for(int i=0; i < NCPU ; i++)
     {   
         if( i == id )
            continue;
         else
         {
              // 获取锁，
              acquire(&kmem[i].lock);
              if(kmem[i].freelist)
              {
                   r = kmem[i].freelist;
                   kmem[i].freelist = r->next;
                   release(&kmem[i].lock);
                   break;
              }
              release(&kmem[i].lock);
         }
     }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
