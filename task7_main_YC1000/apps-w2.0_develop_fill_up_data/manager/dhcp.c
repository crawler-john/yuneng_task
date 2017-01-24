#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>  //open(lcd)
#include <sys/ioctl.h>

int getip(void)
{
    int socket_ip;
    struct ifreq ifr;
    char ip_addr[20]={'\0'};
    int flag=0;

    socket_ip=socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(ifr.ifr_name, "eth0");
    if(ioctl(socket_ip,SIOCGIFADDR,&ifr)<0)
    {
      flag=0;
      printf("ioctl error!\n");
    }
    else
    {
      flag=1;
      strcpy(ip_addr,inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr));
      printf("IP ADDRESS:%s\n",ip_addr);
    }

    return flag;
}

void displayall(void)
{
    /*int fd_lcd;
    //char buff[21]={'\0'};
    //memset(buff,'0xff',20);

    fd_lcd=open("/dev/lcd",O_WRONLY);
    ioctl(fd_lcd,0xC0,0);
    write(fd_lcd,buff,20);
    close(fd_lcd);*/
    int fd_lcd;
    char display[21]={'\0'};

    fd_lcd=open("/dev/lcd",O_WRONLY);
    ioctl(fd_lcd,0x01,0);
    strcpy(display,"Loading...");
    ioctl(fd_lcd,0x85,0);
    write(fd_lcd,display,strlen(display));
    close(fd_lcd);

}

int dhcp(void)
{
    int getip_flag,i;

    displayall();
    system("/sbin/udhcpc &");
    sleep(15);
    getip_flag=getip();
    if(1==getip_flag)
    {
      return 1;
    }
    else
    {
      printf("in first else\n");
      system("killall udhcpc");
      system("/sbin/udhcpc &");
      sleep(30);
      getip_flag=getip();
      if(1==getip_flag)
        return 1;
      else
      {
        printf("in second else\n");
        system("killall udhcpc");
        //udhcpc程序启动，一直获取IP，直到成功
        system("ifconfig eth0 192.168.131.228");
        sleep(1);
//        system("/usr/sbin/udhcpd -f /etc/yuneng/udhcpd_eth0.conf &");				//开启有线DHCP服务器
        //system("ifconfig eth0 169.254.139.228");
        system("/home/applications/network.exe &");
        return 0;
      }
    }
}
