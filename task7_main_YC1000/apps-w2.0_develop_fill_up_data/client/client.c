/*
 * client.c
 * V1.2.2
 * modified on: 2013-08-13
 * 与EMA异步通信
 * update操作数据库出现上锁时，延时再update
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <net/if.h>	//ifr
#include <netdb.h>
#include "sqlite3.h"

//#define DEBUG
//#define DEBUGLOG

sqlite3 *db;

void printmsg(char *msg)		//打印字符串
{
#ifdef DEBUG
	printf("Client: %s!\n", msg);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/client.log", "a");
	if(fp)
	{
		fprintf(fp, "Client: %s!\n", msg);
		fclose(fp);
	}
#endif
}

void print2msg(char *msg1, char *msg2)		//打印字符串
{
#ifdef DEBUG
	printf("Client: %s, %s!\n", msg1, msg2);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/client.log", "a");
	if(fp)
	{
		fprintf(fp, "Client: %s, %s!\n", msg1, msg2);
		fclose(fp);
	}
#endif
}

void printdecmsg(char *msg, int data)		//打印整形数据
{
#ifdef DEBUG
	printf("Client: %s: %d!\n", msg, data);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/client.log", "a");
	if(fp)
	{
		fprintf(fp, "Client: %s: %d!\n", msg, data);
		fclose(fp);
	}
#endif
}

void printhexmsg(char *msg, char *data, int size)		//打印十六进制数据
{
#ifdef DEBUG
	int i;

	printf("Client: %s: ", msg);
	for(i=0; i<size; i++)
		printf("%X, ", data[i]);
	printf("\n");
#endif
#ifdef DEBUGLOG
	FILE *fp;
	int j;

	fp = fopen("/home/client.log", "a");
	if(fp)
	{
		fprintf(fp, "Client: %s: ", msg);
		for(j=0; j<size; j++)
			fprintf(fp, "%X, ", data[j]);
		fprintf(fp, "\n");
		fclose(fp);
	}
#endif
}

void getcurrenttime(char db_time[])		//ECU上显示用的时间，与上面函数获取的时间只是格式区别，格式：年-月-日 时：分：秒，如2012-09-02 14：28：35
{
	time_t tm;
	struct tm record_time;    //记录时间
	char temp[5]={'\0'};

	time(&tm);
	memcpy(&record_time,localtime(&tm), sizeof(record_time));
	sprintf(temp, "%d", record_time.tm_year+1900);
	strcat(db_time,temp);
	strcat(db_time,"-");
	if(record_time.tm_mon+1<10)
		strcat(db_time,"0");
	sprintf(temp, "%d", record_time.tm_mon+1);
	strcat(db_time,temp);
	strcat(db_time,"-");
	if(record_time.tm_mday<10)
		strcat(db_time,"0");
	sprintf(temp, "%d", record_time.tm_mday);
	strcat(db_time,temp);
	strcat(db_time," ");
	if(record_time.tm_hour<10)
		strcat(db_time,"0");
	sprintf(temp, "%d", record_time.tm_hour);
	strcat(db_time,temp);
	strcat(db_time,":");
	if(record_time.tm_min<10)
		strcat(db_time,"0");
	sprintf(temp, "%d", record_time.tm_min);
	strcat(db_time,temp);
	strcat(db_time,":");
	if(record_time.tm_sec<10)
		strcat(db_time,"0");
	sprintf(temp, "%d", record_time.tm_sec);
	strcat(db_time,temp);
}

int get_hour()
{
	time_t tm;         //实例化time_t结构
	struct tm timenow;         //实例化tm结构指针

	time(&tm);
	memcpy(&timenow,localtime(&tm), sizeof(timenow));
	printdecmsg("Hour:", timenow.tm_hour);

	return timenow.tm_hour;
}

int writeconnecttime(void)			//保存最后一次连接上服务器的时间，在home页面上会显示
{
	char connecttime[20]={'\0'};
	FILE *fp;

	getcurrenttime(connecttime);
	fp=fopen("/etc/yuneng/connect_time.conf","w");
	fprintf(fp,"%s",connecttime);
	fclose(fp);

	return 0;
}

void showconnected(void)		//已连接EMA
{
	FILE *fp;

	fp = fopen("/tmp/connectemaflag.txt", "w");
	if(fp)
	{
		fputs("connected", fp);
		fclose(fp);
	}

}

void showdisconnected(void)		//无法连接EMA
{
	FILE *fp;

	fp = fopen("/tmp/connectemaflag.txt", "w");
	if(fp)
	{
		fputs("disconnected", fp);
		fclose(fp);
	}

}

int randvalue(void)
{
	int i;

	srand((unsigned)time(NULL));
	i = rand()%2;
	printdecmsg("Randvalue", i);

	return i;
}

int createsocket(void)					//创建套接字
{
	int fd_sock;

	fd_sock=socket(AF_INET,SOCK_STREAM,0);
	if(-1==fd_sock)
		printmsg("Failed to create socket");
	else
		printmsg("Create socket successfully");

	return fd_sock;
}

int connect_socket(int fd_sock)				//连接到服务器
{
	char domain[1024]={'\0'};		//EMA的域名
	char ip[20] = {'\0'};	//EMA的缺省IP
	int port[2]={8995, 8996}, i;
	char buff[1024] = {'\0'};
	struct sockaddr_in serv_addr;
	struct hostent * host;
	FILE *fp;

	fp = fopen("/etc/yuneng/datacenter.conf", "r");
	if(fp)
	{
		while(1)
		{
			memset(buff, '\0', sizeof(buff));
			fgets(buff, sizeof(buff), fp);
			if(!strlen(buff))
				break;
			if(!strncmp(buff, "Domain", 6))
			{
				strcpy(domain, &buff[7]);
				if('\n' == domain[strlen(domain)-1])
					domain[strlen(domain)-1] = '\0';
			}
			if(!strncmp(buff, "IP", 2))
			{
				strcpy(ip, &buff[3]);
				if('\n' == ip[strlen(ip)-1])
					ip[strlen(ip)-1] = '\0';
			}
			if(!strncmp(buff, "Port1", 5))
				port[0]=atoi(&buff[6]);
			if(!strncmp(buff, "Port2", 5))
				port[1]=atoi(&buff[6]);
		}
		fclose(fp);
	}

	if(!strlen(domain))
		strcpy(domain, "ecu.apsema.com");
	if(!strlen(ip))
		strcpy(ip, "60.190.131.190");


	host = gethostbyname(domain);
	if(NULL == host)
	{
		printmsg("Resolve domain failure");
	}
	else
	{
		memset(ip, '\0', sizeof(ip));
		inet_ntop(AF_INET, *host->h_addr_list, ip, 32);
		printmsg(ip);
	}

	print2msg("IP", ip);
	printdecmsg("Port1", port[0]);
	printdecmsg("Port2", port[1]);

	bzero(&serv_addr,sizeof(struct sockaddr_in));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(port[randvalue()]);
	serv_addr.sin_addr.s_addr=inet_addr(ip);
	bzero(&(serv_addr.sin_zero),8);

	if(-1==connect(fd_sock,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr))){
		showdisconnected();
		printmsg("Failed to connect to EMA");
		return -1;
	}
	else{
		showconnected();
		printmsg("Connect to EMA successfully");
		writeconnecttime();
		return 1;
	}
}

/*int connect_socket(int fd_sock)				//连接到服务器
{
	char ip[20] = {'\0'};
	char tmp[20] = {'\0'};
	int port[2]={8990}, i;

	FILE *fp;

	fp = fopen("/etc/yuneng/datacenter.conf", "r");
	fgets(ip, 50, fp);
	for(i=0; i<2; i++)
	{
		memset(tmp, '\0', sizeof(tmp));
		fgets(tmp, sizeof(tmp), fp);
		if(strlen(tmp)){
			port[i] = atoi(tmp);
			printdecmsg("Port", port[i]);
		}
	}
	fclose(fp);

	ip[strlen(ip)-1] = '\0';
	/*if('\n' == tmp[strlen(tmp)-1])
		tmp[strlen(tmp)-1] = '\0';
	port = port[randvalue()]);*/
	//printf("%s, %d\n", ip, port);
	/*if(!strlen(ip))
		strcpy(ip, "60.190.131.190");
	struct sockaddr_in serv_addr;

	bzero(&serv_addr,sizeof(struct sockaddr_in));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(port[randvalue()]);
	serv_addr.sin_addr.s_addr=inet_addr(ip);
	bzero(&(serv_addr.sin_zero),8);

	if(-1==connect(fd_sock,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr))){
		showdisconnected();
		printmsg("Failed to connect to EMA");	//thread
		return -1;
	}
	else{
		showconnected();
		printmsg("Connect to EMA successfully");
		writeconnecttime();
		return 1;
	}
}*/

