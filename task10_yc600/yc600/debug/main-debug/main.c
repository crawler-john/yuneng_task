#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>  //open(lcd)
#include <string.h>

#include "variation.h"
#include "3501.h"
#include "access.h"
#include "socket.h"
#include "sqlite3.h"
#include "database.h"
#include "debug.h"
#include "save_historical_data.h"
#include "process_protect_parameters.h"
#include "check_data.h"

char displayip[20]={'\0'};		//液晶屏显示ECU IP Adress
char displayconnect[5]={'\0'};			//液晶屏显示+/-WEB
int displaysyspower=0;			//液晶屏显示当前系统功率
float displayltg=0;			//液晶屏显示历史发电量
int displaycount=0;			//液晶屏显示当前在线数
int displaymaxcount=0;			//液晶屏显示最大逆变器数
int shutdown_control = 0;		//关机标志
int connectflag=0;			//连接EMA的标志
int transfd;				//以太网和GPRS的选择标志
sqlite3 *db=NULL;			//数据库
sqlite3 *tmpdb=NULL;			//临时数据库


int plcmodem;		//PLC的文件描述符
int caltype=0;		//计算方式，NA版和非NA版的区别
unsigned char ccuid[7]={'\0'};		//ECU3501的ID
char ecuid[13]={'\0'};			//ECU的ID
int zoneflag = 0;			//修改时区的标志，改动为1， 没改动为0
char sendcommandtime[3]={'\0'};							//向逆变器发送广播命令时的时间，格式：分秒。
int afdflag = 0;	//逆变器AFD对齐

extern int format(struct inverter_info_t *firstinverter, char *datatime, int syspower, float curgeneration, float ltgeneration);

int initinverter(int plcmodem, sqlite3 *db, char *ccid, struct inverter_info_t *firstinverter)		//初始化每个逆变器
{
	int i, maxcount=0;
	int flag = 0;				//自动上报标志
	FILE *fp;
	struct inverter_info_t *inverter = firstinverter;
	
//	display_scanning();			//液晶屏显示Searching画面
	int lasttime = time(NULL);
	
	for(i=0; i<MAXINVERTERCOUNT; i++){
		memset(inverter->inverterid, '\0', INVERTERIDLEN);		//清空逆变器3501UID
		memset(inverter->tnuid, '\0', TNUIDLENGTH);			//清空逆变器ID
		
		inverter->predv=0;
		inverter->predi=0;
		inverter->preop=0;
		inverter->prenp=0;
		inverter->preit=0;
		inverter->prenv=0;
		inverter->predvb=0;
		inverter->predib=0;
		inverter->preopb=0;

		inverter->lendtimes = 10;		//初始必须大于3，避免系统刚起来又没有数据时借用数据
		
		inverter->flagyc500 = 0;

		inverter->dv=0;			//清空当前一轮直流电压
		inverter->di=0;			//清空当前一轮直流电流
		inverter->op=0;			//清空当前逆变器输出功率
		inverter->np=0;			//清空电网频率
		inverter->it=0;			//清空逆变器温度
		inverter->nv=0;			//清空电网电压
		inverter->dvb=0;			//B路清空当前一轮直流电压
		inverter->dib=0;			//B路清空当前一轮直流电流
		inverter->opb=0;			//B路清空当前逆变器输出功率
		//inverter->npb=0;			//B路清空电网频率
		//inverter->itb=0;			//B路清空逆变器温度
		//inverter->nvb=0;			//B路清空电网电压
		
		inverter->curgeneration = 0;	//清空逆变器当前一轮发电量
		inverter->curgenerationb = 0;	//B路清空逆变器当前一轮发电量

		inverter->preaccgen = 0;
		inverter->preaccgenb = 0;
		inverter->curaccgen = 0;
		inverter->curaccgenb = 0;
		inverter->preacctime = 0;
		inverter->curacctime = 0;
		
		memset(inverter->prestatus, '\0', STATUSLENGTH);
		memset(inverter->status_web, '\0', sizeof(inverter->status_web));		//清空逆变器的状态
		memset(inverter->status, '\0', 12);		//清空逆变器的状态
		memset(inverter->statusb, '\0', 12);		//B路清空逆变器的状态
		
		inverter->flag='1';		//上一轮有数据的标志置位
		inverter->configflag=0;	//配置标志清零
		memset(inverter->lastreporttime, '\0', 3);	//清空最近一次上报时间

		inverter->thistime=0;		//清空上次上报的时间
		inverter->lasttime=lasttime;	//设置初始时间
		//inverter->thistimeb=0;		//B路清空上次上报的时间
		//inverter->lasttimeb=lasttime;	//B路设置初始时间
		
		inverter->processed_protect_flag=0;
		inverter->inverter_with_13_parameters=0;

		inverter++;
	}
	
	get_ecu_type();
	flag = getplcautoflag();		//读取自动上报/手动输入ID的标志
	printdecmsg("flag", flag);
	if(0 == flag){												//手动输入
		maxcount = manualsetid(plcmodem, ccid, firstinverter, db);
	}
	else if(1 == flag){											//自动上报
		turnonautorptid(plcmodem, ccuid, firstinverter,db);
		do{
			maxcount = autorptid(plcmodem, ccid, firstinverter, db);
		}while(0 == maxcount); 
		if(maxcount>0)
		{
			fp=fopen("/etc/yuneng/autoflag.conf","w");					//如果自动上报ID有逆变器ID，就转化成手动输入，ZK
			fputs("0",fp);
			fclose(fp);
		}		
	}
	else
		return -1;
	
	return maxcount;
}

