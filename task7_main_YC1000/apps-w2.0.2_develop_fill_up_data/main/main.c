#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "variation.h"
#include "database.h"
#include "save_historical_data.h"
#include "fill_up_data.h"

#define MAIN_VERSION "W2.0.0"
#define POLLING_INTERVAL 300
ecu_info ecu;

int init_ecu()
{
	FILE *fp;
	char buff[50] = {'\0'};
	char temp[50] = {'\0'};

	fp = fopen("/etc/yuneng/ecuid.conf", "r");
	if (fp) {
		fgets(ecu.id, 13, fp);
		fclose(fp);
	}

	fp = fopen("/etc/yuneng/ecu_eth0_mac.conf", "r");
	if (fp) {
		memset(buff, '\0', sizeof(buff));
		fgets(buff, 18, fp);
		fclose(fp);
		if((buff[12]>='0') && (buff[12]<='9'))
			buff[12] -= 0x30;
		if((buff[12]>='A') && (buff[12]<='F'))
			buff[12] -= 0x37;
		if((buff[13]>='0') && (buff[13]<='9'))
			buff[13] -= 0x30;
		if((buff[13]>='A') && (buff[13]<='F'))
			buff[13] -= 0x37;
		if((buff[15]>='0') && (buff[15]<='9'))
			buff[15] -= 0x30;
		if((buff[15]>='A') && (buff[15]<='F'))
			buff[15] -= 0x37;
		if((buff[16]>='0') && (buff[16]<='9'))
			buff[16] -= 0x30;
		if((buff[16]>='A') && (buff[16]<='F'))
			buff[16] -= 0x37;
		ecu.panid = ((buff[12]) * 16 + (buff[13])) * 256 + (buff[15]) * 16 + (buff[16]);
	}

	//获取ECU信道
	ecu.channel = 0x10;
	fp = fopen("/etc/yuneng/channel.conf", "r");
	if (fp) {
		memset(buff, '\0', sizeof(buff));
		fgets(buff, 5, fp);
		fclose(fp);
		if((buff[2]>='0') && (buff[2]<='9'))
			buff[2] -= 0x30;
		if((buff[2]>='A') && (buff[2]<='F'))
			buff[2] -= 0x37;
		if((buff[2]>='a') && (buff[2]<='f'))
			buff[2] -= 0x57;
		if((buff[3]>='0') && (buff[3]<='9'))
			buff[3] -= 0x30;
		if((buff[3]>='A') && (buff[3]<='F'))
			buff[3] -= 0x37;
		if((buff[3]>='a') && (buff[3]<='f'))
			buff[3] -= 0x57;
		ecu.channel = buff[2]*16+buff[3];
	} else {
		fp = fopen("/etc/yuneng/channel.conf", "w");
		if (fp) {
			fputs("0x10", fp);
			fclose(fp);
		}
	}

	memset(ecu.ip, '\0', sizeof(ecu.ip));
	ecu.life_energy = get_lifetime_power();
	ecu.current_energy = 0;
	ecu.system_power = 0;
	ecu.count = 0;
	ecu.total = 0;
	ecu.flag_ten_clock_getshortaddr = 1;		//ZK
	ecu.polling_total_times=0;					//ECU一天之中总的轮询次数清0， ZK

	fp = fopen("/etc/yuneng/version.conf", "r");
	if (fp) {
		memset(buff, '\0', sizeof(buff));
		memset(temp, '\0', sizeof(temp));
		fgets(buff, 50, fp);
		fclose(fp);
		if('\n' == buff[strlen(buff)-1])
			buff[strlen(buff)-1] = '\0';
		strcpy(temp, &buff[strlen(buff)-2]);
		if((0 == strcmp(temp, "NA")) || (0 == strcmp(temp, "MX")))			//如果是NA或者MX返回1，否则返回0；
			ecu.type = 1;
		else
			ecu.type = 0;
	}

	fp = fopen("/etc/yuneng/timezone.conf", "r");
	if (fp) {
		memset(buff, '\0', sizeof(buff));
		fgets(buff, 50, fp);
		fclose(fp);

		if(!strncmp(buff, "Asia/Shanghai", 13))
			ecu.zoneflag = 0;
		else
			ecu.zoneflag = 1;
	}

	printecuinfo(&ecu);

	return 1;
}

