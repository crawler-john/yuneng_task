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
void setBigButton()
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




int readBigButton()
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
/*   1    -  Pressed Button                                                  */
/*   0    -  Not Pressed Button                                              */
/*****************************************************************************/
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
	int flag = 0,prevflag = 0;	//按钮前后对比标志
	int buttonStatus = 0,prevbuttonStatus = 0;	//文件中，按钮对应状态标志
	pthread_t thread_LED = 0;	//按键按下线程ID
	LED_Init();
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
	}
	LED_DeInit();
}