int resetinverter(struct inverter_info_t *curinverter)		//重置每个逆变器
{
	int i;
	
	for(i=0; i<MAXINVERTERCOUNT; i++){
		curinverter->predv = curinverter->dv;
		curinverter->predi = curinverter->di;
		curinverter->preop = curinverter->op;
		curinverter->prenp = curinverter->np;
		curinverter->preit = curinverter->it;
		curinverter->prenv = curinverter->nv;
		curinverter->prestatus[0] = curinverter->status[0];
		curinverter->prestatus[1] = curinverter->status[1];
		curinverter->prestatus[2] = curinverter->status[2];
		
		curinverter->dv=0;
		curinverter->di=0;
		curinverter->op=0;
		curinverter->np=0;
		curinverter->it=0;
		curinverter->nv=0;
		curinverter->curgeneration = 0;
		memset(curinverter->status_web, '\0', sizeof(curinverter->status_web));		//清空逆变器的状态
		memset(curinverter->status, '\0', 12);
		memset(curinverter->statusb, '\0', 12);

		curinverter->dvb=0;
		curinverter->dib=0;
		curinverter->opb=0;
		//curinverter->npb=0;
		//curinverter->itb=0;
		//curinverter->nvb=0;
		curinverter->curgenerationb = 0;
		memset(curinverter->statusb, '\0', 12);
		
		curinverter->status_send_flag = 0;

		curinverter++;
	}
	
	return 0;
}

int disparameters(int connectflag, int systempower, float ltgeneration, int curcount, int maxcount)		//显示参数
{
	FILE *fp;
	char status[256];

	get_ip(displayip);
	
	if(1 == connectflag)
		strcpy(displayconnect, "GPRS");
	else
	{
		fp = fopen("/tmp/connectemaflag.txt", "r");
		if(fp)
		{
			fgets(status, sizeof(status), fp);
			fclose(fp);
			if(!strcmp("connected", status))
				strcpy(displayconnect, "+WEB");
			if(!strcmp("disconnected", status))
				strcpy(displayconnect, "-WEB");
		}
		else
			strcpy(displayconnect, "-WEB");
	}

	displaysyspower = systempower;
	displayltg=ltgeneration;				//液晶屏上显示的历史发电量
	displaycount=curcount;					//液晶屏上显示的当前在线数
	displaymaxcount = maxcount;				//液晶屏上显示的系统最大逆变器数
	if(curcount > 0)
	{
		writesystempower(displaysyspower);			//页面上显示系统当前功率
		writecurrentnumber(curcount);				//页面上显示当前连接数
	}
	write_real_systempower(displaysyspower);
}

