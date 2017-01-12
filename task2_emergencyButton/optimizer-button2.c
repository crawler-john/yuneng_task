/*****************************************************************************/
/*                                                                           */
/*  Include Files                                                            */
/*                                                                           */
/*****************************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

/*****************************************************************************/
/*                                                                           */
/*  Variable Declarations                                                    */
/*                                                                           */
/*****************************************************************************/
static int ledfd = 0;

/*****************************************************************************/
/*                                                                           */
/*  Definitions                                                              */
/*                                                                           */
/*****************************************************************************/
typedef enum
{
	LED_OK = 10,
	LED_COMM = 20,
	LED_FAULT = 30,
} eLEDType;

/*****************************************************************************/
/*                                                                           */
/*  Function Implementations                                                 */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Initialize LED                                                          */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   									     */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   the result value                                                        */
/*                                                                           */
/*****************************************************************************/
int LED_Init()
{
	int ret = 0;
	ledfd = open("/dev/led",O_WRONLY);
	if(ledfd > 0)
	{
		printf("ledfd:%d\n",ledfd);
		return 0;
	}else
	{
		printf("open led drive failed!\n");
		exit(-1);
	}
}

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   DeInitialize LED                                                        */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   the result value                                                        */
/*                                                                           */
/*****************************************************************************/
void LED_DeInit()
{
	close(ledfd);
}


/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Control LED                                                             */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   ledType[in]:          LED Type selected                                 */
/*   ledStatus[in]:        "0" : light  "1" : not light                      */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   the result value                                                        */
/*                                                                           */
/*****************************************************************************/
void  LED_Control(eLEDType ledType,const char* ledStatus)
{
	ioctl(ledfd,ledType);
	write(ledfd,ledStatus,1);
}


/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Thread of Horse Race LED                                                */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   the result value                                                        */
/*                                                                           */
/*****************************************************************************/
void Thread_HorseRaceLED()	//按键按下启动的线程
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

/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Modify  /etc/yuneng/bigbutton.conf file Flag                            */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*   flag[in]:      "0" : not pressed key  "1" : pressed key                 */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   the result value                                                        */
/*                                                                           */
/*****************************************************************************/
void setBigButton()  //设置配置文件，如果配置文件是1，则改变为0.如果配置文件是0，则改变为1.
{
        char flag;
        FILE *fp = fopen("/etc/yuneng/bigbutton.conf","r");
        if(fp){
                fread(&flag,1,1,fp);
                fclose(fp);
        }

        fp = fopen("/etc/yuneng/bigbutton.conf","w");
        if(fp){
                if(memcmp(&flag,"1",1))
                {
                        fwrite("1",1,1,fp);
                        fflush(fp);
                        fclose(fp);
                }else
                {
                        fwrite("0",1,1,fp);
                        fflush(fp);
                        fclose(fp);
                }
        }
}




int readBigButton()	//读取配置文件信息，如果没有配置文件则创建一个为0的配置文件，否则读取配置文件的内容
{
        char flag;
        FILE *fp = fopen("/etc/yuneng/bigbutton.conf","r");
        if(fp){
                fread(&flag,1,1,fp);
                fclose(fp);
                return atoi(&flag);

        }else
        {	//如果文件不存在创建文件,并写入0
                fp = fopen("/etc/yuneng/bigbutton.conf","w");
                if(fp){
                        fwrite("0",1,1,fp);
                        fflush(fp);
                        fclose(fp);
                }
                return 0;
        }
}


/*****************************************************************************/
/* Function Description:                                                     */
/*****************************************************************************/
/*   Get Button Status                                                       */
/*                                                                           */
/*****************************************************************************/
/* Parameters:                                                               */
/*****************************************************************************/
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
/* Return Values:                                                            */
/*****************************************************************************/
/*   0    -  Pressed Button                                                  */
/*   1    -  Not Pressed Button                                              */
/*****************************************************************************/
int get_button_status()	//获取按键的状态
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
	int flag = 1,prevflag = 1;	//按钮前后对比标志,初始状态为按键未按下，为1
	int buttonStatus = 0,prevbuttonStatus = 0;	//文件中，按钮对应状态标志
	pthread_t thread_LED = 0;	//按键按下线程ID
	LED_Init();		//初始化并设置LED灯都不亮
	LED_Control(LED_OK,"1");
	LED_Control(LED_COMM,"1");
	LED_Control(LED_FAULT,"1");
	while(1)
	{
		prevbuttonStatus=buttonStatus;
		buttonStatus = readBigButton();
		if(buttonStatus!=prevbuttonStatus)//读取到的状态和之前不同，则触发线程对应的命令
		{
			printf("buttonStatus:%d\n",buttonStatus);
			if(buttonStatus) 
			{	//LED light
				printf("create pthread...\n");
				ret = pthread_create(&thread_LED,NULL,(void *)Thread_HorseRaceLED,NULL);
				if(ret)	printf("Create pthread error!\n");
				printf("create pthread success\n");
			}
			else
			{	//LED not light
				pthread_cancel(thread_LED);
				pthread_join(thread_LED,NULL);
				printf("close pthread success\n");
				LED_Control(LED_OK,"1");
				LED_Control(LED_COMM,"1");
				LED_Control(LED_FAULT,"1");
			}
		}
		prevflag = flag;
		if((flag = get_button_status()) && (flag != prevflag)) //获取到的按键电平和之前不同
		{
			printf("flag:%d\n",flag);
			setBigButton(); 
		}
		usleep(1000);
	}
	LED_DeInit();
}
