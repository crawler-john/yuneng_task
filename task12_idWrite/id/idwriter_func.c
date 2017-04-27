#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
void update_time()
{
    int fp;
    char write_buff[50]={'\0'};
    char temp[5]={'\0'};
    time_t tm;
    struct timeval tv;
    time_t timep;

    struct tm *timenow;
	time(&tm);
//	memcpy(&timenow,localtime(&tm), sizeof(timenow));
	timenow=localtime(&tm);
    tv.tv_sec = tm;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

	sprintf(temp,"%d",timenow->tm_year+1900);

    strcat(write_buff,temp);
    if(timenow->tm_mon+1<10)
      strcat(write_buff,"0");
    memset(temp,'\0',5);
    sprintf(temp,"%d",timenow->tm_mon+1);
    strcat(write_buff,temp);
    if(timenow->tm_mday<10)
      strcat(write_buff,"0");
    memset(temp,'\0',5);
    sprintf(temp,"%d",timenow->tm_mday);
    strcat(write_buff,temp);
    if(timenow->tm_hour<10)
      strcat(write_buff,"0");
    memset(temp,'\0',5);
    sprintf(temp,"%d",timenow->tm_hour);
    strcat(write_buff,temp);
    if(timenow->tm_min<10)
      strcat(write_buff,"0");
    memset(temp,'\0',5);
    sprintf(temp,"%d",timenow->tm_min);
    strcat(write_buff,temp);
    if(timenow->tm_sec<10)
      strcat(write_buff,"0");
    memset(temp,'\0',5);
    sprintf(temp,"%d",timenow->tm_sec);
    strcat(write_buff,temp);
     printf("write in rtc:%s\n",write_buff);

    fp=open("/dev/rtc2",O_WRONLY);
    write(fp,write_buff,strlen(write_buff));
    close(fp);
}