//void reset_shutdown(void)					//按钮菜单选中“shut down”后，再选择确定或取消，如果没选择，60秒后自动退出菜单
//{
//    sleep(60);
//    shutdown_control=0;
//}
//
//void button(void)						//按钮功能
//{
//    FILE *fp;
//    int fd,fdlcd;
//    char buff[1];
//    char buflcd[21]={'\0'};
//    struct timeval tpstart,tpend,tptmp;
//    tpstart.tv_sec=0;
//    tpstart.tv_usec=0;
//    tptmp.tv_sec=0;
//    tptmp.tv_usec=0;
//    int timeuse=0,timetmp=0;
//    float thousand=1000,million=1000000;
//
//    int command = 0;
//
//    char temp[10]={'\0'};
//    int system_lp=0;
//    float lifetime_lpower=0;
//
//    pthread_t shutdown_id;
//    pthread_attr_t shutdown_attr;
//    int shutdown_ret;
//
//    get_ip(displayip);
//
//    fd=open("/dev/button", O_RDONLY);
//    fdlcd=open("/dev/lcd", O_WRONLY);
//
//    while(1)
//    {
//      read(fd,buff,1);
//      if(buff[0]=='1')
//      {
//        if(tpstart.tv_sec)
//        {
//          gettimeofday(&tpend,NULL);
//          timeuse=(tpend.tv_sec-tpstart.tv_sec)+(tpend.tv_usec-tpstart.tv_usec)/1000000;
//          switch((timeuse/2-1)%4)
//          {
//            case 0:
//              if(shutdown_control==1)
//              {
//                  printmsg("Cancel");
//                shutdown_control=0;sleep(1);break;
//              }
//              else
//              {
//                  printmsg("Exit Menu");
//                sleep(1);
//                ioctl(fdlcd,0x01,0);
//                break;
//              }
//            case 1:
//              if(shutdown_control==1)
//              {
//                  printmsg("OK");
//		turnoffall();
//                command=2;shutdown_control=0;sleep(1);break;
//              }
//              else
//              {
//                  printmsg("Search Device");
//		//fp=fopen("/etc/yuneng/autoflag.conf","w");					//换成自动上报后，需改变配置文件，ZK
//		//fputs("1",fp);
//		//fclose(fp);
//                exit(0);
//              }
//            case 2:
//              if(shutdown_control==1)
//              {
//                  printmsg("Cancel");
//                shutdown_control=0;sleep(1);break;
//              }
//              else
//              {
//                  printmsg("Status");
//                ioctl(fdlcd,0x01,0);
//                ioctl(fdlcd,0x83,0);
//
//                strcpy(buflcd,"Connected");
//                write(fdlcd,buflcd,strlen(buflcd));
//                ioctl(fdlcd,0x8e,0);
//                if(displaycount<10)
//                  sprintf(temp, "00%d", displaycount);
//                else if(displaycount<100)
//                  sprintf(temp, "0%d", displaycount);
//                else
//                  sprintf(temp, "%d", displaycount);
//                //buflcd=temp;
//                strcpy(buflcd,temp);
//                write(fdlcd,buflcd,strlen(buflcd));
//                ioctl(fdlcd,0xc3,0);
//                //buflcd="Total:";
//                strcpy(buflcd,"Total:");
//                write(fdlcd,buflcd,strlen(buflcd));
//                ioctl(fdlcd,0xce,0);
//                if(displaymaxcount<10)
//                  sprintf(temp, "00%d", displaymaxcount);
//                else if(displaymaxcount<100)
//                  sprintf(temp, "0%d", displaymaxcount);
//                else
//                  sprintf(temp, "%d", displaymaxcount);
//                //buflcd=temp;
//                strcpy(buflcd,temp);
//                write(fdlcd,buflcd,strlen(buflcd));
//                sleep(3);
//                ioctl(fdlcd,0x01,0);
//                break;
//              }
//            case 3:
//              if(shutdown_control==1)
//              {
//                  printmsg("OK");
//		turnoffall();
//                command=2;shutdown_control=0;sleep(1);break;
//              }
//              else
//              {
//                pthread_attr_init(&shutdown_attr);
//                pthread_attr_setdetachstate(&shutdown_attr,PTHREAD_CREATE_DETACHED);
//                shutdown_ret=pthread_create(&shutdown_id,&shutdown_attr,(void *) reset_shutdown,NULL);
//                if(shutdown_ret!=0)
//                {
//                  printmsg("Create pthread error");
//                }
//                  printmsg("Shutdown");
//                shutdown_control=1;
//                //sleep(1);
//                ioctl(fdlcd,0x01,0);
//                break;
//              }
//            default:
//                printmsg("No command");
//              break;
//          }
//          tpstart.tv_sec=0;
//          tpstart.tv_usec=0;
//          tptmp.tv_sec=0;
//          tptmp.tv_usec=0;
//          timetmp=0;
//        }
//        else
//        {
//          if(shutdown_control==1)
//          {
//            ioctl(fdlcd,0x01,0);
//            ioctl(fdlcd,0x80,0);
//            //buflcd="Please choose";
//            strcpy(buflcd,"Please choose");
//            write(fdlcd,buflcd,strlen(buflcd));
//            ioctl(fdlcd,0xc0,0);
//            //buflcd="OK or Cancel!";
//            strcpy(buflcd,"OK or Cancel!");
//            write(fdlcd,buflcd,strlen(buflcd));
//          }
//          else
//          {
//          ioctl(fdlcd,0x01,0);
//          ioctl(fdlcd,0x80,0);
//
//          write(fdlcd,displayip,strlen(displayip));
//          ioctl(fdlcd,0x90,0);
//          strcpy(buflcd, displayconnect);
//          write(fdlcd,buflcd,strlen(buflcd));
//          ioctl(fdlcd,0xc0,0);
//          if(displaysyspower<10000)
//            sprintf(temp, "%dW", displaysyspower);
//          /*else if(displaysyspower<1000000)
//          {
//            system_lp=displaysyspower/thousand;
//            sprintf(temp, "%.1fkW", system_lp);
//          }*/
//          else
//          {
//            system_lp=displaysyspower/thousand;
//            sprintf(temp, "%dKW", system_lp);
//          }
//          //buflcd=temp;
//          strcpy(buflcd,temp);
//          write(fdlcd,buflcd,strlen(buflcd));
//          ioctl(fdlcd,0xc6,0);
//
//          if(displayltg<1000)
//            sprintf(temp, "%.2fkWh", displayltg);
//          else if(displayltg<1000000)
//          {
//            lifetime_lpower=displayltg/thousand;
//            sprintf(temp, "%.2fMWh", lifetime_lpower);
//          }
//          else
//          {
//            lifetime_lpower=displayltg/million;
//            sprintf(temp, "%.2fGWh", lifetime_lpower);
//          }
//          //buflcd=temp;
//          strcpy(buflcd,temp);
//          write(fdlcd,buflcd,strlen(buflcd));
//          if(displaycount==0)
//          {
//            if(displaycount!=displaymaxcount)
//            {
//              ioctl(fdlcd,0xd0,0);
//              sprintf(temp, "000!");
//            }
//            else
//            {
//              ioctl(fdlcd,0xd1,0);
//              sprintf(temp, "000");
//            }
//          }
//          else if(displaycount<10)
//          {
//            if(displaycount!=displaymaxcount)
//            {
//              ioctl(fdlcd,0xd0,0);
//              sprintf(temp, "00%d!", displaycount);
//            }
//            else
//            {
//              ioctl(fdlcd,0xd1,0);
//              sprintf(temp, "00%d", displaycount);
//            }
//          }
//          else if(displaycount<100)
//          {
//            if(displaycount!=displaymaxcount)
//            {
//              ioctl(fdlcd,0xd0,0);
//              sprintf(temp, "0%d!", displaycount);
//            }
//            else
//            {
//              ioctl(fdlcd,0xd1,0);
//              sprintf(temp, "0%d", displaycount);
//            }
//          }
//          else
//          {
//            if(displaycount!=displaymaxcount)
//            {
//              ioctl(fdlcd,0xd0,0);
//              sprintf(temp, "%d!", displaycount);
//            }
//            else
//            {
//              ioctl(fdlcd,0xd1,0);
//              sprintf(temp, "%d", displaycount);
//            }
//          }
//          //buflcd=temp;
//          strcpy(buflcd,temp);
//          write(fdlcd,buflcd,strlen(buflcd));
//          }
//        }
//      }
//      else
//      {
//        if(tpstart.tv_sec==0)
//          gettimeofday(&tpstart,NULL);
//        else
//        {
//          gettimeofday(&tptmp,NULL);
//          timetmp=(1000000*(tptmp.tv_sec-tpstart.tv_sec)+(tptmp.tv_usec-tpstart.tv_usec))/1000000;
//          switch((timetmp/2-1)%4)
//          {
//            case 0:
//              if(shutdown_control==1)
//              {ioctl(fdlcd,0x01,0);
//              ioctl(fdlcd,0x80,0);
//              //buflcd="Cancel";
//              strcpy(buflcd,"Cancel");
//              write(fdlcd,buflcd,strlen(buflcd));break;}
//              else{
//              ioctl(fdlcd,0x01,0);
//              ioctl(fdlcd,0x80,0);
//              //buflcd="Exit Menu";
//              strcpy(buflcd,"Exit Menu");
//              write(fdlcd,buflcd,strlen(buflcd));
//              break;}
//            case 1:
//              if(shutdown_control==1)
//              {ioctl(fdlcd,0x01,0);
//              ioctl(fdlcd,0x80,0);
//              //buflcd="OK";
//              strcpy(buflcd,"OK");
//              write(fdlcd,buflcd,strlen(buflcd));break;}
//              else{
//              ioctl(fdlcd,0x01,0);
//              ioctl(fdlcd,0x80,0);
//              //buflcd="Search Device";
//              strcpy(buflcd,"Search Device");
//              write(fdlcd,buflcd,strlen(buflcd));
//              //buflcd="New device scan";
//              //ioctl(fdlcd,0xc0,0);
//              //write(fdlcd,buflcd,strlen(buflcd));
//              break;}
//            case 2:
//              if(shutdown_control==1)
//              {ioctl(fdlcd,0x01,0);
//              ioctl(fdlcd,0x80,0);
//              //buflcd="Cancel";
//              strcpy(buflcd,"Cancel");
//              write(fdlcd,buflcd,strlen(buflcd));break;}
//              else{
//              ioctl(fdlcd,0x01,0);
//              //buflcd="Status";
//              strcpy(buflcd,"Status");
//              ioctl(fdlcd,0x80,0);
//              write(fdlcd,buflcd,strlen(buflcd));
//              break;}
//            case 3:
//              if(shutdown_control==1)
//              {ioctl(fdlcd,0x01,0);
//              ioctl(fdlcd,0x80,0);
//              //buflcd="OK";
//              strcpy(buflcd,"OK");
//              write(fdlcd,buflcd,strlen(buflcd));break;}
//              else{
//              ioctl(fdlcd,0x01,0);
//              //buflcd="Shutdown";
//              strcpy(buflcd,"Turn off all");
//              ioctl(fdlcd,0x80,0);
//              write(fdlcd,buflcd,strlen(buflcd));
//		strcpy(buflcd,"inverters");
//              ioctl(fdlcd,0xC0,0);
//              write(fdlcd,buflcd,strlen(buflcd));
//              break;}
//            default:
//              break;
//          }
//        }
//      }
//      usleep(10000);
//    }
//}
//
//void button_pthread(void)			//开启按键菜单功能（即开启按键的独立线程）
//{
//    pthread_t button_id;
//    int button_ret=0;
//
//    button_ret=pthread_create(&button_id,NULL,(void *) button,NULL);
//    if(button_ret!=0)
//    {
//      printmsg("Create pthread error");
//    }
//}*/

