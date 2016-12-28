#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <time.h>
#define DEBUGLOG

static int ledfd = 0;
char date_time[20] = {0};

typedef enum
{
	LED_OK = 10,
	LED_COMM = 20,
	LED_FAULT = 30,
} eLEDType;

#define FILE_NAME       "/home/led_test.log"


char *get_current_time()                //发给EMA记录时获取的时间，格式：年月日时分秒，如20120902142835
{
        time_t tm;
        struct tm record_time;    //记录时间

        time(&tm);
        memcpy(&record_time, localtime(&tm), sizeof(record_time));

        sprintf(date_time, "%04d-%02d-%02d %02d:%02d:%02d", record_time.tm_year+1900, record_time.tm_mon+1, record_time.tm_mday,
                        record_time.tm_hour, record_time.tm_min, record_time.tm_sec);

        return date_time;
}
void printdecmsg(char *msg, int data)           //打印整形数据
{

#ifdef DEBUGLOG
        FILE *fp;

        fp = fopen(FILE_NAME, "a");
        if(fp)
        {
                fprintf(fp, "[%s] %s: %d!\n", get_current_time(), msg, data);
                fclose(fp);
        }
        //file_rename();
#endif
}

	

int LED_Init()
{
	int ret = 0;
	ledfd = open("/dev/led",O_WRONLY);
	printf("ledfd:%d\n",ledfd);
	if(ledfd < 0)
		ret = -1;
	return ret;
}

void LED_DeInit()
{
	close(ledfd);
}

int LED_Control(eLEDType ledType,const char* ledStatus)
{
	ioctl(ledfd,ledType);
	write(ledfd,ledStatus,1);
}

int main() {
	int ret = 0,i = 0;
	ret = LED_Init();
	usleep(500000);
	if(!ret)
	{
		while(1)
		{
			printdecmsg("led_test",++i);
			LED_Control(LED_OK,"0");
			LED_Control(LED_FAULT,"1");
			usleep(500000);
			LED_Control(LED_COMM,"0");
			LED_Control(LED_OK,"1");
			usleep(500000);
			LED_Control(LED_FAULT,"0");
			LED_Control(LED_COMM,"1");
			usleep(500000);

		}
	}

	LED_DeInit();

	return 0;
}

