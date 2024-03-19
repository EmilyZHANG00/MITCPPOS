/*写一个简化版本的UNIX的find程序：
查找目录树中具有特定名称的所有文件，你的解决方案应该放在user/find.c

只需要查找某个目录中,特定名字的文件
*/

#include "kernel/types.h"

#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"


//因为需要递归，所以写成一个函数
int  find(char *path,char *filename)
{
    char buf[512], *p;    //buf整个路径的全名字  p会在这个过程中不断移动，以保证读取到的是当前文件的名字
    struct dirent de;
    struct stat st;
    int fd = open(path,0);  

    //1.先把要搜索的文件打开
    if(fd<0)
    {
         fprintf(1,"%s can not open",path);
         return 0;
    }

    //2.获取这个文件的stat,并且把读到的内容写到st中
    if(fstat(fd,&st)<0)
    {
         fprintf(1,"%s can not get stat",path);
         return 0;
    }

    //3.成功打开并获取到stat，判断它是不是一个目录，如果不是目录的话报错
    if(st.type!=T_DIR)
    {
        fprintf(1,"Usage find  dir  filename");
        return 0;
    }
    else
    {
        //路径长度过长
        if(strlen(path)+1+DIRSIZ+1>sizeof(buf))
        {
            fprintf(1, "find: path too long\n");
            return 0;
        }

        //进行数据的读取
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/'; //p指向最后一个'/'之后

        while(read(fd,&de,sizeof(de))==sizeof(de))   //一次读出一个表项
        {
            if(de.inum==0)
            continue;

            //把名字添加到路径中,添加字符串结尾标识符
            // memmove(p,de.name,sizeof(de.name));   //(这里没有采用DIR_SIZE)
            // p[strlen(de.name)]=0;   
            memmove(p,de.name,strlen(de.name));   //(这里没有采用DIR_SIZE)
            p[strlen(de.name)]='\0';   
            //尝试获取这个新文件的文件状态
            if (stat(buf, &st) < 0) {
                fprintf(1, "find: cannot stat %s\n", buf);
                continue;
            }
            
            //如果是DIR目录文件并且不是..和.文件夹，则递归查找;如果是普通文件,对比这个文件名字是否等于filename;
            if(st.type==T_DIR &&  strcmp(p, ".") != 0 && strcmp(p, "..") != 0)
            {
                find(buf,filename);
            }
            else   //# 注意：p指向的是当前的文件名字,buf指向的内容是整个路径名字
            {
                if(strcmp(filename,p)==0)
                {
                    fprintf(1,"%s\n",buf);
                }
            }
        }
    }
    close(fd);
    return 0;
}


int  main(int argc, char *argv[])
{
    if(argc!=3)
    {
        fprintf(1,"Usage find  dir  filename");
        return 0;
    }
    find(argv[1],argv[2]);
    exit(0);
}