void afd_set(void)
{
	while(1){
		afdflag = 1;
		sleep(1800);
	}
}

void afd_pthread(void)			//开启按键菜单功能（即开启按键的独立线程）
{
    pthread_t afd_id;
    int afd_ret=0;

    afd_ret=pthread_create(&afd_id, NULL, (void *) afd_set, NULL);

    if(afd_ret!=0)
      printmsg("Create afd thread error");
}

/*void socket_connect(void)			//连接EMA，直到连接上为止
{
    int ret;
    //struct arguments *args;
    //args = (struct arguments *)arg;

    do
    {
        printmsg("in socket_connect");
      sleep(60);
      ret=connect_socket(transfd);
    }
    while(ret==-1);

    connectflag = 1;				//连接上EMA后，标志置‘1’
    //write_connect_time();
	resendrecord(db);
}

void connect_process(void)			//开启连接EMA的线程
{
    pthread_t connect_id;
    pthread_attr_t connect_attr;
    int connect_ret;

    //struct arguments arg;
    //arg.database = db;
    //arg.socket = socketfd;
    //close_socket(main_socket);
    //main_socket=createsocket();
    pthread_attr_init(&connect_attr);
    pthread_attr_setdetachstate(&connect_attr,PTHREAD_CREATE_DETACHED);
    connect_ret=pthread_create(&connect_id, &connect_attr, (void *)socket_connect, NULL);
}*/

