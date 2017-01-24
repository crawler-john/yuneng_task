#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

//#define DEBUG 0
//#define DEBUGLOG 0

void printmsg(char *msg)		//打印字符串
{
#ifdef DEBUG
	printf("clientmonitor==>%s!\n", msg);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/clientmonitor.log", "a");
	if(fp)
	{
		fprintf(fp, "%s!\n", msg);
		fclose(fp);
	}
#endif
}

void printdecmsg(char *msg, int data)		//打印整形数据
{
#ifdef DEBUG
	printf("clientmonitor==>%s: %d!\n", msg, data);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/clientmonitor.log", "a");
	if(fp)
	{
		fprintf(fp, "%s: %d!\n", msg, data);
		fclose(fp);
	}
#endif
}

int main(void)
{
	pid_t appid;
	int pid;

	appid = vfork();	//client以子进程运行
	if(appid == 0)
	{
		pid = getpid();
		printdecmsg("Starting client", pid);
		execl("/home/applications/client", NULL);
		exit(0);
	}

	while(1)
	{
		waitpid(pid, NULL, 0);	//等待子进程结束，在结束前本程序暂停运行
		printmsg("client has been stoped");	//如果运行到此处，说明client已经中止，重新运行client
		sleep(10);
		appid = vfork();
		if(appid == 0)
		{
			pid = getpid();
			printdecmsg("Starting client", pid);
			execl("/home/applications/client", NULL);
			exit(0);
		}
	}
	return 0;
}