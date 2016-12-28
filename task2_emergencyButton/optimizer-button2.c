#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <pthread.h>

static int ledfd = 0;

typedef enum
{
	LED_OK = 10,
	LED_COMM = 20,
	LED_FAULT = 30,
} eLEDType;

int LED_Init()
{
	int ret = 0;
	ledfd = open("/dev/led",O_WRONLY);
	printf("ledfd:%d\n",ledfd);
	return (ledfd < 0)?-1:0;
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

void Thread_HorseRaceLED()
{
	while(1)
	{
		LED_Control(LED_FAULT,"1");
		LED_Control(LED_OK,"0");
		usleep(100000);
		LED_Control(LED_OK,"1");
		LED_Control(LED_COMM,"0");
		usleep(100000);
		LED_Control(LED_COMM,"1");
		LED_Control(LED_FAULT,"0");
		usleep(100000);
	}
}

void setBigButton(const char *flag)
{	
	FILE *fp = fopen("/etc/yuneng/bigbutton.conf","w");
	if(fp){
		fwrite(flag,1,1,fp);
		fflush(fp);
		fclose(fp);
	}
}

int get_button_status()
{
	int fd, ret;
	char buff[2]={'1'};

	fd = open("/dev/emergency_switch",O_RDONLY);
	if(fd){
		ret = read(fd, buff, 1);
		close(fd);
	}

	return atoi(buff);
}


int main()
{
	int ret = 0;
	int flag = 0,prevflag = 0;
	int buttonStatus = 0;
	pthread_t thread_LED = 0;
	LED_Init();
	LED_Control(LED_OK,"1");
	LED_Control(LED_COMM,"1");
	LED_Control(LED_FAULT,"1");
	while(1)
	{
		prevflag = flag;
		if((flag = get_button_status()) && (flag != prevflag))
		{
			buttonStatus = (buttonStatus == 0)?1:0;
			printf("%d\n",buttonStatus);		
			if(buttonStatus)
			{	//LED light
				setBigButton("1"); 
				ret = pthread_create(&thread_LED,NULL,(void *)Thread_HorseRaceLED,NULL);
				if(ret)	printf("Create pthread error!\n");
			}
			else
			{	//LED not light
				pthread_cancel(thread_LED);
				pthread_join(thread_LED,NULL);
				setBigButton("0"); 
				
				LED_Control(LED_OK,"1");
				LED_Control(LED_COMM,"1");
				LED_Control(LED_FAULT,"1");
			}
		}
	}
	LED_DeInit();
}