/*int transsyscurgen(char *buff, float curgen)		//增加系统当前一轮发电量
{
	int i, temp=0;
	char curgentemp[10]={'\0'};

	temp = (int)(curgen*1000000.0);
	sprintf(curgentemp, "%d", temp);
	for(i=0; i<CURSYSGENLEN-strlen(curgentemp); i++)
		strcat(buff, "A");
	strcat(buff, curgentemp);

	return 0;
}*/

float calsystemgeneration(struct inverter_info_t *inverter)		//计算当前一轮的系统发电量
{
	int i;
	float temp=0.0;
	
	for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(inverter->inverterid)); i++){
		if('1'==inverter->flag)
			temp = temp + inverter->curgeneration + inverter->curgenerationb;
		inverter++;
	}

	printfloatmsg("temp", temp);
	
	return temp;
}

/*int transltgen(char *buff, float ltgen)		//增加历史发电量
{
	int i;
	char ltgentemp[10]={'\0'};
	int temp = (int)ltgen;

	sprintf(ltgentemp, "%d", temp);
	for(i=0; i<LIFETIMEGENLEN-strlen(ltgentemp); i++)
		strcat(buff, "A");
	strcat(buff, ltgentemp);

	return 0;
}

int transdv(char *buff, float dv)				//增加逆变器的直流电压
{
	int i;
	char dvtemp[10]={'\0'};
	int temp = (int)dv;
	
	sprintf(dvtemp, "%d", temp);
	for(i=0; i<DVLENGTH-strlen(dvtemp); i++)		//直流电压占5个字节，与EMA协议规定
		strcat(buff, "A");
	strcat(buff, dvtemp);
	
	return 0;
}

int transdi(char *buff, float di)				//增加逆变器的直流电流
{
	int i;
	char ditemp[10]={'\0'};
	int temp = (int)di;

	sprintf(ditemp, "%d", temp);
	for(i=0; i<DILENGTH-strlen(ditemp); i++)
		strcat(buff, "A");
	strcat(buff, ditemp);
		
	return 0;
}

int transpower(char *buff, int power)			//增加逆变器的功率
{
	int i;
	char powertemp[10]={'\0'};

	sprintf(powertemp, "%d", power*100);
	for(i=0; i<POWERLENGTH-strlen(powertemp); i++)
		strcat(buff, "A");
	strcat(buff, powertemp);
		
	return 0;
}

int transfrequency(char *buff, float frequency)			//增加逆变器的电网频率
{
	int i;
	char fretemp[10]={'\0'};
	int temp = (int)frequency;

	sprintf(fretemp, "%d", temp);
	for(i=0; i<FREQUENCYLENGTH-strlen(fretemp); i++)
		strcat(buff, "A");
	strcat(buff, fretemp);
	
	return 0;
}

int transtemperature(char *buff, int temperature)			//增加逆变器的温度
{
	int i;
	char tempertemp[10]={'\0'};

	if(temperature<0)
		sprintf(tempertemp, "%d", temperature*-1);
	else
		sprintf(tempertemp, "%d", temperature);

	if(temperature<=-10)
		strcat(buff, "B");
	else if(temperature<0)
		strcat(buff, "AB");
	else if(temperature<10)
		strcat(buff, "AA");
	else if(temperature<100)
		strcat(buff, "A");
	else
		;

	strcat(buff, tempertemp);
		
	return 0;
}

int transgridvolt(char *buff, int voltage)			//增加逆变器的电网电压
{
	int i;
	char gvtemp[10]={'\0'};

	sprintf(gvtemp, "%d", voltage);
	for(i=0; i<GRIDVOLTLENGTH-strlen(gvtemp); i++)
		strcat(buff, "A");
	strcat(buff, gvtemp);

	return 0;
}*/

//int transstatus(char *buff, char *status)		//增加逆变器的状态
//{
//	unsigned char cmp = 0x10;
//	int i;
//
//	for(i=0; i<11; i++){
//		if('1' == status[i])
//			strcat(buff, "1");
//		else
//			strcat(buff, "0");
//		//cmp = cmp>>1;				//右移一位
//	}
//
//	/*cmp = 0x10;
//	for(i=0; i<3; i++){
//		if(1==(status[1]&cmp))
//			strcat(buff, "1");
//		else
//			strcat(buff, "0");
//		cmp = cmp>>1;				//右移一位
//	}*/
//
//	return 0;
//}

/*int transcurgen(char *buff, float gen)		//增加逆变器的当前一轮发电量
{
	int i;
	char gentemp[10]={'\0'};
	int temp = (int)gen;

	sprintf(gentemp, "%d", temp);
	for(i=0; i<CURGENLENGTH-strlen(gentemp); i++)
		strcat(buff, "A");
	strcat(buff, gentemp);

	return 0;
}

int transsyspower(char *buff, int syspower)			//增加系统功率
{
	int i;
	char syspowertemp[10]={'\0'};

	sprintf(syspowertemp, "%d", syspower*100);
	for(i=0; i<SYSTEMPOWERLEN-strlen(syspowertemp); i++)
		strcat(buff, "A");
	strcat(buff, syspowertemp);

	return 0;
}*/

