#include "kernel/types.h"
#include "user/user.h"
/* 
   怎么处理读数和取数的顺序？
   不断地从左边读

   fork安排在哪里？
   似乎应该在第一次往右管道放数字的时候就fork出一个新的进程/但是参考答案似乎并不是这样做的？
*/

void transmitprime(int fromleft[2],int first,int isfirstsend)
{
    close(fromleft[1]);   //关闭左邻居的写管道

    //子进程第一件事情是读父进程发来的数据并输出
    int buf;
    if(read(fromleft[0],&buf,sizeof(buf))==sizeof(int))
    {    
        first=buf;
        fprintf(1,"prime %d\n",buf);
    }
    else
    {
        exit(0);
    }


    int toright[2];
    if(pipe(toright)==-1)
    {
       // fprintf(1,"%d :pipe error\n",getpid());
        exit(1); 
    }

    //1.从左边的管道读一个数
    while(read(fromleft[0],&buf,sizeof(buf))==sizeof(buf))  
    {
            if(buf % first!=0)
            {
                //先把数据写进去
                if(write(toright[1],&buf,sizeof(buf))!=sizeof(buf))
                {
                 //   fprintf(1,"%d :write buf error!\n",getpid());
                    exit(0);
                }

                // if(isfirstsend==1)     // 如果是第一次写，则创建一个进程作为其右邻居
                // {
                //     if(fork()==0)        // 子进程才需要执行，父进程直接往下继续判断
                //     {
                //         first=-1;
                //         isfirstsend=1;
                //         transmitprime(toright,first,isfirstsend);
                //     }
                // }
                
                // else
                // {
                //     isfirstsend = 0;
                // }
            }
        }
    
    //把左边的数据读完了，并且把相应的送到了右边邻居，现在开始创建右边邻居
    if(fork()==0)     //子进程才需要执行，父进程直接往下继续判断
    {
        first=-1;
        isfirstsend=1;
        transmitprime(toright,first,isfirstsend);
    }

    //关闭左邻居的读管道和右邻居的写管道
    close(fromleft[0]); 
    close(toright[1]);

    wait(0);   
    exit(0);
}

int main()
{
    int first = -1;
    int isfirstsend = 1;
    
    int toright[2];
    if(pipe(toright)==-1)
    {
        fprintf(1,"%d :pipe error\n",getpid());
    }

    for(int i=2;i<=35;i++)
    {
            if(write(toright[1],&i,sizeof(i))!=sizeof(i))
            {
                fprintf(1,"%d :write buf error!\n",getpid());
            }
    }
    
    if(fork()==0)
    {
        transmitprime(toright,first,isfirstsend);
    }


    close(toright[0]);
    close(toright[1]);
    wait(0);   //等待子进程退出才退出
    exit(0);
}