void close_socket(int fd_sock)					//关闭套接字
{
	close(fd_sock);
	printmsg("Close socket");
}

int clear_send_flag(char *readbuff)
{
	int i, j, count;		//EMA返回多少个时间（有几个时间，就说明EMA保存了多少条记录）
	char recv_date_time[16];
	char sql[1024]={'\0'};
	char *zErrMsg=0;

	if(strlen(readbuff) >= 3)
	{
		count = (readbuff[1] - 0x30) * 10 + (readbuff[2] - 0x30);
		if(count == (strlen(readbuff) - 3) / 14)
		{
			for(i=0; i<count; i++)
			{
				memset(sql,'\0',sizeof(sql));
				memset(recv_date_time, '\0', sizeof(recv_date_time));
				strncpy(recv_date_time, &readbuff[3+i*14], 14);
				sprintf(sql,"UPDATE Data SET resendflag='0' WHERE date_time='%s'", recv_date_time);
				for(j=0; j<3; j++)
				{
					if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
					{
						print2msg("Clear send flag into database", zErrMsg);
						break;
					}
					else
						print2msg("Clear send flag into database", zErrMsg);
					sleep(1);
				}
			}
		}
	}

	return 0;
}

int update_send_flag(char *send_date_time)
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	int i;

	memset(sql,'\0',sizeof(sql));
	sprintf(sql,"UPDATE Data SET resendflag='2' WHERE date_time='%s'", send_date_time);
	for(i=0; i<3; i++)
	{
		if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
		{
			print2msg("Update send flag into database", zErrMsg);
			break;
		}
		sleep(5);
	}

	return 0;
}

