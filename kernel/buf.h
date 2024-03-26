struct buf {
  int valid;   // has data been read from disk?   是否包含块的副本
  int disk;    // does disk "own" buf?    是否已经交给磁盘
  uint dev;         //设备号
  uint blockno;     //区块号
  struct sleeplock lock;      //睡眠锁
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  uint64 lastusetime;    // 最后使用时间
};