int init_inverter(inverter_info *inverter)
{
	int i;
	char flag_autorpt = '0';				//自动上报标志
	char flag_limitedid = '0';				//限定ID标志

	FILE *fp;
	inverter_info *curinverter = inverter;

//	display_scanning();			//液晶屏显示Searching画面

	for(i=0; i<MAXINVERTERCOUNT; i++, curinverter++)
	{
		memset(curinverter->id, '\0', sizeof(curinverter->id));		//清空逆变器3501UID
		memset(curinverter->tnuid, '\0', sizeof(curinverter->tnuid));			//清空逆变器ID

		curinverter->model = 0;

		curinverter->dv=0;			//清空当前一轮直流电压
		curinverter->di=0;			//清空当前一轮直流电流
		curinverter->op=0;			//清空当前逆变器输出功率
		curinverter->gf=0;			//清空电网频率
		curinverter->it=0;			//清空逆变器温度
		curinverter->gv=0;			//清空电网电压
		curinverter->dvb=0;			//B路清空当前一轮直流电压
		curinverter->dib=0;			//B路清空当前一轮直流电流
		curinverter->opb=0;			//B路清空当前逆变器输出功率
		curinverter->gvb=0;
		curinverter->dvc=0;
		curinverter->dic=0;
		curinverter->opc=0;
		curinverter->gvc=0;
		curinverter->dvd=0;
		curinverter->did=0;
		curinverter->opd=0;
		curinverter->gvd=0;



		curinverter->curgeneration = 0;	//清空逆变器当前一轮发电量
		curinverter->curgenerationb = 0;	//B路清空逆变器当前一轮发电量

		curinverter->preaccgen = 0;
		curinverter->preaccgenb = 0;
		curinverter->curaccgen = 0;
		curinverter->curaccgenb = 0;
		curinverter->preacctime = 0;
		curinverter->curacctime = 0;

		memset(curinverter->status_web, '\0', sizeof(curinverter->status_web));		//清空逆变器的状态
		memset(curinverter->status, '\0', sizeof(curinverter->status));		//清空逆变器的状态
		memset(curinverter->statusb, '\0', sizeof(curinverter->statusb));		//B路清空逆变器的状态

		curinverter->dataflag = 0;		//上一轮有数据的标志置位
	//	curinverter->bindflag=0;		//绑定逆变器标志位置清0
		curinverter->no_getdata_num=0;	//ZK,清空连续获取不到数据的次数
		curinverter->disconnect_times=0;		//没有与逆变器通信上的次数清0， ZK
		curinverter->signalstrength=0;			//信号强度初始化为0
		curinverter->fill_up_data_flag=0;      //补数据标志位
	}

	get_ecu_type();		//获取ECU型号
	fp = fopen("/etc/yuneng/limitedid.conf", "r");
	if(fp)
	{
		flag_limitedid = fgetc(fp);
		fclose(fp);
	}

	if ('1' == flag_limitedid) {
		while(1) {
			bind_inverters(); //绑定逆变器
			ecu.total = get_id_from_db(inverter); //获取逆变器数量
			if (ecu.total > 0) {
				break; //直到逆变器数量大于0时退出循环
			} else {
				display_input_id(); //提示用户输入逆变器ID
				sleep(5);
			}
		}
		fp = fopen("/etc/yuneng/limitedid.conf", "w");
		if (fp) {
			fputs("0", fp);
			fclose(fp);
		}
	}
	else {
		while(1) {
			ecu.total = get_id_from_db(inverter);
			if (ecu.total > 0) {
				break; //直到逆变器数量大于0时退出循环
			} else {
				display_input_id(); //提示用户输入逆变器ID
				sleep(5);
			}
		}
	}

	return 1;
}

int init_all(inverter_info *inverter)
{
	openzigbee();
	init_database();
	init_ecu();
	init_inverter(inverter);
	init_tmpdb(inverter);
	init_lost_data_info();	//初始化补数据结构体

	read_gfdi_turn_on_off_status(inverter);
	system("killall button");
	system("/home/applications/button &");
	display_on_lcd_and_web(); //液晶屏上的参数

	return 0;
}

