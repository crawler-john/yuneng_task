#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
	{
		return 0;
	}
}

int main(int argc, char *argv[])
{
	struct ifreq ifr;
	char ip_addr[20]={'\0'};
	int socket_ip;
	int flag=0;
	FILE *fp;
	char autoflag='1';

	socket_ip=socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, "eth0");

	fp=fopen("/etc/yuneng/dhcp.conf", "r");
	autoflag=fgetc(fp);
	fclose(fp);
	if(autoflag=='0')
		exit(0);
    
    do{
    	sleep(300);
    	system("killall udhcpc");
    	sleep(1);
    	system("udhcpc &");
    	sleep(30);
	system("killall udhcpc");
    	
    	if(ioctl(socket_ip,SIOCGIFADDR,&ifr)<0){
    		printf("ioctl<0\n");
    		flag=1;
    		//if(!check_wlan0())
    		system("ifconfig eth0 192.168.131.228");
    	}
    	else{
    		strcpy(ip_addr,inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr));
    		//printf("%s\n", ip_addr);
    		if(0==strcmp(ip_addr, "192.168.131.228"))
    			flag=1;
    		else{
    			//system("killall udhcpd");
    			flag=0;
    		}
    	}
    }while(flag==1);

    return 0;
}
