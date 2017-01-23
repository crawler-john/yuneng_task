/*
 * wifi_init.c
 * WiFi开机初始化程序
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//获取ECU_id最后4位
int get_ecuid(void)
{
	FILE *fp;
	char ecuid[13] = {'\0'};

	if (NULL == (fp = fopen("/etc/yuneng/ecuid.conf", "r")))
	{
		perror("ecuid.conf");
		return -1;
	}
	fgets(ecuid, 13, fp);
	fclose(fp);
	fp = NULL;
	return atoi(ecuid+8);
}

//检测有线网卡eth0状态(1:连接，0：断开)
int check_eth0(void)
{
	FILE *fp;
	char buff[2] = {'\0'};

	if(NULL != (fp = fopen("/sys/class/net/eth0/carrier", "r")))
	{
		fgets(buff, 2, fp);
		fclose(fp);
		return atoi(buff);
	}
	else
		return 0;
}

//检测无线网卡wlan0状态(1:连接，0：断开)
int check_wlan0(void)
{
	FILE *fp;
	char buff[2] = {'\0'};

	if(NULL != (fp = fopen("/sys/class/net/wlan0/carrier", "r")))
	{
		fgets(buff, 2, fp);
		fclose(fp);
		return atoi(buff);
	}
	else
		return 0;
}

//读取上一次的模式(0:关闭wlan; 1:主机模式; 2：从机模式)
int get_mode(void)
{
	FILE *fp;
	char buff[2];

	if(NULL != (fp = fopen("/etc/yuneng/wifi_stat.conf", "r")))
	{
		memset(buff, 0, sizeof(buff));
		fgets(buff, 2, fp);
		fclose(fp);
		fp = NULL;
		return atoi(buff);
	}
	else
	{
		fp = fopen("/etc/yuneng/wifi_stat.conf", "w");
		fputs("1\n",fp);
		fclose(fp);
		fp = NULL;
		return 1;
	}
}

//检查或建立AP配置文件
int check_ap_init(void)
{
	FILE *fp;
	char buff[32];
	int set_flag = 0;

	if(NULL != (fp = fopen("/etc/yuneng/wifi_ap_info.conf", "r")))
	{
		fgets(buff, sizeof(buff), fp);
		fclose(fp);
		if(!strncmp(buff, "#SSID=ECU-WIFI_0000", 19)){
			set_flag = 1;
		}
	}
	else {
		set_flag = 1;
	}

	if(set_flag == 1)
	{
		fp = fopen("/etc/yuneng/wifi_ap_info.conf", "w");
		if(fp){
			memset(buff, 0, sizeof(buff));
			sprintf(buff, "#SSID=ECU-WIFI_%04d\n", get_ecuid()%10000);
			fputs(buff, fp);
			fputs("#method=0\n"
				  "#channel=0\n"
				  "interface=wlan0\n"
				  "driver=ar6000\n", fp);
			sprintf(buff, "ssid=ECU-WIFI_%04d\n", get_ecuid()%10000);
			fputs(buff, fp);
			fputs("channel=6\n"
				  "ignore_broadcast_ssid=0\n", fp);
			fclose(fp);

		}
	}
	return 0;
}
//检查IP网络配置文件udhcpd
int check_udhcpd(void)
{
	FILE *fp;
	char buff[32];

	if(access("/etc/yuneng/udhcpd.conf", F_OK))
	{
		fp = fopen("/etc/yuneng/udhcpd.conf", "w");
		fputs("start 192.168.0.100\n"
			  "end 192.168.0.198\n"
			  "interface wlan0\n"
			  "max_leases 99\n"
			  "opt dns 192.168.0.1 192.168.3.1\n"
			  "option subnet 255.255.255.0\n"
			  "opt router 192.168.0.1", fp);
		fclose(fp);
	}
	return 0;
}

//添加网络
int add_route(void)
{
	FILE *fp;
	char buff[128] = {'\0'};
	char cmd[128] = {'\0'};
	char ip[16] = {'\0'};
	int i;

	fp = fopen("/etc/resolv.conf", "r");
	while(1)
	{
		memset(buff, 0, sizeof(buff));
		fgets(buff, 50, fp);
		if(!strlen(buff))
			break;
		if(!strncmp(buff, "nameserver", 10))
		{
			sscanf(&buff[11], "%[^\n]", ip);
		}
	}
	fclose(fp);
	i = strlen(ip)-1;
	while(i > 0)
	{
		if(!strncmp(&ip[i], ".", 1))
		{
			strncpy(&ip[i+1], "0", 1);
			break;
		}
		else
		{
			ip[i+1] = '\0';
			i--;
		}
	}
	snprintf(cmd, sizeof(cmd), "/sbin/route add -net %s netmask 255.255.255.0 dev wlan0", ip);
	return system(cmd);
}

int main(void)
{
	FILE *fp;
	char buff[512];
	int ret;

	//检测网线是否连接，若连接需要udhcpc.sh重新执行udhcpc命令
	//目的是当开机启动时，如果网线连接的状态下，ECU自动连接WIFI后，任何时间拔掉网线，使用无线能正常上网
//	if(check_eth0())
//	{
//		system("killall udhcpc");
//		usleep(10000);
//		system("udhcpc -s /etc/yuneng/udhcpc.sh");
//	}
	//加载模块驱动
	//system("/sbin/insmod /home/modules/ar6000.ko fwpath=/home/");
	//sleep(1);
	if(1 == get_mode())
	{
		system("/usr/sbin/iwconfig wlan0 mode master");	//设置模式为主机模式
		usleep(10000);
		system("/sbin/ifconfig wlan0 192.168.0.1");	//设置wlan0的IP地址
		usleep(10000);
		check_udhcpd();
		check_ap_init();
		system("/usr/sbin/hostapd /etc/yuneng/wifi_ap_info.conf -B");	//建立默认的AP
		sleep(1);
		system("/usr/sbin/udhcpd /etc/yuneng/udhcpd_wlan0.conf &");	//自动为连接的设备分配IP
	}
	else if(2 == get_mode())
	{
		system("/usr/sbin/iwconfig wlan0 mode managed");	//设置模式为从机模式
		usleep(10000);
		while(!check_wlan0() && !access("/etc/yuneng/wifi_signal_info.conf", F_OK))
		{
			ret = system("/usr/sbin/wpa_supplicant -Dwext -i wlan0 -c /etc/yuneng/wifi_signal_info.conf -B");
			sleep(1);
			if(!ret)
			{
				//密码正确
				ret = system("/sbin/udhcpc -nq -i wlan0" );	//向路由器请求分配IP地址
				sleep(1);
				if(!ret)
				{
					//分配到IP地址（连接成功）
					if(!check_eth0())
					{
						system("/sbin/route del -net 60.0.0.0 netmask 255.0.0.0 dev eth0");
					}
					add_route();
					break;
				}
				else
				{
					//分配IP地址失败
					system("killall wpa_supplicant");
					sleep(60);
					continue;
				}
			}
			else
			{
				//密码错误
				system("killall wpa_supplicant");
				sleep(60);
				continue;
			}
		}
	}
}