//把所有逆变器的数据按照ECU和EMA的通信协议转换，见协议
/*int protocal(struct inverter_info_t *firstinverter, char *id, char *buff, char *sendcommanddatetime, int syspower, float curgeneration, float ltgeneration)
{
	int i;
	char temp[50] = {'\0'};
	struct inverter_info_t *inverter = firstinverter;//printf("protocal1\n");

	strcat(buff, "APS1300000AAAAAAA1");
	strcat(buff, id);//printf("protocal3 ");
	transsyspower(buff, syspower);//printf("protocal4 ");
	transsyscurgen(buff, curgeneration);//printf("protocal5 ");
	transltgen(buff, ltgeneration*10.0);//printf("protocal6 ");
	strcat(buff, sendcommanddatetime);//printf("protocal7\n");

	sprintf(temp, "%d", displaycount);
	for(i=0; i<(3-strlen(temp)); i++)
		strcat(buff, "A");
	strcat(buff, temp);

	if(!zoneflag)
		strcat(buff, "0");
	else
		strcat(buff, "1");
	strcat(buff, "00000END");

	for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(inverter->inverterid)); i++){
		if('1' == inverter->flag){
			if(1 == inverter->flagyc500){
				strcat(buff, inverter->inverterid);//printf("protocal11");
				strcat(buff, "A");
				transdv(buff, inverter->dv);//printf("protocal12");
				transdi(buff, inverter->di);//printf("protocal13");
				transpower(buff, inverter->op);//printf("protocal14");
				transfrequency(buff, inverter->np*10.0);//printf("protocal15");
				transtemperature(buff, inverter->it);//printf("protocal16");
				transgridvolt(buff, inverter->nv);//printf("protocal17");
				transstatus(buff, inverter->status);//printf("protocal18");
				transcurgen(buff, inverter->curgeneration*1000000.0);//printf("protocal19\n");
				strcat(buff, "END");

				strcat(buff, inverter->inverterid);//printf("protocal11");
				strcat(buff, "B");
				transdv(buff, inverter->dvb);//printf("protocal12");
				transdi(buff, inverter->dib);//printf("protocal13");
				transpower(buff, inverter->opb);//printf("protocal14");
				transfrequency(buff, inverter->np*10.0);//printf("protocal15");
				transtemperature(buff, inverter->it);//printf("protocal16");
				transgridvolt(buff, inverter->nv);//printf("protocal17");
				transstatus(buff, inverter->statusb);//printf("protocal18");
				transcurgen(buff, inverter->curgenerationb*1000000.0);//printf("protocal19\n");
				strcat(buff, "END");
			}
			else{
				strcat(buff, inverter->inverterid);//printf("protocal11");
				strcat(buff, "0");
				transdv(buff, inverter->dv);//printf("protocal12");
				transdi(buff, inverter->di);//printf("protocal13");
				transpower(buff, inverter->op);//printf("protocal14");
				transfrequency(buff, inverter->np*10.0);//printf("protocal15");
				transtemperature(buff, inverter->it);//printf("protocal16");
				transgridvolt(buff, inverter->nv);//printf("protocal17");
				transstatus(buff, inverter->status);//printf("protocal18");
				transcurgen(buff, inverter->curgeneration*1000000.0);//printf("protocal19\n");
				strcat(buff, "END");
			}
		}
		inverter++;
	}//printf("protocal2 ");
	memset(temp, '\0', 50);
	sprintf(temp, "%d", strlen(buff));
	for(i=0; i<(5-strlen(temp)); i++)
		buff[5+i] =  'A';
	for(i=0; i<strlen(temp); i++)
		buff[5+5-strlen(temp)+i] = temp[i];

	strcat(buff, "\n");

	printmsg(buff);

	return 0;
}*/

int protocal(struct inverter_info_t *firstinverter, char *id, char *buff, char *sendcommanddatetime, int syspower, float curgeneration, float ltgeneration)		
{
	int i;
	char temp[256] = {'\0'};
	struct inverter_info_t *inverter = firstinverter;

	sprintf(temp, "APS150000000010001%s%010d%010d%010d%s%03d", id, syspower, (int)(curgeneration*1000000.0),
			(int)(ltgeneration*10.0), sendcommanddatetime, displaycount);
	strcat(buff, temp);

	if(!zoneflag)
		strcat(buff, "000000END");
	else
		strcat(buff, "100000END");

	for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(inverter->inverterid)); i++){
		if('1' == inverter->flag){
			memset(temp, '\0', sizeof(temp));
			if(1 == inverter->flagyc500){
				sprintf(temp, "%sA%05d%03d%05d%05d%03d%03d%010dEND", inverter->inverterid, (int)(inverter->dv), (int)(inverter->di),
						inverter->op*100, (int)(inverter->np*10.0), inverter->it+100, inverter->nv, (int)(inverter->curgeneration*1000000.0));
				strcat(buff, temp);

				memset(temp, '\0', sizeof(temp));
				sprintf(temp, "%sB%05d%03d%05d%05d%03d%03d%010dEND", inverter->inverterid, (int)(inverter->dvb), (int)(inverter->dib),
						inverter->opb*100, (int)(inverter->np*10.0), inverter->it+100, inverter->nv, (int)(inverter->curgenerationb*1000000.0));

				strcat(buff, temp);
			}
			else{
				sprintf(temp, "%s0%05d%03d%05d%05d%03d%03d%010dEND", inverter->inverterid, (int)(inverter->dv), (int)(inverter->di),
						inverter->op*100, (int)(inverter->np*10.0), inverter->it+100, inverter->nv, (int)(inverter->curgeneration*1000000.0));

				strcat(buff, temp);
			}
		}
		inverter++;
	}

	memset(temp, '\0', sizeof(temp));
	sprintf(temp, "%05d", strlen(buff));
	for(i=0; i<strlen(temp); i++)
		buff[5+i] = temp[i];
	
	strcat(buff, "\n");

	printmsg(buff);
	
	return 0;
}