int recv_response(int fd_sock, char *readbuff)
{
	fd_set rd;
	struct timeval timeout;
	int recvbytes, res, count=0, readbytes = 0;
	char recvbuff[65535], temp[16];

//	FD_ZERO(&rd);
//	FD_SET(fd_sock, &rd);
//	timeout.tv_sec = 120;
//	timeout.tv_usec = 0;

//	res = select(fd_sock+1, &rd, NULL , NULL, &timeout);
//	if(res <= 0){
//		printmsg("Receive date from EMA timeout");
//		return -1;
//	}
//	else{
//		sleep(1);
//		recvbytes = recv(fd_sock, readbuff, 65535, 0);
//		printmsg(readbuff);
//		printdecmsg("Receive", recvbytes);
//		if(0 == recvbytes)
//			return -1;
//		else
//			return recvbytes;
//	}

	while(1)
	{
		FD_ZERO(&rd);
		FD_SET(fd_sock, &rd);
		timeout.tv_sec = 120;
		timeout.tv_usec = 0;

		res = select(fd_sock+1, &rd, NULL, NULL, &timeout);
		if(res <= 0){
			//printerrmsg("select");
			printmsg("Receive data reply from EMA timeout");
			return -1;
		}
		else{
			memset(recvbuff, '\0', sizeof(recvbuff));
			memset(temp, '\0', sizeof(temp));
			recvbytes = recv(fd_sock, recvbuff, sizeof(recvbuff), 0);
			if(0 == recvbytes)
				return -1;
			strcat(readbuff, recvbuff);
			readbytes += recvbytes;
			if(readbytes >= 3)
			{
				count = (readbuff[1]-0x30)*10 + (readbuff[2]-0x30);
				if(count==((strlen(readbuff)-3)/14))
					return readbytes;
			}

//				for(i=5, j=0; i<10; i++)
//				{
//					if(readbuff[i] <= '9' && readbuff[i] >= '0')
//						temp[j++] = readbuff[i];
//				}

//				if(atoi(temp) == strlen(readbuff))
//				{
//					print2msg("Recv", readbuff);
//					printdecmsg("Receive", readbytes);
//					return readbytes;
//				}

		}
	}
}

int send_record(int fd_sock, char *sendbuff, char *send_date_time)			//发送数据到EMA
{
	int sendbytes=0, res=0;
	char readbuff[65535] = {'\0'};

	sendbytes = send(fd_sock, sendbuff, strlen(sendbuff), 0);

	if(-1 == sendbytes)
		return -1;

	if(-1 == recv_response(fd_sock, readbuff))
		return -1;
	else
	{
		if('1' == readbuff[0])
			update_send_flag(send_date_time);
		clear_send_flag(readbuff);
		return 0;
	}

}

