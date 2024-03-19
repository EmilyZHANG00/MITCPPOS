// copy.c: 将控制台输入内容输出到控制台

#include "kernel/types.h"
#include "user/user.h"

int main()
{
    char buf[64];     //缓冲区大小为64字节
    while(1)
    {   //读数据
       int n=read(0,buf,sizeof(buf));   //从控制台读数据，最大读64字节，n返回实际读取到的数据长度
       if(n <= 0)       //如果n小于0,那么在某些地方出现了错误，直接跳过 
       break;
       write(1,buf,n);   //把输入的输出重新输出到控制台   
    }
    exit(0);
}

