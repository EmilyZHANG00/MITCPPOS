#include "kernel/types.h"
#include "user/user.h"

int main(int argc,char *argv[])
{
    if(argc!=2)
    {
        printf("no argument!");
        exit(1);
    }
    else
    {
         //第二个参数为休眠时间;
         sleep(atoi(argv[1]));
         exit(0);
    }
    return 0;
}