int preprocess()			//发送头信息到EMA，读取已经已经存入EMA的记录的时间
{
	int sendbytes=0, res=0, recvbytes = 0;
	char readbuff[65535] = {'\0'};
	fd_set rd;
	struct timeval timeout;
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	char sendbuff[256] = {'\0'};
	FILE *fp;
	char **azResult;
	int nrow, ncolumn;
	int fd_sock;

	strcpy(sql,"SELECT date_time FROM Data WHERE resendflag='2'");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
	print2msg("Select nrow of resendflag=2 from db", zErrMsg);
	sqlite3_free_table( azResult );
	if(nrow < 1)
		return 0;

	strcpy(sendbuff, "APS13AAA22");
	fp = fopen("/etc/yuneng/ecuid.conf", "r");
	if(fp)
	{
		fgets(&sendbuff[10], 13, fp);
		fclose(fp);
	}
	strcat(sendbuff, "\n");
	print2msg("Sendbuff", sendbuff);

	fd_sock = createsocket();
	printdecmsg("Socket", fd_sock);
	if(1 == connect_socket(fd_sock))
	{
		while(1)
		{
			memset(readbuff, '\0', sizeof(readbuff));
			sendbytes = send(fd_sock, sendbuff, strlen(sendbuff), 0);
			if(-1 == sendbytes)
			{
				close_socket(fd_sock);
				return -1;
			}
			if(recv_response(fd_sock, readbuff) > 3)
				clear_send_flag(readbuff);
			else
				break;
		}
	}

	close_socket(fd_sock);
	return 0;
}

int resend_record()
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	char **azResult;
	int i, res, nrow, ncolumn;
	int fd_sock;

	memset(sql,'\0',sizeof(sql));
	strcpy(sql,"SELECT record,date_time FROM Data WHERE resendflag='2'");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
	print2msg("Select result from db", zErrMsg);
	if(nrow >= 1)
	{
		printdecmsg("Record count(flag=2)", nrow);
		fd_sock = createsocket();
		printdecmsg("Socket", fd_sock);
		if(1 == connect_socket(fd_sock))
		{
			for(i=nrow; i>=1; i--){
				if(1 != i)
					azResult[i*ncolumn][78] = '1';
				printmsg(azResult[i*ncolumn]);
				res = send_record(fd_sock, azResult[i*ncolumn], azResult[i*ncolumn+1]);
				if(-1 == res)
					break;
			}
		}
		close_socket(fd_sock);
	}
	else
		printmsg("There are none record");
	sqlite3_free_table( azResult );

	return 0;
}

int main(int argc, char *argv[])
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	char **azResult;
	int nrow, ncolumn, res, i;
	int fd_sock;
	int thistime=0, lasttime=0;

	printmsg("Started");

	while(1)
	{
		thistime = lasttime = time(NULL);
		sqlite3_open("/home/record.db", &db);

		if(1 == get_hour())
		{
			preprocess();		//预处理，先发送一个头信息给EMA，让EMA把标记为2的记录的时间返回给ECU，然后ECU再把标记为2的记录再次发给EMA，防止EMA收到记录返回收到标记而没有存入数据库的情况
			resend_record();
		}

		memset(sql,'\0',sizeof(sql));
		strcpy(sql,"SELECT record,date_time FROM Data WHERE resendflag='1'");
		sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
		print2msg("Select result from db", zErrMsg);
		if(nrow >= 1)
		{
			printdecmsg("Record count", nrow);
			fd_sock = createsocket();
			printdecmsg("Socket", fd_sock);
			if(1 == connect_socket(fd_sock))
			{
				for(i=nrow; i>=1; i--){
					if(thistime-lasttime > 300)
					{
						//sqlite3_free_table( azResult );
						break;
					}
					if(1 != i)
						azResult[i*ncolumn][78] = '1';
					printmsg(azResult[i*ncolumn]);
					res = send_record(fd_sock, azResult[i*ncolumn], azResult[i*ncolumn+1]);
					if(-1 == res)
						break;
					thistime = time(NULL);
				}
			}
			close_socket(fd_sock);
		}
		else
			printmsg("There are none record");
		sqlite3_free_table( azResult );
		sqlite3_close(db);
		if(thistime-lasttime < 300)
			sleep(300 + lasttime - thistime);
	}

	return 0;
}