int reset_inverter(inverter_info *inverter)
{
	int i;
	inverter_info *curinverter = inverter;

	for(i=0; i<MAXINVERTERCOUNT; i++, curinverter++)
	{
		curinverter->dataflag = 0;

		curinverter->dv=0;
		curinverter->di=0;
		curinverter->op=0;
		curinverter->gf=0;
		curinverter->it=0;
		curinverter->gv=0;

		curinverter->dvb=0;
		curinverter->dib=0;
		curinverter->opb=0;

		curinverter->curgeneration = 0;
		curinverter->curgenerationb = 0;
		curinverter->status_send_flag=0;

		memset(curinverter->status_web, '\0', sizeof(curinverter->status_web));		//清空逆变器的状态
		memset(curinverter->status, '\0', sizeof(curinverter->status));
		memset(curinverter->statusb, '\0', sizeof(curinverter->statusb));
	}

	return 1;
}

int main(int argc, char *argv[])
{
	int thistime=0, durabletime=65535, reportinterval=POLLING_INTERVAL;		//thistime：本轮向逆变器发送广播要数据的时间点；durabletime：ECU本轮向逆变器要数据的持续时间
	char broadcast_hour_minute[3]={'\0'};									//向逆变器发送广播命令时的时间
	int time_linux;
	inverter_info inverter[MAXINVERTERCOUNT];

	printf("\nmain.exe %s\n", MAIN_VERSION);
	printmsg("Start-------------------------------------------------");

	init_all(inverter);

	while(1){
		if((durabletime-thistime) >= reportinterval){
			thistime = time(NULL);

			memset(ecu.broadcast_time, '\0', sizeof(ecu.broadcast_time));				//清空本次广播的时间

			time_linux = get_time(ecu.broadcast_time, broadcast_hour_minute);					//重新获取本次广播的时间

			printmsg("****************************************");
			print2msg("ecu.broadcast_time",ecu.broadcast_time);


			ecu.count = getalldata(inverter,time_linux);			//获取所有逆变器数据，返回当前有数据的逆变器的数量
			save_time_to_database(inverter, time_linux);//YC1000补报：将补报时间戳保存到数据库


			ecu.life_energy = ecu.life_energy + ecu.current_energy;				//计算系统历史发电量


			update_life_energy(ecu.life_energy);								//设置系统历史发电量

			update_today_energy(ecu.current_energy);							//设置系统当天的发电量


			if(ecu.count>0)
			{
				save_system_power(ecu.system_power,ecu.broadcast_time);			//ZK
				update_daily_energy(ecu.current_energy,ecu.broadcast_time);
				update_monthly_energy(ecu.current_energy,ecu.broadcast_time);
				update_yearly_energy(ecu.current_energy,ecu.broadcast_time);
				update_lifetime_energy(ecu.life_energy);
			}
			display_on_lcd_and_web(); //液晶屏显示信息

			if(ecu.count>0)
			{
				protocol(inverter, ecu.broadcast_time);
				protocol_status(inverter, ecu.broadcast_time);
				saveevent(inverter, ecu.broadcast_time);							//保存当前一轮逆变器的事件
			}

			if(ecu.count>0)
			{
				displayonweb(inverter, ecu.broadcast_time);								//实时数据页面的数据
			}
//			printinverterinfo(&inverter);										//打印逆变器解析信息，ZK
//			format(inverter, ecu.broadcast_time, ecu.system_power, ecu.current_energy, ecu.life_energy);

			reset_inverter(inverter);											//重置每个逆变器
			remote_update(inverter);

			//对于轮询没有数据的逆变器进行重新获取短地址操作
			bind_nodata_inverter(inverter);
		}

		process_all(inverter);
		sleep(1);

		durabletime = time(NULL);				//如果轮询一遍的时间不到5分钟，那么一直等到5分钟再轮询下一遍，超过5分钟则等到10分钟。。。5分钟起跳
		if(0 != (durabletime-thistime)%POLLING_INTERVAL)
			reportinterval = ((durabletime-thistime)/POLLING_INTERVAL + 1)*POLLING_INTERVAL;
		else
			reportinterval = durabletime-thistime;

		durabletime = fill_up_data(inverter,(reportinterval+POLLING_INTERVAL+thistime),thistime);
	}
	return 0;
}