int protocol_status(struct inverter_info_t *firstinverter, char *datetime)
{
	int i, count=0;
	struct inverter_info_t *inverter = firstinverter;
	char sendbuff[65535]={'\0'};
	char temp[65535]={'\0'};
	char length[16]={'\0'};

	for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(inverter->inverterid)); i++, inverter++){
		if((1 == inverter->status_send_flag) && ('1' == inverter->flag)){
			strcat(temp, inverter->status_ema);
			count++;
		}
	}

	if(count > 0){
		sprintf(sendbuff, "APS1500000A123AAA1%s%04d%sEND%s", ecuid, count, datetime, temp);
		sprintf(length, "%05d", strlen(sendbuff));
		for(i=5; i<10; i++)
			sendbuff[i] = length[i-5];
		strcat(sendbuff, "\n");
		save_status(sendbuff, datetime);
	}
}

int main(int argc, char *argv[])
{
	
	int curcount=0, maxcount=0;		//curcount:当前通信的逆变器数；maxcount:用户安装的逆变器数
	int systempower=0;	//系统功率
	int transflag; 		//int transfd, transflag;
	int configdomainflag=0;			//配置逆变器的标志位
	char sendpresetflag='0';		//ECU发送预设值给逆变器的标志位
	int thistime=0, durabletime=0;		//thistime：本轮向逆变器发送广播要数据的时间点；durabletime：ECU本轮向逆变器要数据的持续时间
	int i;			//控制for循环
	int res = 0;
	int fd_reset;
	
	float ltgeneration=0;			//系统历史总发电量
	float cursysgeneration=0;			//系统当前一轮发电量


	char sendbuff[MAXINVERTERCOUNT*RECORDLENGTH+RECORDTAIL]={'\0'};		//ECU把数据按照与EMA的协议转换后，放入sendbuff数组中
	char resendflag='1';			//ECU重发数据给EMA的标志位
	////char presetdata[20]={0};		//保存ECU发送给逆变器的预设值（在参数配置页面上输入的参数经转换）
	struct inverter_info_t inverter[MAXINVERTERCOUNT];		//最多MAXINVERTERCOUNT个逆变器
	//char sendcommandtime[3]={'\0'};							//向逆变器发送广播命令时的时间，格式：分秒。
	char sendcommanddatetime[20]={'\0'};					//发送给EMA时的日期和时间，格式：年月日时分秒
	//sqlite3 *db=NULL;
	int reportinterval=300;
	
	printf("********************************************************************\n");
	printf("*main                                                              *\n");
	printf("********************************************************************\n");
	
	initialize();		//初始化设置
	db=database_init();	//初始化数据库
	historical_database_init();
	record_db_init();
	
	//char data[6] = {0x01, 0x02, 0x03, 0x04, 0x05, '\0'};
	fd_reset = open_reset();
	close_reset(fd_reset);
	getecuid(ecuid);	//获取ECU的ID
	zoneflag = get_timezone();
	
	transflag = gettransflag();	//获取发送标志，判断是以太网还是GPRS
	/*if(1==transflag){			//GPRS
		transfd = opengprs();
		gprs_resendrecord(db, transfd);		//先发送数据库中保存的以前没发送至EMA的记录（GPRS方式）
	}
	/*else{
		transfd = createsocket();
		connectflag = connect_socket(transfd);	//连接EMA
		if(-1 == connectflag)			//如果没连接上，则启动连接线程，不停地连接EMA，直到连接成功
			connect_process();
		else{
			resendrecord(db);	//先发送数据库中保存的以前没发送至EMA的记录（以太网方式）
		}
	}*/

	printdecmsg("transflag", transflag);

	plcmodem = openplc();		//打开PLC串口
	while(-1==getccuid(plcmodem, ccuid))		//不停地读取ECU的3501的UID，直到读到为止
		sleep(1);
	//sendalamcmd(plcmodem, ccuid, tnuid, 0xd0);
	
	maxcount = initinverter(plcmodem, db, ccuid, inverter);		//初始化每个逆变器
	ltgeneration=get_lifetime_power(db);				//从数据库中读取系统历史发电量
	disparameters(transflag, systempower, ltgeneration, curcount, maxcount);		//显示各个参数
	show_data_on_lcd(systempower, ltgeneration, curcount, maxcount);
	
	read_gfdi_turn_on_off_status(inverter);
	tmpdb = create_tmpdb();
	init_tmpdb(tmpdb, inverter);

	if(maxcount<=100)				//如果轮询一遍的时间不到5分钟，那么一直等到5分钟再轮询下一遍，超过5分钟则等到10分钟。。。5分钟起跳
		reportinterval = 300;
	else if(maxcount<=200)
		reportinterval = 600;
	else if(maxcount<=300)
		reportinterval = 900;
	else
		reportinterval = 1200;
	
	//button_pthread();				//开启菜单按键线程，按键开始工作
	turn_on_serial();
	system("killall button");
	system("/home/applications/button &");
	//afd_pthread();
	
	caltype = get_version();			//获取版本号，判断是否NA版来做不同的数据计算
	
	thistime = time(NULL);			//得到本次轮询开始的时间点
	durabletime = thistime+65535;		//确保第一次可以轮询；
	while(1){
		if((durabletime-thistime)>=reportinterval){
			thistime = time(NULL);
		printmsg("------------------------------------------------------------------->");

		memset(sendcommanddatetime, '\0', 20);	//printf("point1 ");		//清空本次广播的时间

		get_time(sendcommanddatetime, sendcommandtime);	//printf("point2 ");	//重新获取本次广播的时间

		test_signal_strength(plcmodem, ccuid);		//检测电网信号强度，ZK

		curcount = getalldata(inverter, sendcommandtime);//printf("point3 ");		//获取所有逆变器数据，返回当前有数据的逆变器的数量

		systempower = calsystempower(inverter);	//printf("point4 ");		//计算系统当前一轮的功率

		cursysgeneration = calsystemgeneration(inverter);//printf("point5 ");	//计算系统当前一轮的发电量

		ltgeneration = ltgeneration + cursysgeneration;//printf("point6 ");		//计算系统历史发电量

		set_lifetime_power(db,ltgeneration);//printf("point7\n");			//设置系统历史发电量

		todaypower(db, cursysgeneration);			//设置系统当天的发电量

		printdecmsg("systempower", systempower);
		printfloatmsg("cursysgeneration", cursysgeneration);
		printfloatmsg("ltgeneration", ltgeneration);
		printdecmsg("curcount", curcount);

		disparameters(transflag, systempower, ltgeneration, curcount, maxcount);
		protocal(inverter, ecuid, sendbuff, sendcommanddatetime, systempower, cursysgeneration, ltgeneration);//printf("point81\n");		//把数据转换为协议
		protocol_status(inverter, sendcommanddatetime);

			/*if(curcount>0){//if(strlen(sendbuff)>0){			//如果当前有逆变器上传数据，那么ECU发送记录给EMA
				if(1==transflag){		//GPRS发送
					if(1 != gprssendrecord(transfd, sendbuff))	//如果发送失败，标记，以便下次重新发送
						resendflag = '1';
					else
						resendflag = '0';
				}
				/*else{				//以太网发送
					if(1==connectflag){				//如果连接标志为1，说明ECU与EMA已连接，可以发送送数据
						res = send_record(transfd, sendbuff);
						if(-1 == res){
							connectflag=-1;
							close_socket(transfd);
							transfd=createsocket();
							connect_process();
							resendflag = '1';
						}
						else if(0 == res)
							resendflag = '1';
						else
							resendflag = '0';
					}
					else						//说明ECU和EMA没有连接上，直接设置重发标志
						resendflag = '1';
				}*/
			//}

		if(curcount>0){
			save_record(sendbuff, sendcommanddatetime);//printf("point9 ");			//把发送给EMA的记录保存在数据库中
			saveevent(db, inverter, sendcommanddatetime);//printf("point10 ");		//保存当前一轮逆变器的事件

			save_system_power(systempower,sendcommanddatetime);
			update_daily_energy(cursysgeneration,sendcommanddatetime);
			update_monthly_energy(cursysgeneration,sendcommanddatetime);
			update_yearly_energy(cursysgeneration,sendcommanddatetime);
			update_lifetime_energy(ltgeneration);
		}

			//}
		//}//printf("point11 ");		//printf("test point!\n");
		disparameters(transflag, systempower, ltgeneration, curcount, maxcount);//printf("point12 ");		//液晶屏上的参数
		show_data_on_lcd(systempower, ltgeneration, curcount, maxcount);
		sleep(1);

		if(curcount > 0)
			displayonweb(inverter, sendcommanddatetime);//printf("point13 ");						//实时数据页面的数据

		if(curcount > 0){
			process_connect(inverter);
			set_protection_parameters(inverter);
			read_max_power(inverter);
			//process_max_power_all(inverter);
			process_grid_environment_all(inverter);
			process_ird_all(inverter);
			process_restore_inverter(inverter);
			get_inverter_version_all(inverter);
		}
		processpower(inverter);
		set_protection_parameters_inverter(inverter);
		read_reference_protect_parameters(inverter);
		turn_on_off(inverter);
		clear_gfdi(inverter);
		process_grid_environment(inverter);
		process_ird(inverter);
		process_protect_parameters(inverter);
		get_all_signal_strength(inverter);
		get_inverter_version_single();
		update_inverter();

		format(inverter, sendcommanddatetime, systempower, cursysgeneration, ltgeneration);
		turnoffautorptid(plcmodem, ccuid, inverter);			//发送禁止逆变器上报ID的命令，ZK

		resetinverter(inverter);//printf("point14\n");								//重置每个逆变器
		memset(sendbuff, '\0', MAXINVERTERCOUNT*RECORDLENGTH+RECORDTAIL);			//清空发送缓冲sendbuff*/
		
		if(0 == maxcount)					//如果系统中没有逆变器上传数据，等待1s
			sleep(1);}
		else
			sleep(1);

		if(curcount > 0)
			process_all(inverter);

		durabletime = time(NULL);				//如果轮询一遍的时间不到5分钟，那么一直等到5分钟再轮询下一遍，超过5分钟则等到10分钟。。。5分钟起跳
//		if((durabletime-thistime)<=300)
//			reportinterval = 300;
//		else if((durabletime-thistime)<=600)
//			reportinterval = 600;
//		else if((durabletime-thistime)<=900)
//			reportinterval = 900;
//		else
//			reportinterval = 1200;
		if(0 != (durabletime-thistime)%300)
			reportinterval = ((durabletime-thistime)/300 + 1)*300;
		else
			reportinterval = durabletime-thistime;
	}

	return 0;
}
