// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKETNUM   13     // 设置13个桶

// bcache 和kmeme结构体类似的，包括一个锁，
// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.

//   struct buf head;      // 根据最近使用程度进行排序，head的下一个是最新的
// } bcache;

struct buf BUFCACHE[NBUF];     //具体的n个缓冲块实体

struct {
  struct spinlock lock;
  uint64    bufcnt;    //当前桶中的区块数目
  uint64    freebufcnt;  //当前空闲的区块数目
  struct buf head;  //头节点是一个空结点，只是用来方便进行一些操作
} bcache[BUCKETNUM];


int hash(uint blockno)
{
  return blockno % BUCKETNUM;
}

void
binit(void)
{
  for(int i=0;i<BUCKETNUM;i++)
  {
    initlock(&bcache[i].lock, "bcache");
    // Create linked list of buffers
    bcache[i].bufcnt=0;
    bcache[i].freebufcnt=0;
    bcache[i].head.prev = &bcache[i].head;
    bcache[i].head.next = &bcache[i].head;
  }

  //一开始将所有的空余节点都放在0节点上
  bcache[0].bufcnt=NBUF;
  bcache[0].freebufcnt=NBUF;

  struct buf *b;
  for(b = BUFCACHE; b < BUFCACHE+NBUF; b++){
    //把所有节点以头插法串联起来
    b->next = bcache[0].head.next;
    b->prev = &bcache[0].head;
    initsleeplock(&b->lock, "buffer");    //初始化每一个缓冲块自己的睡眠锁
    bcache[0].head.next->prev = b;
    bcache[0].head.next = b;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

// 扫描缓冲区列表，查找具有给定设备号和扇区号的缓冲区，如果存在，bget将获取缓冲区的睡眠锁，然后返回锁定的缓冲区
// 如果不存在，再次扫描缓冲区列表，查找未在使用的缓冲区，编辑缓冲区的元数据以记录新的设备和扇区号并获得锁

static struct buf*
bget(uint dev, uint blockno)
{
  int bucketid=hash(blockno);
  struct buf *b;

  acquire(&bcache[bucketid].lock);

  // 1  找到该映射块，直接返回(只能根据head,一次遍历每个链表)
  for(b = bcache[bucketid].head.next; b != &bcache[bucketid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->lastusetime=ticks;

      release(&bcache[bucketid].lock);     //释放整个链表的锁
      acquiresleep(&b->lock);              // 对这个区块内容的锁需要重新锁上，因为这说明当前这个缓存块需要被用了
      return b;
    }
  }

  // 2 如果没有找到，需要找到一个块来替换(需要找到最久未使用的那一块)
  // Recycle the least recently used (LRU) unused buffer.
   struct buf * tmp = 0;    
   b=0;

  for(tmp = bcache[bucketid].head.next; tmp != &bcache[bucketid].head; tmp = tmp->next){
    if(tmp->refcnt == 0 && (b==0 || tmp->lastusetime < b->lastusetime)) {        // 当前查找到的块没有在被用，即可以用来进行缓存
          b=tmp;
    }
  }
  // 当前查找到的块没有在被用，即可以用来进行缓存
  if(b!=0)
  {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->lastusetime = ticks;
        release(&bcache[bucketid].lock);
        acquiresleep(&b->lock);
        return b;
  }

  //3 如果遍历当前块，还是没有找到一个空闲的桶，那么就去看看其他桶有没有空闲的块
for(int i=0;i<BUCKETNUM;i++)
  {      
      if(i==bucketid)
        continue;
      acquire(&bcache[i].lock);
      tmp = 0;    

      for(tmp = bcache[i].head.next; tmp != &bcache[i].head; tmp = tmp->next){
        if(tmp->refcnt == 0 && (b==0 || tmp->lastusetime < b->lastusetime)) {        // 当前查找到的块没有在被用，即可以用来进行缓存
                b=tmp;            }        }
      if(b!=0)
      {
        b->next->prev = b->prev;   
        b->prev->next = b->next; 
        release(&bcache[i].lock);
        // 进行换桶  把这个缓存块绑定到当前的桶上面
  
        b->next = bcache[bucketid].head.next;  
        b->prev = &bcache[bucketid].head;
        bcache[bucketid].head.next->prev = b;
        bcache[bucketid].head.next = b;

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->lastusetime=ticks;

        release(&bcache[bucketid].lock);
        acquiresleep(&b->lock);
        return b;
      }
      release(&bcache[i].lock);

  }
  release(&bcache[bucketid].lock);
  panic("bget: no buffers");
}


//  Return a locked buf with the contents of the indicated block.
//  调用 bget 为给定的扇区获取一块缓冲区
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}


// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
// 释放一个缓存块，应该把它放到head的最近使用列表中
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  
  int bucketid=hash(b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache[bucketid].lock);
  b->refcnt--;
  // if(b->refcnt==0)
  // bcache[bucketid].freebufcnt-=1;
  release(&bcache[bucketid].lock);
}

void
bpin(struct buf *b) {

  
  int bucketid=hash(b->blockno);
  acquire(&bcache[bucketid].lock);

  b->refcnt++;
  release(&bcache[bucketid].lock);
}

void
bunpin(struct buf *b) {
  int bucketid=hash(b->blockno);
  acquire(&bcache[bucketid].lock);
  b->refcnt--;
  release(&bcache[bucketid].lock);
}


