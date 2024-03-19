#include "kernel/types.h"
#include "user/user.h"
#include"kernel/param.h"
/*
     从标准输入中按行读取，并且为每一行执行一个命令，将行作为参数提供给命令


     对于输入的一行命令 |xargs 之前的是标准输入，后面的是参数

     | 已经可以把前面的命令处理掉并且把输出给到xargs的标准输入了，
     
     所以我们需要做的就是从标准输入里面读到这些内容，并且把每一行作为参数给后面接着的命令去执行


*/

int main(int argc,char *argv[])
{
    if (argc < 2) {
        fprintf(2, "Usage: xargs command\n");
        exit(1);
    }

    char *new_argvs[MAXARG]={0};
    //第0个参数是xargs,所以应当从第一个开始读起
    for(int i=1;i<argc;i++)
    {
        new_argvs[i-1]=argv[i];
    }


    //从标准输入读取一行数据(一个字符一个字符读取)
    char buf;
    char new_arg[512], *p=new_arg;
    while(read(0 , &buf , sizeof(buf)))
    {
        //读取到换行符，执行命令
        if(buf=='\n')
        {
            if(fork()==0)
            {
                new_argvs[argc-1]=new_arg;
                *p = '\0';
                exec(argv[1],new_argvs);
                fprintf(1, "exec %s failed\n", argv[1]); // 执行失败
                exit(0);
            }
            else
            {
                wait(0);
                memset(new_arg,0,sizeof(new_arg));
                p=new_arg;
            }
        }
        else
        {
           *p=buf;
           p++;
        }
    }

    return 0;
}
