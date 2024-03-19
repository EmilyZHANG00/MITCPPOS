#include "kernel/types.h"
#include "user/user.h"

int main()
{
    //1 创建管道
    int pipefdtochild[2];   //父进程向子进程发送消息的管道
    int pipefdtoparent[2];   //子进程向父进程发送消息的管道  0为读端，1为写端
    if(pipe(pipefdtochild)==-1)   
    {
        printf("something error when create pipe to child!\n");
        exit(0);
    }
    if(pipe(pipefdtoparent)==-1)   
    {   
        //关闭已经打开的管道
        fprintf(2, "fork() error!\n");
        close(pipefdtochild[0]);
        close(pipefdtochild[1]);
        fprintf(1,"something error when create pipe to parent!\n");
        exit(1);
    }


    //2 创建子进程
    int pid=fork();

    //3.发送/接收消息
    if(pid<0)
    {
        fprintf(2, "fork() error!\n");
        close(pipefdtochild[0]);
        close(pipefdtochild[1]);
        close(pipefdtoparent[0]);
        close(pipefdtoparent[1]);
        exit(1);
    }else if(pid==0)
    { 
        int exit_status = 0;
        //关闭向子进程写、向父进程读的管道
        close(pipefdtoparent[0]);
        close(pipefdtochild[1]);

        char buf[64];
        if(read(pipefdtochild[0],buf,sizeof(buf)) != strlen(buf))
        {
            fprintf(1,"child read() error!\n");
            exit_status=1;
        }
        else
        {
            fprintf(1,"%d: received %s\n",getpid(),buf);
        }
        //关闭向子进程的读
        close(pipefdtochild[0]);


        //子进程向父进程发送pong
        memset(buf,0,sizeof(buf));
        memcpy(buf,"pong",strlen("pong"));
        int n=write(pipefdtoparent[1],buf,strlen(buf));
        if(n != strlen(buf))
        {
            fprintf(1,"child write() error!\n");
            exit_status=1;
        }

        //关闭向父进程的写
        close(pipefdtoparent[1]);

        //发送完之后等待父进程发消息(如果没有消息就会阻塞在这里)

        exit(exit_status);
    }
    else
    {
        //关闭向父进程的写 和 向子进程的读
        close(pipefdtochild[0]);
        close(pipefdtoparent[1]);
        int exit_status = 0;

        //向子进程发送ping
        char buf[64]="ping";
        int n=write(pipefdtochild[1],buf,strlen(buf));
        if( n!= strlen(buf))
        {
            fprintf(1,"parent write() error!\n");
            exit_status = 1;
        }
        
        //关闭向子进程写管道
        close(pipefdtochild[1]);

        //接收子进程发送的消息 pong
        n=read(pipefdtoparent[0],buf,sizeof(buf));
        if(n != strlen(buf))
        {
            fprintf(1,"parent read() error!\n");
            exit_status = 1;

        }
        fprintf(1,"%d: received %s\n",getpid(),buf);

        //关闭向父进程的读
        close(pipefdtoparent[0]);

        exit(exit_status);
    }
}
