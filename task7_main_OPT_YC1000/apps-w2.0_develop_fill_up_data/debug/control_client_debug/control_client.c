/**
 * Filename : control_client.c
 * Author : huazhen
 * Version : 1.5.2
 * Date : 2013/11/01
 * Description : EMA_ECU远程控制程序
 * Others : 其他
 * Function List : 主要函数列表
 * History :
 *  2014.05.14 EMA向ECU发送设置指令后，ECU上传信息的时间戳改为0
 *  2014.06.18 逆变器列表与时区改变后重启main.exe程序
 *  2014.06.19 1.添加了my_sqlite3_exec函数：所有对数据库的操作sqlite3_exec改为最多三次
    		   2.每次轮询只查一次process_result表格，将标志位为1的数据发送给EMA，并将标志位清零（原来是每次查一条）
 *  2014.06.23 set_time_zone函数中在“system("killall main.exe");”后增加如下代码：
    			system("killall client");
				system("killall resmonitor");
				usleep(100000);
				system("/home/applications/resmonitor &");
 * 	2014.07.08 增加A120，A121这两个新的函数
 * 	2014.07.23 新增new_set_ac_protection函数，用于设置新的13项交流保护参数
 * 	2014.07.30 A102新增数字版本号
 * 	2014.08.01 主动上报函数增加对逆变器级别的变动数据拼接并上传给EMA的功能
 * 	2014.08.07 1.A113，A117，A120上报给EMA的最大功率范围改为20-270W
 * 	           2.A110设置最大功率函数，将设置标志位从配置文件移至power表
 * 	           3.A103添加或删除逆变器时同时更新power表中的数据
 * 	2014.09.15 1.将GFDI设置的标志为从配置文件转移到数据库中的clear_gfdi表
 * 			   2.将远程开关机的标志位从配置文件转移到数据库中的turn_on_off表
 * 			   3.A122设置13项保护参数新增指定逆变器来设置参数
 * 	2014.09.23 A113，A117，A120上报给EMA的最大功率范围改为20-300W
 * 	2014.12.15 新增电网环境设置(A125)
 * 			   新增IRD设置(A127)
 *  2014.12.25 新增逆变器状态上报(A123)
 *  2014.12.27 逆变器状态上报，新增EMA应答反馈处理（收到EMA应答后再删除数据）
 * ********************************************************************************************
 *  v1.5.2	(2015.01.07)
 * 			修改主函数:将“先建立socket连接再查询数据并发送”改为“先查询数据再建立socket连接并发送”
 * 			新增：A128EMA主动查询逆变器的信号强度，A129EMA主动查询系统的电网质量
 * 	v1.5.2  (2015.02.02)
 * 	        增加A124主动读取电网环境和A126主动读取IRD功能
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <termios.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include "sqlite3.h"

#define DEBUG			//开启控制台调试信息
//#define DEBUGLOG		//开启日志文件

#define MSGVERSION 13

int TimeOut = 10;
int Minutes = 15;
char ecuid[13] = {'\0'};

/***************************************************
 * Function : my_sqlite3_exec()
 * Description :
 * Calls : sqlite3_exec()
 * Called By :
 * Table Accessed :
 * Table Updated :
 * Input :
 * Output :
 * Return : success(0) failure(-1)
 * Others :
 ***************************************************/
int my_sqlite3_exec(sqlite3* db, char *sql, char *zErrMsg)
{
	int i;

	for(i = 0; i < 3; i++)
	{
		if(!sqlite3_exec(db, sql, 0, 0, &zErrMsg))
			return 0;
		usleep(100000);
	}
	return -1;
}

/* 打印字符串 */
void printmsg(char *msg)
{
#ifdef DEBUG
	printf("control_client: %s!\n", msg);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/control_client.log", "a");
	if(fp)
	{
		fprintf(fp, "control_client: %s!\n", msg);
		fclose(fp);
	}
#endif
}

/* 打印两个字符串 */
void print2msg(char *msg1, char *msg2)
{
#ifdef DEBUG
	printf("control_client: %s: %s\n", msg1, msg2);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/control_client.log", "a");
	if(fp)
	{
		fprintf(fp, "control_client: %s: %s\n", msg1, msg2);
		fclose(fp);
	}
#endif
}

/* 打印整形数据 */
void printdecmsg(char *msg, int data)
{
#ifdef DEBUG
	printf("control_client: %s: %d!\n", msg, data);
#endif
#ifdef DEBUGLOG
	FILE *fp;

	fp = fopen("/home/control_client.log", "a");
	if(fp)
	{
		fprintf(fp, "control_client: %s: %d!\n", msg, data);
		fclose(fp);
	}
#endif
}

/* 打印十六进制数据 */
void printhexmsg(char *msg, char *data, int size)
{
#ifdef DEBUG
	int i;

	printf("control_client: %s: ", msg);
	for (i = 0; i < size; i++)
		printf("%X, ", data[i]);
	printf("\n");
#endif
#ifdef DEBUGLOG
	FILE *fp;
	int j;

	fp = fopen("/home/control_client.log", "a");
	if(fp)
	{
		fprintf(fp, "control_client: %s: ", msg);
		for(j=0; j<size; j++)
		fprintf(fp, "%X, ", data[j]);
		fprintf(fp, "\n");
		fclose(fp);
	}
#endif
}

/* 打印错误消息 */
void printerrmsg(char *errmsg)
{
#ifdef DEBUG
	fprintf(stderr, "%s:%s\n", errmsg, strerror(errno));
#endif
#ifdef DEBUGLOG
	FILE *fp;
	char buff[256] = {'\0'};

	sprintf(buff, "%s:%s\n", errmsg, strerror(errno));
	fp = fopen("/home/control_client.log", "a");
	if(fp)
	{
		fprintf(fp, "Error: %s", buff);
		fclose(fp);
	}
#endif
}

/* 获取当前时间 */
void getcurrenttime(char curtime[])
{
	time_t tm;
	struct tm record_time;

	time(&tm);
	memcpy(&record_time, localtime(&tm), sizeof(record_time));
	sprintf(curtime, "%04d%02d%02d%02d%02d%02d",
			record_time.tm_year + 1900,
			record_time.tm_mon + 1,
			record_time.tm_mday,
			record_time.tm_hour,
			record_time.tm_min,
			record_time.tm_sec);
}

/* 打印当前时间 */
void writecurrenttime(void)
{
	time_t tm;
	struct tm record_time;
	char curtime[20] = { '\0' };

	time(&tm);
	memcpy(&record_time, localtime(&tm), sizeof(record_time));
	sprintf(curtime, "%04d-%02d-%02d %02d:%02d:%02d",
			record_time.tm_year + 1900,
			record_time.tm_mon + 1,
			record_time.tm_mday,
			record_time.tm_hour,
			record_time.tm_min,
			record_time.tm_sec);
	print2msg("current_time", curtime);
}

/* 删除左侧指定字符 */
void deletechar(char ch, char *string)
{
	int i;
	while (ch == string[0])
	{
		for (i = 0; i < strlen(string); i++)
		{
			string[i] = string[i + 1];
		}
	}
}

/* 随机取0或1 */
int randvalue(void)
{
	int i;
	srand((unsigned)time(NULL));
	i = rand()%2;
	return i;
}

/* 创建socket */
int create_socket(void)
{
	int sockfd;
	if (-1 == (sockfd = socket(AF_INET, SOCK_STREAM, 0)))
	{
		printerrmsg("socket");
		return -1;
	}
	return sockfd;
}

/* 建立连接 */
int connect_socket(int sockfd)
{
	char buf[1024] = {'\0'};
	char connecttime[20] = {'\0'};
	struct sockaddr_in serv_addr;
	struct hostent *host;
	char Domain[1024]={'\0'};
	char IP[20] = {'\0'};
	int Port[2]={8997, 8997};
	int rand = randvalue();
	int i;
	FILE *fp;

	if (NULL == (fp = fopen("/etc/yuneng/controlclient.conf", "r")))
	{
		printerrmsg("controlclient.conf");
		return -1;
	}
	else
	{
		while(1)
		{
			memset(buf, '\0', sizeof(buf));
			fgets(buf, sizeof(buf), fp);
			if(!strlen(buf))
				break;
			if(!strncmp(buf, "Timeout", 7))
			{
				TimeOut = atoi(&buf[8]);
			}
			if(!strncmp(buf, "Report_Interval", 15))
			{
				Minutes = atoi(&buf[16]);
			}
			if(!strncmp(buf, "Domain", 6))
			{
				sscanf(&buf[7], "%[^\n]", Domain);
			}
			if(!strncmp(buf, "IP", 2))
			{
				sscanf(&buf[3], "%[^\n]", IP);
			}
			if(!strncmp(buf, "Port1", 5))
			{
				Port[0]=atoi(&buf[6]);
			}
			if(!strncmp(buf, "Port2", 5))
			{
				Port[1]=atoi(&buf[6]);
			}
		}
		fclose(fp);
		fp = NULL;
	}
	if(!strlen(Domain) && !strlen(IP))
	{
		strcpy(Domain, "ecu.apsema.com");
	}
	if(strlen(Domain))
	{
		if(NULL == (host = gethostbyname(Domain)))
		{
			herror("gethostbyname");
			printmsg("Resolve domain failure");
			if(!strlen(IP))
				strcpy(IP, "60.190.131.190");
		}
		else
		{
			memset(IP, '\0', sizeof(IP));
			inet_ntop(AF_INET, *host->h_addr_list, IP, 32);
		}
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(Port[rand]);
	serv_addr.sin_addr.s_addr = inet_addr(IP);
	bzero(&(serv_addr.sin_zero), 8);
	for(i=0; i<3; i++)
	{
		if (-1 == connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)))
		{
			memset(buf, '\0', sizeof(buf));
			sprintf(buf,"Failed to connect to EMA %s:%d", IP, Port[rand]);
			printmsg(buf);
			writecurrenttime();
			printerrmsg("connect");
		}
		else
		{
			memset(buf, '\0', sizeof(buf));
			sprintf(buf,"Connected to ->  %s:%d  successfully", IP, Port[rand]);
			printmsg(buf);
			writecurrenttime();
			return 0;
		}
		sleep(1000);
	}
	return -1;
}

/**************************************************
 * 接收数据函数
 * 功能：
 *  接收从EMA发送过来的消息
 * 输入：
 *  fd_sock		SOCKET标志符
 * 输出：
 *  readbuff	接收到的数据
 * 返回值：
 *  readbytes 	成功（收到的字符数）
 *  -1 			失败
 **************************************************/
int recv_response(int fd_sock, char *readbuff)
{
	fd_set rd;
	struct timeval timeout;
	int recvbytes, readbytes = 0, res, i, j;
	char recvbuff[65535], temp[16];

	while(1)
	{
		FD_ZERO(&rd);
		FD_SET(fd_sock, &rd);
		timeout.tv_sec = TimeOut;
		timeout.tv_usec = 0;

		res = select(fd_sock+1, &rd, NULL, NULL, &timeout);
		if(res <= 0){
			printerrmsg("select");
			printmsg("Receive date from EMA timeout");
			return -1;
		}
		else{
			memset(recvbuff, '\0', sizeof(recvbuff));
			memset(temp, '\0', sizeof(temp));
			recvbytes = recv(fd_sock, recvbuff, 65535, 0);
			if(0 == recvbytes)
				return -1;
			strcat(readbuff, recvbuff);
			readbytes += recvbytes;
			if(readbytes >= 10)
			{
				for(i=5, j=0; i<10; i++)
				{
					if(readbuff[i] <= '9' && readbuff[i] >= '0')
						temp[j++] = readbuff[i];
				}
				if(atoi(temp) == strlen(readbuff))
				{
					print2msg("Recv", readbuff);
					printdecmsg("Receive", readbytes);
					return readbytes;
				}
			}
		}
	}
}


/**************************************************
 * 发送数据函数
 * 功能：
 *  将ECU的消息发送给EMA
 * 输入：
 *  fd_sock		SOCKET标志符
 *  record		需要发送的数据
 * 返回值：
 *  0 	成功
 *  1 	失败
 **************************************************/
int send_msg(int fd_sock, char *record)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		if (-1 != send(fd_sock, record, strlen(record), 0))
		{
			print2msg("Sent", record);
			return 0;
		}
	}
	printmsg("response_version_mac: send faild");
	return 1;
}


/***************************************************
 * 发送询问命令
 * 功能：
 *  向EMA发送询问命令
 * 输入：
 *  fd_sock 	SOCKET标志符
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int send_ack(int fd_sock)
{
	FILE *fp;
	char record[65535]={'\0'};
	int i;

	/* Head */
	strcpy(record, "APS13AAA51A101AAA0");
	/* ECU Message */
	strcat(record, ecuid);
	strcat(record,"A10100000000000000END");
	strcat(record, "\n");

	//发送
	if(send_msg(fd_sock, record))
		return 1;
	else
		return 0;
}


/*校验消息函数*/
int check_msg(char *recvbuf)
{
	int cmd_id = 0, length;
	char buff[128] = {'\0'};

	//校验开头APS与结尾END
	if (strncmp(recvbuf, "APS", 3) || strncmp(&recvbuf[strlen(recvbuf)-3], "END", 3))
		return -1;
	//校验长度
	strncpy(buff, &recvbuf[5], 5);
	deletechar('A', buff);
	length = atoi(buff);
	if(length != strlen(recvbuf))
		return -1;
	//校验ECUID
	memset(buff, '\0', sizeof(buff));
	strncpy(buff, &recvbuf[18], 12);
	if (strcmp(buff, ecuid))
		return -1;

	return cmd_id;
}


/***************************************************
 * 获取ECU版本号
 * 功能：
 *  获取ECU版本号
 * 返回值：
 *
 ***************************************************/
int get_type(void)
{
	char version[50] = {'\0'};

	FILE *fp;
	fp = fopen("/etc/yuneng/version.conf", "r");
	fgets(version, 50, fp);
	fclose(fp);

	if('\n' == version[strlen(version)-1])
		version[strlen(version)-1] = '\0';

	if(!strncmp(&version[strlen(version)-6], "NA-120", 6))
		return 2;
	else if(!strncmp(&version[strlen(version)-2], "MX", 2))
		return 2;
	else if(!strncmp(&version[strlen(version)-2], "NA", 2))
		return 1;
	else
		return 0;
}


/**************************************************
 * 保存处理结果
 * 功能：
 *  将命令处理结果保存到数据库的process_result表
 * 输入：
 *  item 	命令号
 *  result 	处理结果
 * 返回值：
 *  0 成功
 *  1 失败
 **************************************************/
int save_process_result(int item, char *result)
{
	sqlite3 *db;
	char *zErrMsg = 0;
	char sql[65535];
	int i;

	//打开数据库
	if(sqlite3_open("/home/database.db", &db)){
		sqlite3_close(db);
		return 1;
	}

	//更新数据
	strcpy(sql, "CREATE TABLE IF NOT EXISTS process_result(item INTEGER, result VARCHAR, flag INTEGER, PRIMARY KEY(item))");
	if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
	{
		sprintf(sql, "REPLACE INTO process_result (item, result, flag) VALUES(%d, '%s', 1)", item, result);
		for(i=0; i<3; i++)
		{
			if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
			{
				sqlite3_close(db);
				return 0;
			}
			else
			{
				print2msg("INSERT INTO process_result", zErrMsg);
			}
			sleep(1);
		}
	}
	else
	{
		print2msg("CREATE TABLE process_result", zErrMsg);
	}

	sqlite3_close(db);
	return 1;
}


/**************************************************
 * 上报处理结果
 * 功能：
 *  上报缓存在process_result表中的处理结果
 * 返回值：
 *  0 成功
 *  1 失败
 **************************************************/
int response_result()
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[1024] = {'\0'};
	char buf[1024] = {'\0'};
	char record[65535]= {'\0'};
	int i, j, k, item;
	int sockfd;
	char recvbuf[65535] = {'\0'};
	int timestamp_num = 0;

	//打开数据库
	if (sqlite3_open("/home/database.db", &db)){
		close(sockfd);
		return 2;
	}

/* ECU级别处理结果上报 */
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT * FROM process_result WHERE flag=1");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{
			for(j = 1; j <= nrow; j++)
			{
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//发送数据
				if (-1 != send(sockfd, azResult[j*ncolumn + 1], strlen(azResult[j*ncolumn + 1]), 0))
				{
					print2msg("Sent", azResult[j*ncolumn + 1]);
					item = atoi(azResult[j*ncolumn]);
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "UPDATE process_result SET flag=0 WHERE item=%d", item);
					my_sqlite3_exec(db, sql, zErrMsg);
				}
				//断开连接
				close(sockfd);
				sleep(1);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
		usleep(100000);//延时100毫秒
	}


/* 逆变器级别处理结果上报 */

	//A115 GFDI状态
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT result FROM inverter_process_result WHERE item=115 and flag=1");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{
			if(nrow){
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//准备数据
				memset(record, '\0', sizeof(record));
				sprintf(record, "APS1300000A115AAA0%s%04d00000000000000END", ecuid, nrow);
				for(j = 1; j <= nrow; j++)
				{
					strcat(record,azResult[j]);
				}
				memset(buf, '\0', sizeof(buf));
				sprintf(buf, "%05d", strlen(record));
				strncpy(&record[5], buf, 5);
				strcat(record, "\n");
				//发送数据
				if (-1 != send(sockfd, record, strlen(record), 0))
				{
					print2msg("Sent", record);
					item = 115;
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "UPDATE inverter_process_result SET flag=0 WHERE item=%d", item);
					my_sqlite3_exec(db, sql, zErrMsg);
				}
				//断开连接
				close(sockfd);
				sleep(1);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
		usleep(100000);
	}

	//A116 开关机状态
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT result FROM inverter_process_result WHERE item=116 and flag=1");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{
			if(nrow){
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//准备数据
				memset(record, '\0', sizeof(record));
				sprintf(record, "APS1300000A116AAA0%s%04d00000000000000END", ecuid, nrow);
				for(j = 1; j <= nrow; j++)
				{
					strcat(record,azResult[j]);
				}
				memset(buf, '\0', sizeof(buf));
				sprintf(buf, "%05d", strlen(record));
				strncpy(&record[5], buf, 5);
				strcat(record, "\n");
				//发送数据
				if (-1 != send(sockfd, record, strlen(record), 0))
				{
					print2msg("Sent", record);
					item = 116;
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "UPDATE inverter_process_result SET flag=0 WHERE item=%d", item);
					my_sqlite3_exec(db, sql, zErrMsg);
				}
				//断开连接
				close(sockfd);
				sleep(1);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
		usleep(100000);
	}

	//A117 最大功率
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT result FROM inverter_process_result WHERE item=117 and flag=1");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{
			if(nrow){
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//准备数据
				memset(record, '\0', sizeof(record));
				sprintf(record, "APS1300000A117AAA0%s%04d00000000000000END", ecuid, nrow);
				for(j = 1; j <= nrow; j++)
				{
					strcat(record,azResult[j]);
				}
				memset(buf, '\0', sizeof(buf));
				sprintf(buf, "%05d", strlen(record));
				strncpy(&record[5], buf, 5);
				strcat(record, "\n");
				//发送数据
				if (-1 != send(sockfd, record, strlen(record), 0))
				{
					print2msg("Sent", record);
					item = 117;
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "UPDATE inverter_process_result SET flag=0 WHERE item=%d", item);
					my_sqlite3_exec(db, sql, zErrMsg);
				}
				//断开连接
				close(sockfd);
				sleep(1);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
		usleep(100000);
	}

	//A121交流保护参数
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT result FROM inverter_process_result WHERE item=121 and flag=1");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{
			if(nrow){
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//准备数据
				memset(record, '\0', sizeof(record));
				sprintf(record, "APS1300000A121AAA0%s%04d00000000000000END", ecuid, nrow);
				for(j = 1; j <= nrow; j++)
				{
					strcat(record,azResult[j]);
				}
				memset(buf, '\0', sizeof(buf));
				sprintf(buf, "%05d", strlen(record));
				strncpy(&record[5], buf, 5);
				strcat(record, "\n");
				//发送数据
				if (-1 != send(sockfd, record, strlen(record), 0))
				{
					print2msg("Sent", record);
					item = 121;
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "UPDATE inverter_process_result SET flag=0 WHERE item=%d", item);
					my_sqlite3_exec(db, sql, zErrMsg);
				}
				//断开连接
				close(sockfd);
				sleep(1);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
		usleep(100000);
	}

	//A124上报电网环境
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT result FROM inverter_process_result WHERE item=124 and flag=1");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{
			if(nrow){
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//准备数据
				memset(record, '\0', sizeof(record));
				sprintf(record, "APS1300000A124AAA0%s%04d00000000000000END", ecuid, nrow);
				for(j = 1; j <= nrow; j++)
				{
					strcat(record,azResult[j]);
				}
				memset(buf, '\0', sizeof(buf));
				sprintf(buf, "%05d", strlen(record));
				strncpy(&record[5], buf, 5);
				strcat(record, "\n");
				//发送数据
				if (-1 != send(sockfd, record, strlen(record), 0))
				{
					print2msg("Sent", record);
					item = 124;
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "UPDATE inverter_process_result SET flag=0 WHERE item=%d", item);
					my_sqlite3_exec(db, sql, zErrMsg);
				}
				//断开连接
				close(sockfd);
				sleep(1);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
		usleep(100000);
	}

	//A126上报IRD
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT result FROM inverter_process_result WHERE item=126 and flag=1");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{
			if(nrow){
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//准备数据
				memset(record, '\0', sizeof(record));
				sprintf(record, "APS1300000A126AAA0%s%04d00000000000000END", ecuid, nrow);
				for(j = 1; j <= nrow; j++)
				{
					strcat(record,azResult[j]);
				}
				memset(buf, '\0', sizeof(buf));
				sprintf(buf, "%05d", strlen(record));
				strncpy(&record[5], buf, 5);
				strcat(record, "\n");
				//发送数据
				if (-1 != send(sockfd, record, strlen(record), 0))
				{
					print2msg("Sent", record);
					item = 126;
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "UPDATE inverter_process_result SET flag=0 WHERE item=%d", item);
					my_sqlite3_exec(db, sql, zErrMsg);
				}
				//断开连接
				close(sockfd);
				sleep(1);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
		usleep(100000);
	}

	//A128上报逆变器的信号强度
		memset(sql, '\0', sizeof(sql));
		sprintf(sql, "SELECT result FROM inverter_process_result WHERE item=128 and flag=1");
		for (i = 0; i < 3; i++)
		{
			if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
			{
				if(nrow){
					//创建SOCKET
					sockfd = create_socket();
					//建立连接
					if(connect_socket(sockfd))return 1;
					//准备数据
					memset(record, '\0', sizeof(record));
					sprintf(record, "APS1300000A128AAA0%s%04d00000000000000END", ecuid, nrow);
					for(j = 1; j <= nrow; j++)
					{
						strcat(record,azResult[j]);
					}
					memset(buf, '\0', sizeof(buf));
					sprintf(buf, "%05d", strlen(record));
					strncpy(&record[5], buf, 5);
					strcat(record, "\n");
					//发送数据
					if (-1 != send(sockfd, record, strlen(record), 0))
					{
						print2msg("Sent", record);
						item = 128;
						memset(sql, '\0', sizeof(sql));
						sprintf(sql, "UPDATE inverter_process_result SET flag=0 WHERE item=%d", item);
						my_sqlite3_exec(db, sql, zErrMsg);
					}
					//断开连接
					close(sockfd);
					sleep(1);
				}
				sqlite3_free_table(azResult);
				break;
			}
			sqlite3_free_table(azResult);
			usleep(100000);
		}

	//关闭数据库
	sqlite3_close(db);
	return 0;
}


/***************************************************
 * A102：上报ECU软件版本号以及逆变器列表
 * 功能：
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_inverter_info(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char length[4] = { '\0' };
	char record[65535] = { '\0' };
	char buf[1024] = { '\0' };
	char buf2[1024] = { '\0' };
	char sql[1024] = { '\0' };
	int ECU_Inverter_Num = 0;
	int i, ret;
	int item = 102;

	/* Head */
	strcpy(record, "APS13AAAAAA102AAA0");

	/* ECU Message */
	//ECU_ID
	strcat(record, ecuid);
	//Length + ECU_Version + ECU_Version_NUM
	if (NULL == (fp = fopen("/etc/yuneng/version.conf", "r")))
	{
		perror("version.conf");
		return -1;
	}
	memset(buf, '\0', sizeof(buf));
	fgets(buf, 50, fp);
	if (10 == buf[strlen(buf) - 1])
	{
		buf[strlen(buf) - 1] = '\0';
	}
	fclose(fp);
	fp = NULL;
	memset(buf2, '\0', sizeof(buf2));
	if (NULL != (fp = fopen("/etc/yuneng/version_number.conf", "r")))
	{
		fgets(buf2, 50, fp);
		if (10 == buf2[strlen(buf2) - 1])
		{
			buf2[strlen(buf2) - 1] = '\0';
		}
		fclose(fp);
		fp = NULL;
	}
	if(strlen(buf2)){
		sprintf(length, "%02d", strlen(buf)+2+strlen(buf2));
		strcat(record, length);
		strcat(record, buf);
		strcat(record, "--");
		strcat(record, buf2);
	}
	else{
		sprintf(length, "%02d", strlen(buf));
		strcat(record, length);
		strcat(record, buf);
	}

	//打开数据库
	if (sqlite3_open("/home/database.db", &db))
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	//ECU_Inverter_Num
	strcpy(sql, "SELECT id FROM id ");
	for (i = 0; i < 3; i++)
	{
		if (!(ret = sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg)))
		{
			memset(sql, '\0', sizeof(sql));
			if (nrow)
			{
				ECU_Inverter_Num = nrow;
			}
			memset(buf, '\0', sizeof(buf));
			sprintf(buf, "%d", ECU_Inverter_Num);
			for (i = 3; i > strlen(buf); i--)
			{
				strcat(record, "A");
			}
			strcat(record, buf);
			strcat(record, timestamp);
			strcat(record, "END");
			/*Inverter Message*/
			for (i = 0; i < ECU_Inverter_Num; i++)
			{
				//Inverter_ID
				memset(buf, '\0', sizeof(buf));
				sprintf(buf, "%s", azResult[i + 1]);
				strcat(record, buf);
				memset(buf, '\0', sizeof(buf));
				//Inverter_Type
				strcpy(buf, "00");
				strcat(record, buf);
				memset(buf, '\0', sizeof(buf));
				//Inverter_version
				strcpy(buf, "00000");
				strcat(record, buf);
				//Inverter_Msg_End
				strcat(record, "END");
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}
	sqlite3_close(db);
	if(ret)
		return -1;
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%d", strlen(record));
	for (i=5; i>strlen(buf); i--)
	{
		strncpy(&record[10 - i], "A", 1);
	}
	strncpy(&record[10 - i], buf, i);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A103：设置ECU软件版本号以及逆变器列表
 * 功能：
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_inverter_id(int fd_sock, char *readbuff, int stat_flag)
{
	FILE *fp;
	char sql[100] = { '\0' };
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn, tmp;
	char *zErrMsg = 0;
	char record[65535] = { '\0' };
	char buf[1024] = { '\0' };
	char timestamp[15] = { '\0' };
	char inverter_id[999][13] = { '\0' };
	int flag = 0, num = 0, ack_flag = 0, i, j, cmd_flag = 0;
	int item = 103;

	//FLAG
	strncpy(buf, &readbuff[30], 1);
	flag = atoi(buf);
	memset(buf, '\0', sizeof(buf));
	//ECU_Inverter_Num
	strncpy(buf, &readbuff[31], 3);
	for (i = 0; i < strlen(buf); i++)
	{
		if ('A' == buf[i])
			buf[i] = '0';
	}
	num = atoi(buf);
	memset(buf, '\0', sizeof(buf));
	//Timestamp
	strncpy(timestamp, &readbuff[34], 14);
	//Inverter_ID
	for (i = 0; i < num; i++)
	{
		strncpy(inverter_id[i], &readbuff[51 + i * 15], 12);
	}
	/*配置处理*/
	if (sqlite3_open("/home/database.db", &db))
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		ack_flag = 2;
	}
	else
	{
		switch (flag) {
			case 0: {
				//0清除所有逆变器ID
				memset(sql, '\0', sizeof(sql));
				strcpy(sql, "DELETE FROM id");
				if(!my_sqlite3_exec(db, sql, zErrMsg))
					printmsg("Delete all from id");
				memset(sql, '\0', sizeof(sql));
				strcpy(sql, "DELETE FROM power");
				if(!my_sqlite3_exec(db, sql, zErrMsg))
					printmsg("Delete all from power");
				cmd_flag = 1;
				break;
			}
			case 1: {
				//1添加新的逆变器ID
				for (i = 0; i < num; i++)
				{
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "SELECT id FROM id WHERE id='%s' ", inverter_id[i]);
					for (j = 0; j < 3; j++)
					{
						if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
						{
							if (nrow)
							{
								print2msg("The ID is already exists", inverter_id[i]);
								ack_flag = 2;
							}
							else
							{
								memset(sql, '\0', sizeof(sql));
								sprintf(sql, "INSERT INTO \"id\" VALUES(%d,'%s',0)", nrow+1, inverter_id[i]);
								if(!my_sqlite3_exec(db, sql, zErrMsg))
									print2msg("Add ID in id ", inverter_id[i]);
								memset(sql, '\0', sizeof(sql));
								sprintf(sql,"INSERT INTO \"power\" (item,id,limitedresult,flag) VALUES(%d,'%s','-',%d);", nrow+1, inverter_id[i], 0);
								if(!my_sqlite3_exec(db, sql, zErrMsg))
									print2msg("Add ID in power", inverter_id[i]);
								cmd_flag = 1;
							}
							sqlite3_free_table(azResult);
							break;
						}
						sqlite3_free_table(azResult);
					}
				}
				break;
			}
			case 2: {
				//2删除部分逆变器ID
				for (i = 0; i < num; i++)
				{
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "SELECT id FROM id WHERE id='%s' ", inverter_id[i]);
					for (j = 0; j < 3; j++)
					{
						if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
						{
							if (!nrow)
							{
								print2msg("No such ID", inverter_id[i]);
								ack_flag = 2;
							}
							else
							{
								memset(sql, '\0', sizeof(sql));
								sprintf(sql, "DELETE FROM id WHERE id=%s", inverter_id[i]);
								if(!my_sqlite3_exec(db, sql, zErrMsg))
									print2msg("Delete ID from id", inverter_id[i]);
								memset(sql, '\0', sizeof(sql));
								sprintf(sql, "DELETE FROM power WHERE id=%s", inverter_id[i]);
								if(!my_sqlite3_exec(db, sql, zErrMsg))
									print2msg("Delete ID from power", inverter_id[i]);
								cmd_flag = 1;
							}
							sqlite3_free_table(azResult);
							break;
						}
						sqlite3_free_table(azResult);
					}
				}
				break;
			}
			default: {
				ack_flag = 1;
				break;
			}
		}
	}
	sqlite3_close(db);
	if(cmd_flag)
		system("killall main.exe");
	/*Head*/
	strcpy(record, "APS13AAA52A100AAA0");
	/*EMA_ECU_ACK*/
	//ECU_ID
	if (NULL == (fp = fopen("/etc/yuneng/ecuid.conf", "r")))
	{
		perror("ecuid.conf");
		return -1;
	}
	fgets(buf, 12 + 1, fp);
	fclose(fp);
	fp = NULL;
	strcat(record, buf);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A103%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	memset(buf, '\0', sizeof(buf));
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
		response_inverter_info(fd_sock, "00000000000000", 1);
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
		close(fd_sock);
		fd_sock = create_socket();
		connect_socket(fd_sock);
		response_inverter_info(fd_sock, "00000000000000", 0);
	}

	return 0;
}


/***************************************************
 * A104：上报ECU本地时间及时区
 * 功能：
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_time_zone(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	int item = 104;
	int i;

	/* Head */
	strcpy(record, "APS13AAAAAA104AAA0");

	/* ECU_Message */
	//ECU_ID
	strcat(record, ecuid);
	//ECU_Local_DateTime
	memset(buf, '\0', sizeof(buf));
	getcurrenttime(buf);
	strcat(record, buf);
	//Timestamp
	strcat(record, timestamp);
	//ECU_TIMEZONE
	if (NULL == (fp = fopen("/etc/yuneng/timezone.conf", "r")))
	{
		perror("timezone.conf");
		return -1;
	}
	memset(buf, '\0', sizeof(buf));
	fgets(buf, 50, fp);
	if (10 == buf[strlen(buf) - 1])
	{
		buf[strlen(buf) - 1] = '\0';
	}
	fclose(fp);
	fp = NULL;
	strcat(record, buf);
	//ECU_Msg_End
	strcat(record, "END");

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%d", strlen(record));
	for (i = 5; i > strlen(buf); i--)
	{
		strncpy(&record[10 - i], "A", 1);
	}
	strncpy(&record[10 - i], buf, i);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A105：设置ECU时区
 * 功能：
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_time_zone(int fd_sock, char*readbuff, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	char timestamp[15] = { '\0' };
	char timezone[50] = { '\0' };
	int ack_flag = 0, i;
	int item = 105;

	//Timestamp
	strncpy(timestamp, &readbuff[30], 14);
	//ECU_TIMEZONE
	strcpy(buf, &readbuff[44]);
	strncpy(timezone, buf, strlen(buf) - 3);
	memset(buf, '\0', sizeof(buf));
	/*配置处理*/
	snprintf(buf, sizeof(buf), "cp /usr/share/zoneinfo/%s /etc/localtime", timezone);
	if (!system(buf))
	{
//		print2msg("set timezone successfully",timezone);
		//ECU_TIMEZONE
		if (NULL == (fp = fopen("/etc/yuneng/timezone.conf", "w")))
		{
			perror("timezone.conf");
			ack_flag = 2;
			printmsg("save timezone failed");
		}
		else
		{
			fputs(timezone, fp);
			fclose(fp);
			fp = NULL;
		}
	}
	else
	{
		ack_flag = 1;
		printmsg("set timezone failed");
	}
	memset(buf, '\0', sizeof(buf));
	system("killall main.exe");
	system("killall client");
	system("killall resmonitor");
	usleep(100000);
	system("/home/applications/resmonitor &");

	/* Head */
	strcpy(record, "APS13AAA52A100AAA0");

	/* EMA_ECU_ACK */
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A105%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
		response_time_zone(fd_sock, "00000000000000", 1);
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
		close(fd_sock);
		fd_sock = create_socket();
		connect_socket(fd_sock);
		response_time_zone(fd_sock, "00000000000000", 0);
	}

	return 0;
}


/***************************************************
 * A106：上报ECU网络配置信息
 * 功能：
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_network_info(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	int Comm_Cfg_Num = 2;
	int Comm_Cfg_Type = 2;
	int Socket_Type = 0;
	char domain[50] = { '\0' };
	char ip[50] = { '\0' };
	char port1[6] = { '\0' };
	char port2[6] = { '\0' };
	char Timeout[4] = { '\0' };
	char Report_Interval[3] = { '\0' };
	int i;
	int item = 106;

	/* Head */
	strcpy(record, "APS13AAAAAA106AAA0");

	/* ECU Message */
	//ECU_ID
	if (NULL == (fp = fopen("/etc/yuneng/ecuid.conf", "r")))
	{
		perror("ecuid.conf");
		Comm_Cfg_Num--;
	}
	else
	{
		fgets(buf, 12 + 1, fp);
		fclose(fp);
		fp = NULL;
		strcat(record, buf);
//		print2msg("ECU_ID", buf);
		memset(buf, '\0', sizeof(buf));
		//Comm_Cfg_Num
		sprintf(buf, "%d", Comm_Cfg_Num);
		strcat(record, buf);
		memset(buf, '\0', sizeof(buf));
		//Timestamp
		strcat(record, timestamp);
//		print2msg("Timestamp", timestamp);
		//ECU_Msg_End
		strcat(record, "END");
	}
	/*Comm_Cfg_Type==1*/
	//查询
	if (NULL == (fp = fopen("/etc/yuneng/datacenter.conf", "r")))
	{
		perror("datacenter.conf");
		Comm_Cfg_Num--;
	}
	else
	{
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Domain=", 7))
		{
			sscanf(&buf[7], "%[^\n]", domain);
			if (strlen(domain))
			{
				Socket_Type = 1;
			}
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "IP=", 3))
		{
			sscanf(&buf[3], "%[^\n]", ip);
			if (strlen(ip))
			{
				Socket_Type = 0;
			}
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Port1=", 6))
		{
			sscanf(&buf[6], "%[^\n]", port1);
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Port2=", 6))
		{
			sscanf(&buf[6], "%[^\n]", port2);
		}
		memset(buf, '\0', sizeof(buf));
		fclose(fp);
		fp = NULL;
		if (NULL == (fp = fopen("/etc/yuneng/client.conf", "r")))
		{
			perror("client.conf");
			Comm_Cfg_Num--;
		}
		else
		{
			fgets(buf, 50, fp);
			if (!strncmp(buf, "Timeout=", 8))
			{
				sscanf(&buf[8], "%[^\n]", Timeout);
			}
			memset(buf, '\0', sizeof(buf));
			fgets(buf, 50, fp);
			if (!strncmp(buf, "Report_Interval=", 16))
			{
				sscanf(&buf[16], "%[^\n]", Report_Interval);
			}
			memset(buf, '\0', sizeof(buf));
			fclose(fp);
			fp = NULL;
			//记录
			//Comm_Cfg_Type
			sprintf(buf, "%d", 1);
			strcat(record, buf);
//			print2msg("Comm_Cfg_Type", buf);
			memset(buf, '\0', sizeof(buf));
			//Port1
			for (i = 5; i > strlen(port1); i--)
			{
				strcat(record, "A");
			}
			strcat(record, port1);
//			print2msg("Port1", port1);
			//Port2
			for (i = 5; i > strlen(port2); i--)
			{
				strcat(record, "A");
			}
			strcat(record, port2);
//			print2msg("Port2", port2);
			//Timeout
			for (i = 3; i > strlen(Timeout); i--)
			{
				strcat(record, "A");
			}
			strcat(record, Timeout);
//			print2msg("Timeout", Timeout);
			//Report_Interval
			for (i = 2; i > strlen(Report_Interval); i--)
			{
				strcat(record, "A");
			}
			strcat(record, Report_Interval);
//			print2msg("Report_Interval", Report_Interval);
			//Socket_Type
			sprintf(buf, "%d", Socket_Type);
			strcat(record, buf);
//			print2msg("Socket_Type", buf);
			memset(buf, '\0', sizeof(buf));
			//Socket_Addr
			if (0 == Socket_Type)
			{
				strcat(record, ip);
//				print2msg("Socket_Addr", ip);
			}
			if (1 == Socket_Type)
			{
				strcat(record, domain);
//				print2msg("Socket_Addr", domain);
			}
			//Comm_Msg_End
			strcat(record, "END");
		}
	}

	/*Comm_Cfg_Type==2*/
	memset(domain, '\0', sizeof(domain));
	memset(ip, '\0', sizeof(ip));
	memset(port1, '\0', sizeof(port1));
	memset(port2, '\0', sizeof(port2));
	memset(Timeout, '\0', sizeof(Timeout));
	memset(Report_Interval, '\0', sizeof(Report_Interval));
	//查询
	if (NULL == (fp = fopen("/etc/yuneng/controlclient.conf", "r")))
	{
		perror("controlclient.conf");
		Comm_Cfg_Num--;
	}
	else
	{
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Timeout=", 8))
		{
			sscanf(&buf[8], "%[^\n]", Timeout);
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Report_Interval=", 16))
		{
			sscanf(&buf[16], "%[^\n]", Report_Interval);
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Domain=", 7))
		{
			sscanf(&buf[7], "%[^\n]", domain);
			if (strlen(domain))
			{
				Socket_Type = 1;
			}
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "IP=", 3))
		{
			sscanf(&buf[3], "%[^\n]", ip);
			if (strlen(ip))
			{
				Socket_Type = 0;
			}
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Port1=", 6))
		{
			sscanf(&buf[6], "%[^\n]", port1);
		}
		memset(buf, '\0', sizeof(buf));
		fgets(buf, 50, fp);
		if (!strncmp(buf, "Port2=", 6))
		{
			sscanf(&buf[6], "%[^\n]", port2);
			if (10 == port2[strlen(port2) - 1])
			{
				port2[strlen(port2) - 1] = '\0';
			}
		}
		memset(buf, '\0', sizeof(buf));
		fclose(fp);
		fp = NULL;

		//记录
		//Comm_Cfg_Type
		sprintf(buf, "%d", 2);
		strcat(record, buf);
//		print2msg("Comm_Cfg_Type", buf);
		memset(buf, '\0', sizeof(buf));
		//Port1
		for (i = 5; i > strlen(port1); i--)
		{
			strcat(record, "A");
		}
		strcat(record, port1);
//		print2msg("Port1", port1);
		//Port2
		for (i = 5; i > strlen(port2); i--)
		{
			strcat(record, "A");
		}
		strcat(record, port2);
//		print2msg("Port2", port2);
		//Timeout
		for (i = 3; i > strlen(Timeout); i--)
		{
			strcat(record, "A");
		}
		strcat(record, Timeout);
//		print2msg("Timeout", Timeout);
		//Report_Interval
		for (i = 2; i > strlen(Report_Interval); i--)
		{
			strcat(record, "A");
		}
		strcat(record, Report_Interval);
//		print2msg("Report_Interval", Report_Interval);
		//Socket_Type
		sprintf(buf, "%d", Socket_Type);
		strcat(record, buf);
//		print2msg("Socket_Type", buf);
		memset(buf, '\0', sizeof(buf));
		//Socket_Addr
		if (0 == Socket_Type)
		{
			strcat(record, ip);
//			print2msg("Socket_Addr", ip);
		}
		if (1 == Socket_Type)
		{
			strcat(record, domain);
//			print2msg("Socket_Addr", domain);
		}
		//Comm_Msg_End
		strcat(record, "END");
	}

	sprintf(buf, "%d", strlen(record));
	for (i = 5; i > strlen(buf); i--)
	{
		strncpy(&record[10 - i], "A", 1);
	}
	strncpy(&record[10 - i], buf, i);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%d", Comm_Cfg_Num);
//	print2msg("Comm_Cfg_Num", buf);
	strncpy(&record[30], buf, 1);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A107：设置ECU通信配置
 * 功能：
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_network_info(int fd_sock, char*readbuff, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	char timestamp[15] = { '\0' };
	int num = 0, ack_flag = 0, socket_type = -1, cfg_type = 0, length = 0, i, n;
	char domain[50] = { '\0' };
	char ip[50] = { '\0' };
	char port1[6] = { '\0' };
	char port2[6] = { '\0' };
	char Timeout[4] = { '\0' };
	char Report_Interval[3] = { '\0' };
	int item = 107;

	//Comm_Cfg_Num
	strncpy(buf, &readbuff[30], 1);
	num = atoi(buf);
	memset(buf, '\0', sizeof(buf));
	//Timestamp
	strncpy(timestamp, &readbuff[31], 14);
	/*配置第一条*/
	if (1 == num || 2 == num)
	{
		//Comm_Cfg_Type
		strncpy(buf, &readbuff[48], 1);
		cfg_type = atoi(buf);
//		print2msg("Comm_Cfg_Type", buf);
		memset(buf, '\0', sizeof(buf));
		//Port1
		strncpy(port1, &readbuff[49], 5);
		deletechar('A', port1);
		deletechar('0', port1);
//		print2msg("Port1", port1);
		//Port2
		strncpy(port2, &readbuff[54], 5);
		deletechar('A', port2);
		deletechar('0', port2);
//		print2msg("Port2", port2);
		//Timeout
		strncpy(Timeout, &readbuff[59], 3);
		deletechar('A', Timeout);
		deletechar('0', Timeout);
//		print2msg("Timeout", Timeout);
		//Report_Interval
		strncpy(Report_Interval, &readbuff[62], 2);
		deletechar('A', Report_Interval);
		deletechar('0', Report_Interval);
//		print2msg("Report_Interval", Report_Interval);
		//Socket_Type
		strncpy(buf, &readbuff[64], 1);
//		print2msg("Socket_Type", buf);
		socket_type = atoi(buf);
		memset(buf, '\0', sizeof(buf));
		//Socket_Addr
		if (0 == socket_type)
		{
			sscanf(&readbuff[65], "%[^E]", ip);
			length = strlen(ip);
//			print2msg("IP", ip);
		}
		else if (1 == socket_type)
		{
			sscanf(&readbuff[65], "%[^E]", domain);
			length = strlen(domain);
//			print2msg("Domain", domain);
		}
		else
		{
			ack_flag = 1;
		}
		switch (cfg_type)
		{
			case 1: {
				if (NULL == (fp = fopen("/etc/yuneng/datacenter.conf", "r")))
				{
					perror("datacenter.conf");
					return -1;
				}
				else
				{
					while(1)
					{
						memset(buf, '\0', sizeof(buf));
						fgets(buf, sizeof(buf), fp);
						if(!strlen(buf))
							break;
						if(!strncmp(buf, "Domain", 6) && !strlen(domain))
						{
							sscanf(&buf[7], "%[^\n]", domain);
						}
						if(!strncmp(buf, "IP", 2) && !strlen(ip))
						{
							sscanf(&buf[3], "%[^\n]", ip);
						}
					}
					fclose(fp);
					fp = NULL;
				}
				if (NULL == (fp = fopen("/etc/yuneng/datacenter.conf", "w")))
				{
					perror("datacenter.conf");
					ack_flag = 2;
				}
				else if (0 == ack_flag)
				{
					fputs("Domain=", fp);
					fputs(domain, fp);
					fputs("\n", fp);
					fputs("IP=", fp);
					fputs(ip, fp);
					fputs("\n", fp);
					fputs("Port1=", fp);
					fputs(port1, fp);
					fputs("\n", fp);
					fputs("Port2=", fp);
					fputs(port2, fp);
					fputs("\n", fp);
					fclose(fp);
					fp = NULL;
				}
				if (NULL == (fp = fopen("/etc/yuneng/client.conf", "w")))
				{
					perror("client.conf");
					ack_flag = 2;
				}
				else if (0 == ack_flag)
				{
					fputs("Timeout=", fp);
					fputs(Timeout, fp);
					fputs("\n", fp);
					fputs("Report_Interval=", fp);
					fputs(Report_Interval, fp);
					fputs("\n", fp);
					fclose(fp);
					fp = NULL;
				}
				//重启client
				sprintf(buf, "killall client");
				if (!(i=system(buf)))
				{
					printmsg("Restart client");
				}
				else
				{
					printdecmsg("Restart client Failed",i);
				}
				memset(buf, '\0', sizeof(buf));
				break;
			}
			case 2: {
				if (NULL == (fp = fopen("/etc/yuneng/controlclient.conf", "r")))
				{
					perror("controlclient.conf");
					return -1;
				}
				else
				{
					while(1)
					{
						memset(buf, '\0', sizeof(buf));
						fgets(buf, sizeof(buf), fp);
						if(!strlen(buf))
							break;
						if(!strncmp(buf, "Domain", 6) && !strlen(domain))
						{
							sscanf(&buf[7], "%[^\n]", domain);
						}
						if(!strncmp(buf, "IP", 2) && !strlen(ip))
						{
							sscanf(&buf[3], "%[^\n]", ip);
						}
					}
					fclose(fp);
					fp = NULL;
				}
				if (NULL == (fp = fopen("/etc/yuneng/controlclient.conf", "w")))
				{
					perror("controlclient.conf");
					ack_flag = 2;
				}
				else if (0 == ack_flag)
				{
					fputs("Timeout=", fp);
					fputs(Timeout, fp);
					fputs("\n", fp);
					fputs("Report_Interval=", fp);
					fputs(Report_Interval, fp);
					fputs("\n", fp);
					fputs("Domain=", fp);
					fputs(domain, fp);
					fputs("\n", fp);
					fputs("IP=", fp);
					fputs(ip, fp);
					fputs("\n", fp);
					fputs("Port1=", fp);
					fputs(port1, fp);
					fputs("\n", fp);
					fputs("Port2=", fp);
					fputs(port2, fp);
					fputs("\n", fp);
					fclose(fp);
					fp = NULL;
				}
				break;
			}
			default: {
				ack_flag = 1;
				break;
			}
		}
	}
	else if (0 != num)
	{
		ack_flag = 1;
	}
	/*配置第二条*/
	memset(domain, '\0', sizeof(domain));
	memset(ip, '\0', sizeof(ip));
	memset(port1, '\0', sizeof(port1));
	memset(port2, '\0', sizeof(port2));
	memset(Timeout, '\0', sizeof(Timeout));
	memset(Report_Interval, '\0', sizeof(Report_Interval));
	if (2 == num)
	{
		//Comm_Cfg_Type
		strncpy(buf, &readbuff[68 + length], 1);
		cfg_type = atoi(buf);
//		print2msg("Comm_Cfg_Type", buf);
		memset(buf, '\0', sizeof(buf));
		//Port1
		strncpy(port1, &readbuff[69 + length], 5);
		deletechar('A', port1);
		deletechar('0', port1);
//		print2msg("Port1", port1);
		//Port2
		strncpy(port2, &readbuff[74 + length], 5);
		deletechar('A', port2);
		deletechar('0', port2);
//		print2msg("Port2", port2);
		//Timeout
		strncpy(Timeout, &readbuff[79 + length], 3);
		deletechar('A', Timeout);
		deletechar('0', Timeout);
//		print2msg("Timeout", Timeout);
		//Report_Interval
		strncpy(Report_Interval, &readbuff[82 + length], 2);
		deletechar('A', Report_Interval);
		deletechar('0', Report_Interval);
//		print2msg("Report_Interval", Report_Interval);
		//Socket_Type
		strncpy(buf, &readbuff[84 + length], 1);
//		print2msg("Socket_Type", buf);
		socket_type = atoi(buf);
		memset(buf, '\0', sizeof(buf));
		//Socket_Addr
		if (0 == socket_type)
		{
			sscanf(&readbuff[85 + length], "%[^E]", ip);
			length = strlen(ip);
//			print2msg("IP", ip);
		} else if (1 == socket_type)
		{
			sscanf(&readbuff[85 + length], "%[^E]", domain);
			length = strlen(domain);
//			print2msg("Domain", domain);
		} else {
			ack_flag = 1;
		}
		switch (cfg_type)
		{
			case 1: {
				if (NULL == (fp = fopen("/etc/yuneng/datacenter.conf", "r")))
				{
					perror("datacenter.conf");
					return -1;
				}
				else
				{
					while(1)
					{
						memset(buf, '\0', sizeof(buf));
						fgets(buf, sizeof(buf), fp);
						if(!strlen(buf))
							break;
						if(!strncmp(buf, "Domain", 6) && !strlen(domain))
						{
							sscanf(&buf[7], "%[^\n]", domain);
						}
						if(!strncmp(buf, "IP", 2) && !strlen(ip))
						{
							sscanf(&buf[3], "%[^\n]", ip);
						}
					}
					fclose(fp);
					fp = NULL;
				}
				if (NULL == (fp = fopen("/etc/yuneng/datacenter.conf", "w")))
				{
					perror("datacenter.conf");
					ack_flag = 2;
				}
				else if (0 == ack_flag)
				{
					fputs("Domain=", fp);
					fputs(domain, fp);
					fputs("\n", fp);
					fputs("IP=", fp);
					fputs(ip, fp);
					fputs("\n", fp);
					fputs("Port1=", fp);
					fputs(port1, fp);
					fputs("\n", fp);
					fputs("Port2=", fp);
					fputs(port2, fp);
					fputs("\n", fp);
					fclose(fp);
					fp = NULL;
				}
				if (NULL == (fp = fopen("/etc/yuneng/client.conf", "w")))
				{
					perror("client.conf");
					ack_flag = 2;
				}
				else if (0 == ack_flag)
				{
					fputs("Timeout=", fp);
					fputs(Timeout, fp);
					fputs("\n", fp);
					fputs("Report_Interval=", fp);
					fputs(Report_Interval, fp);
					fputs("\n", fp);
					fclose(fp);
					fp = NULL;
				}
				//重启client
				sprintf(buf, "killall client");
				if (!(i=system(buf)))
				{
					printmsg("Restart client");
				}
				else
				{
					printdecmsg("Restart client Failed",i);
				}
				memset(buf, '\0', sizeof(buf));
				break;
			}
			case 2: {
				if (NULL == (fp = fopen("/etc/yuneng/controlclient.conf", "r")))
				{
					perror("controlclient.conf");
					return -1;
				}
				else
				{
					while(1)
					{
						memset(buf, '\0', sizeof(buf));
						fgets(buf, sizeof(buf), fp);
						if(!strlen(buf))
							break;
						if(!strncmp(buf, "Domain", 6) && !strlen(domain))
						{
							sscanf(&buf[7], "%[^\n]", domain);
						}
						if(!strncmp(buf, "IP", 2) && !strlen(ip))
						{
							sscanf(&buf[3], "%[^\n]", ip);
						}
					}
					fclose(fp);
					fp = NULL;
				}
				if (NULL == (fp = fopen("/etc/yuneng/controlclient.conf", "w")))
				{
					perror("controlclient.conf");
					ack_flag = 2;
				}
				else if (0 == ack_flag)
				{
					fputs("Timeout=", fp);
					fputs(Timeout, fp);
					fputs("\n", fp);
					fputs("Report_Interval=", fp);
					fputs(Report_Interval, fp);
					fputs("\n", fp);
					fputs("Domain=", fp);
					fputs(domain, fp);
					fputs("\n", fp);
					fputs("IP=", fp);
					fputs(ip, fp);
					fputs("\n", fp);
					fputs("Port1=", fp);
					fputs(port1, fp);
					fputs("\n", fp);
					fputs("Port2=", fp);
					fputs(port2, fp);
					fputs("\n", fp);
					fclose(fp);
					fp = NULL;
				}
				break;
			}
			default: {
				ack_flag = 1;
				break;
			}
		}
	}
	/*Head*/
	strcpy(record, "APS13AAA52A100AAA0");
	/*EMA_ECU_ACK*/
	//ECU_ID
	if (NULL == (fp = fopen("/etc/yuneng/ecuid.conf", "r")))
	{
		perror("ecuid.conf");
		return -1;
	}
	fgets(buf, 12 + 1, fp);
	fclose(fp);
	fp = NULL;
	strcat(record, buf);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A107%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	memset(buf, '\0', sizeof(buf));
	print2msg("Sent", record);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
		response_network_info(fd_sock, "00000000000000", 1);
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
		close(fd_sock);
		fd_sock = create_socket();
		connect_socket(fd_sock);
		response_network_info(fd_sock, "00000000000000", 0);
	}

	return 0;
}


/***************************************************
 * A108：自定义命令
 * 功能：
 *  ECU接收EMA发来的自定义命令，在ECU上执行
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_control_cmd(int fd_sock, char *readbuff, int stat_flag)
{
	FILE *fp;
	char cmd[256]={'\0'};
	char record[65535]={'\0'};
	char buf[256]={'\0'};
	char timestamp[15] = { '\0' };
	int ack_flag = 0, i;
	int item = 108;

	//Timestamp
	strncpy(timestamp, &readbuff[30], 14);
	//cmd
	strcpy(buf,&readbuff[47]);
	strncpy(cmd, buf, strlen(buf)-3);
	printmsg(cmd);
	if(!(i=system(cmd)))
	{
		printmsg("Commond Succeed");
	}
	else
	{
		printdecmsg("Commond Faild",i);
		ack_flag = 1;
	}

	/* Head */
	strcpy(record, "APS13AAA52A100AAA0");
	/* EMA_ECU_ACK */
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A108%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A109：设置逆变器的交流保护参数
 * 功能：
 *  设置逆变器的交流保护参数
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_ac_protection(int fd_sock, char*readbuff, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};
	char record[65535] = {'\0'};
	char buf[128] = {'\0'};
	char timestamp[15] = {'\0'};
	int ack_flag = 0;
	int Minpv, Maxpv, Minpf, Maxpf, GRTime, i, ret;
	int item = 109;

	//Timestamp
	strncpy(timestamp, &readbuff[30], 14);
	//Minimum protection voltage
	strncpy(buf, &readbuff[47], 3);
	Minpv = atoi(buf);
	//Maximum protection voltage
	strncpy(buf, &readbuff[50], 3);
	Maxpv = atoi(buf);
	//Minimum protection frequency
	strncpy(buf, &readbuff[53], 3);
	Minpf = atoi(buf);
	//Maximum protection frequency
	strncpy(buf, &readbuff[56], 3);
	Maxpf = atoi(buf);
	//Grid Recovery Time
	strncpy(buf, &readbuff[59], 5);
	GRTime = atoi(buf);
	//打开数据库//
	if (sqlite3_open("/home/database.db", &db))
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	memset(sql, '\0', sizeof(sql));
	strcpy(sql, "SELECT * FROM preset ");
	for (i = 0; i < 3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
		{
			if (!nrow)
			{
				memset(sql, '\0', sizeof(sql));
				//获取版本号
				ret = get_type();
				if(2 == ret)
				{
					//MX
					sprintf(sql, "INSERT INTO preset VALUES(1, 79, 181, %d, %d, %d, %d, %d)", Minpv, Maxpv, Minpf, Maxpf, GRTime);
				}
				else if (1 == ret)
				{
					//NA
					sprintf(sql, "INSERT INTO preset VALUES(1, 150, 320, %d, %d, %d, %d, %d)", Minpv, Maxpv, Minpf, Maxpf, GRTime);
				}
				else
				{
					//SAA
					sprintf(sql, "INSERT INTO preset VALUES(1, 119, 295, %d, %d, %d, %d, %d)", Minpv, Maxpv, Minpf, Maxpf, GRTime);
				}
				printmsg(sql);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			else
			{
				sprintf(sql, "UPDATE preset SET lv2=%d, uv2=%d, lf=%d, uf=%d, rt=%d WHERE id=1", Minpv, Maxpv, Minpf, Maxpf, GRTime);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}
	sqlite3_close(db);

	if(NULL != (fp = fopen("/tmp/presetdata.conf", "w")))
	{
		fputs("1\n", fp);
		fclose(fp);
	}

	/* Head */
	strcpy(record, "APS1300052A100AAA0");
	/* EMA_ECU_ACK */
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A109%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
		response_ecu_ac_protection(fd_sock, "00000000000000", 1);
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
		close(fd_sock);
		fd_sock = create_socket();
		connect_socket(fd_sock);
		response_ecu_ac_protection(fd_sock, "00000000000000", 0);
	}

	return 0;
}


/***************************************************
 * A110：设置逆变器的最大功率
 * 功能：
 *  设置逆变器的最大功率
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_maxpower(int fd_sock, char *readbuff, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] ={'\0'};

	int maxpower;
	char inverter_no[13] = {'\0'};

	char record[65535] = {'\0'};
	char buf[100] = {'\0'};

	char timestamp[15] = {'\0'};
	int ack_flag = 0;
	int type, num, i;
	int item = 110;

	strncpy(buf, &readbuff[30], 1);
	type = atoi(buf);
	strncpy(buf, &readbuff[31], 4);
	num = atoi(buf);
	//Timestamp
	strncpy(timestamp, &readbuff[35], 14);

	if(sqlite3_open("/home/database.db", &db))
	{
		sqlite3_close(db);
		return -1;
	}
	fp = fopen("/tmp/setmaxpower.conf","w");
	if(type)
	{
		for(i=0; i<num; i++)
		{
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[64 + i*18], 3);
			maxpower = atoi(buf);
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[52 + i*18], 12);
			sprintf(sql, "UPDATE power SET limitedpower=%d,flag=1 WHERE id=%s", maxpower, buf);
			my_sqlite3_exec(db, sql, zErrMsg);
			fputs(buf, fp);
			fputs("\n", fp);
		}
	}
	else
	{
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[52], 3);
		maxpower = atoi(buf);
		sprintf(sql, "UPDATE power SET limitedpower=%d,flag=1", maxpower);
		my_sqlite3_exec(db, sql, zErrMsg);
		fputs("ALL\n", fp);
	}

	fclose(fp);
	sqlite3_close(db);

	/* Head */
	strcpy(record, "APS1300052A100AAA0");
	/* EMA_ECU_ACK */
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A110%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A111：设置逆变器的开关机
 * 功能：
 *  设置逆变器的开关机
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_turn_on_off(int fd_sock, char *readbuff, int stat_flag)
{
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] ={'\0'};

	FILE *fp;
	char record[65535] = {'\0'};
	char buf[128] = {'\0'};
	char inverter[13] = {'\0'};

	char timestamp[15] = {'\0'};
	int ack_flag = 0;

	int type, num, i, j;
	int item = 111;

	strncpy(buf, &readbuff[30], 1);
	type = atoi(buf);
	strncpy(buf, &readbuff[31], 4);
	num = atoi(buf);
	//Timestamp
	strncpy(timestamp, &readbuff[35], 14);

	if(sqlite3_open("/home/database.db", &db))
	{
		sqlite3_close(db);
		return -1;
	}

	strcpy(sql,"CREATE TABLE IF NOT EXISTS turn_on_off(id VARCHAR(256), set_flag INTEGER, primary key(id))");
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );

	if(1 == type)
	{	//全关
		sprintf(sql, "SELECT id FROM id");
		for (i = 0; i < 3; i++)
		{
			if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
			{
				for (j = 1; j <= nrow; j++)
				{
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "REPLACE INTO turn_on_off (id, set_flag) VALUES('%s', 2) ", azResult[j]);//关
					sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
				}
				sqlite3_free_table(azResult);
				break;
			}
			sqlite3_free_table(azResult);
		}
		sqlite3_close(db);
	}
	if(0 == type)
	{	//全开
		sprintf(sql, "SELECT id FROM id");
		for (i = 0; i < 3; i++)
		{
			if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
			{
				for (j = 1; j <= nrow; j++)
				{
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "REPLACE INTO turn_on_off (id, set_flag) VALUES('%s', 1) ", azResult[j]);//开
					sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
				}
				sqlite3_free_table(azResult);
				break;
			}
			sqlite3_free_table(azResult);
		}
		sqlite3_close(db);
	}
	if(2 == type)
	{
		for(i=0; i<num; i++)
		{
			memset(inverter, '\0', sizeof(buf));
			strncpy(inverter, &readbuff[52 + i*16], 12);
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[64 + i*16], 1);
			if(atoi(buf))
			{
				sprintf(sql, "REPLACE INTO turn_on_off (id, set_flag) VALUES('%s', 2)", inverter);//关
			}
			else
			{
				sprintf(sql, "REPLACE INTO turn_on_off (id, set_flag) VALUES('%s', 1)", inverter);//开
			}
			sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
		}
		sqlite3_close(db);
	}

	/* Head */
	strcpy(record, "APS1300052A100AAA0");
	/* EMA_ECU_ACK */
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A111%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A112：设置GFDI
 * 功能：
 *  设置GFDI
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	接收到的消息命令
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_gfdi(int fd_sock, char *readbuff, int stat_flag)
{
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] ={'\0'};

	char record[65535] = {'\0'};
	char buf[128] = {'\0'};

	char timestamp[15] = {'\0'};
	int ack_flag = 0;

	char inverter_no[13] = {'\0'};
	int type, num, i, j, length = 0;
	int item = 112;

	strncpy(buf, &readbuff[30], 1);
	type = atoi(buf);
	strncpy(buf, &readbuff[31], 4);
	num = atoi(buf);
	//Timestamp
	strncpy(timestamp, &readbuff[35], 14);

	if(sqlite3_open("/home/database.db", &db))
	{
		sqlite3_close(db);
		return -1;
	}
	strcpy(sql,"CREATE TABLE IF NOT EXISTS clear_gfdi(id VARCHAR(256), set_flag INTEGER, primary key(id))");
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );

	if(type)
	{
		for(i=0; i<num; i++)
		{
			memset(buf, '\0', sizeof(buf));
			sscanf(&readbuff[52 + length], "%[^E]", buf);
			length = length + strlen(buf) + 3;
			strncpy(inverter_no, buf, 12);

			sprintf(sql, "REPLACE INTO clear_gfdi (id, set_flag) VALUES('%s', 1) ", inverter_no);
			sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
			printf("%s\n", zErrMsg);
			sqlite3_close(db);
		}
	}
	else
	{
		sprintf(sql, "SELECT id FROM id");
		for (i = 0; i < 3; i++)
		{
			if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
			{
				for (j = 1; j <= nrow; j++)
				{
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "REPLACE INTO clear_gfdi (id, set_flag) VALUES('%s', 1) ", azResult[j]);
					sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
					printf("%s\n", zErrMsg);
				}
				sqlite3_free_table(azResult);
				break;
			}
			sqlite3_free_table(azResult);
		}
		sqlite3_close(db);
	}
	/*Head*/
	strcpy(record, "APS1300052A100AAA0");
	/*EMA_ECU_ACK*/
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A112%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A113：上传ECU级别的交流保护参数和范围
 * 功能：
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_ecu_ac_protection(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};

	char record[65535] = {'\0'};
	char buf[128] = {'\0'};
	int ecu_type, i;
	int item = 113;

	/* Head */
	strcpy(record, "APS1300000A113AAA0");
	/* ECU Message */
	strcat(record, ecuid);
	strcat(record, timestamp);
	strcat(record, "END");
	sqlite3_open("/home/database.db", &db);
	strcpy(sql, "SELECT * FROM preset");
	for(i=0; i<3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
		{
			if(nrow)
				sprintf(buf, "%03d%03d%03d%03d%05d", atoi(azResult[11]), atoi(azResult[12]), atoi(azResult[13]), atoi(azResult[14]), atoi(azResult[15]));
			else
				sprintf(buf, "00000000000000000");
			strcat(record, buf);
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}

	ecu_type = get_type();
	if(2 == ecu_type)
	{
		//MX
		strcat(record, "082118124155551600600649020300");
	}
	else if(1 == ecu_type)
	{
		//NA
		strcat(record, "181239221298551600600649020300");
	}
	else
	{
		//SAA
		strcat(record, "149217221278451500500549020300");
	}
	//ECU级别的最大功率
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"SELECT * FROM powerall");
	for(i=0; i<3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
		{
			if(nrow)
				sprintf(buf, "%03d", atoi(azResult[5]));
			else
				sprintf(buf, "000");
			strcat(record, buf);
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}
	sqlite3_close(db);
	strcat(record, "END");

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%05d", strlen(record));
	strncpy(&record[5], buf, 5);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A114：上传逆变器级别的交流保护参数和范围
 * 功能：
 *  上传逆变器级别的交流保护参数和范围
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_inverter_ac_protection(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};
	char record[65535] = {'\0'};
	char buf[128] = {'\0'};
	int ecu_type, i, j, num = 0;
	char prevl[10]={'\0'};
	char prevu[10]={'\0'};
	char prefl[10]={'\0'};
	char prefu[10]={'\0'};
	char prert[10]={'\0'};
	char preset[18]={'\0'};
	int item = 114;

	/*Head*/
	strcpy(record, "APS1300000A114AAA0");
	/*ECU Message*/
	strcat(record, ecuid);
	strcat(record, "0000");//number
	strcat(record, timestamp);
	strcat(record, "END");

	sqlite3_open("/home/database.db", &db);
	strcpy(sql,"SELECT id FROM id ");
	if(NULL != (fp = fopen("/etc/yuneng/presetdata.txt", "r")))
	{
		for(i=0; i<3; i++)
		{
			if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
			{
				for(j=0; j<nrow; j++)
				{
					fgets(buf, sizeof(buf), fp);
					if(!strlen(buf))
						break;
					if(buf[strlen(buf)-1])
						buf[strlen(buf)-1]='\0';
					sscanf(buf, "%[^,],%[^,],%[^,],%[^,],%s", &prevl, &prevu, &prefl, &prefu, &prert);
					if(!strcmp(prevl, "-") || !strlen(prevl))
						continue;
					else
						snprintf(preset, sizeof(preset), "%03d%03d%03d%03d%05d", atoi(prevl), atoi(prevu), atoi(prefl), atoi(prefu), atoi(prert));

					strcat(record, azResult[i+1]);
					strcat(record, preset);
					ecu_type = get_type();
					if(2 == ecu_type)
					{
						//MX
						strcat(record, "082118124155551600600649END");
					}
					else if(1 == ecu_type)
					{
						//NA
						strcat(record, "181239221298551600600649END");
					}
					else
					{
						//SAA
						strcat(record, "149217221278451500500549END");
					}
					num++;
				}
				sqlite3_free_table(azResult);
				break;
			}
			sqlite3_free_table(azResult);
		}
		fclose(fp);
	}
	sqlite3_close(db);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%04d", num);
	strncpy(&record[30], buf, 4);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%05d", strlen(record));
	strncpy(&record[5], buf, 5);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A115：上传ECU每个逆变器的GFDI状态
 * 功能：
 *  上传ECU每个逆变器的GFDI状态
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_gfdi_status(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};
	char buf[128] = {'\0'};
	char record[65535] = {'\0'};
	int i, num = 0;
	int item = 115;

	/*Head*/
	strcpy(record, "APS1300000A115AAA0");
	/*ECU Message*/
	strcat(record, ecuid);
	/*Number*/
	strcat(record, "0000");
	strcat(record, timestamp);
	strcat(record, "END");

	if(NULL != (fp = fopen("/etc/yuneng/last_gfdi_status.txt", "r")))
	{
		while(1)
		{
			memset(buf, '\0', sizeof(buf));
			fgets(buf, sizeof(buf), fp);
			if(!strlen(buf))
				break;
			if (10 == buf[strlen(buf) - 1])
			{
				buf[strlen(buf) - 1] = '\0';
			}
			strcat(record, buf);
			num++;
		}
		fclose(fp);
	}
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%04d", num);
	strncpy(&record[30], buf, 4);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%05d", strlen(record));
	strncpy(&record[5], buf, 5);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A116：上传ECU每个逆变器的开关机状态
 * 功能：
 *  上传ECU每个逆变器的开关机状态
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_on_off_status(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};
	char buf[128] = {'\0'};
	char record[65535] = {'\0'};
	int i, num = 0;
	int item = 116;

	/*Head*/
	strcpy(record, "APS1300000A116AAA0");
	/*ECU Message*/
	strcat(record, ecuid);
	/*Number*/
	strcat(record, "0000");
	strcat(record, timestamp);
	strcat(record, "END");

	if(NULL != (fp = fopen("/etc/yuneng/last_turn_on_off_status.txt", "r")))
	{
		while(1)
		{
			memset(buf, '\0', sizeof(buf));
			fgets(buf, sizeof(buf), fp);
			if(!strlen(buf))
				break;
			if (10 == buf[strlen(buf) - 1])
			{
				buf[strlen(buf) - 1] = '\0';
			}
			strcat(record, buf);
			num++;
		}
		fclose(fp);
	}
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%04d", num);
	strncpy(&record[30], buf, 4);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%05d", strlen(record));
	strncpy(&record[5], buf, 5);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A117：上传最大功率及范围
 * 功能：
 *  上传最大功率及范围
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_max_power(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};

	char buf[128] = {'\0'};
	char record[65535] = {'\0'};
	int i;
	int item = 117;

	/*Head*/
	strcpy(record, "APS1300000A117AAA0");
	/*ECU Message*/
	strcat(record, ecuid);
	sqlite3_open("/home/database.db", &db);
	strcpy(sql, "SELECT * FROM power");
	for(i=0; i<3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
		{
			sprintf(buf, "%04d", nrow);
			strcat(record, buf);
			strcat(record, timestamp);
			strcat(record, "END");

			for(i=0; i<nrow; i++)
			{
				strcat(record, azResult[8 + i*7]);
				memset(buf, 0, sizeof(buf));
				if(!strcmp("-", azResult[10 + i*7]))
					strcat(record, "000");
				else
				{
					sprintf(buf, "%03d", atoi(azResult[10 + i*7]));
					strcat(record, buf);
				}
				strcat(record, "020300");
				strcat(record, "END");
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}
	sqlite3_close(db);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%05d", strlen(record));
	strncpy(&record[5], buf, 5);
	memset(buf, '\0', sizeof(buf));
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A118：首次连接EMA上报ECU信息
 * 功能：
 *  上报
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int response_first_time_info(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;

	//A114 上传逆变器级别的交流保护参数和范围
	if(NULL != (fp = fopen("/tmp/presetdata.conf", "w")))
	{
		fputs("2", fp);
		fclose(fp);
	}

	//A117 上传逆变器的最大功率
	if(NULL != (fp = fopen("/tmp/getmaxpower.conf", "w")))
	{
		fputs("ALL", fp);
		fclose(fp);
	}

	//A102 上报ECU软件版本号以及逆变器列表
	response_inverter_info(fd_sock, timestamp, stat_flag);
	close(fd_sock);
	sleep(1);

	//A104 上报ECU本地时间以及时区
	fd_sock = create_socket();
	if(connect_socket(fd_sock))return 1;
	response_time_zone(fd_sock, timestamp, stat_flag);
	close(fd_sock);
	sleep(1);

	//A106 上报ECU的通信配置信息
	fd_sock = create_socket();
	if(connect_socket(fd_sock))return 1;
	response_network_info(fd_sock, timestamp, stat_flag);
	close(fd_sock);
	sleep(1);

	//A113
	fd_sock = create_socket();
	if(connect_socket(fd_sock))return 1;
	response_ecu_ac_protection(fd_sock, timestamp, stat_flag);
	close(fd_sock);
	sleep(1);

	//A115
	fd_sock = create_socket();
	if(connect_socket(fd_sock))return 1;
	response_gfdi_status(fd_sock, timestamp, stat_flag);
	close(fd_sock);
	sleep(1);

	//A116
	fd_sock = create_socket();
	if(connect_socket(fd_sock))return 1;
	response_on_off_status(fd_sock, timestamp, stat_flag);
	close(fd_sock);
	sleep(1);

	//A120
	fd_sock = create_socket();
	if(connect_socket(fd_sock))return 1;
	new_response_ecu_ac_protection(fd_sock, timestamp, stat_flag);

	return 0;
}


/***************************************************
 * A119：设置ECU_EMA通讯开关
 * 功能：
 *  设置ECU_EMA通讯开关
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	EMA传给ECU的设置信息
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int set_ecu_flag(int fd_sock, char *readbuff, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	char timestamp[15] = { '\0' };
	char timezone[50] = { '\0' };
	char ecu_flag[2] = {'\0'};
	int ack_flag = 0, i;
	int item = 119;

	//Timestamp
	strncpy(timestamp, &readbuff[30], 14);
	//ECU_FLAG
	strncpy(ecu_flag, &readbuff[44], 1);
	/*配置处理*/
	fp = fopen("/etc/yuneng/ecu_flag.conf", "w");
	fputs(ecu_flag, fp);
	fputs("\n", fp);
	fclose(fp);
	fp = NULL;
	/*Head*/
	strcpy(record, "APS13AAA52A100AAA0");
	/*EMA_ECU_ACK*/
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A119%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	memset(buf, '\0', sizeof(buf));
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A120：(新的)上传ECU级别的交流保护参数和范围
 * 功能：
 *  上传新的ECU级别的交流保护参数和范围
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int new_response_ecu_ac_protection(int fd_sock, char *timestamp, int stat_flag)
{
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};
	char record[65535] = {'\0'};
	char buf[128] = {'\0'};
	int ecu_type, i, j;
	int item = 120;

	/*Head*/
	strcpy(record, "APS1300000A120AAA0");
	/*ECU Message*/
	strcat(record, ecuid);
	strcat(record, timestamp);
	strcat(record, "END");
	sqlite3_open("/home/database.db", &db);
	strcpy(sql, "SELECT * FROM set_protection_parameters");
	//赋初值为0
	strcat(record, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");//74个A
	for(i=0; i<3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
		{
			if(nrow)
			{
				for(j=1; j<=nrow; j++)
				{
					if(!strcmp(azResult[j*ncolumn], "under_voltage_slow")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", atoi(azResult[j*ncolumn+1]));
						strncpy(&record[47], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "over_voltage_slow")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", atoi(azResult[j*ncolumn+1]));
						strncpy(&record[50], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "under_frequency_slow")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", (int)(10*atof(azResult[j*ncolumn+1])));
						strncpy(&record[53], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "over_frequency_slow")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", (int)(10*atof(azResult[j*ncolumn+1])));
						strncpy(&record[56], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "grid_recovery_time")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%05d", atoi(azResult[j*ncolumn+1]));
						strncpy(&record[59], buf, 5);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "under_voltage_fast")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", atoi(azResult[j*ncolumn+1]));
						strncpy(&record[97], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "over_voltage_fast")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", atoi(azResult[j*ncolumn+1]));
						strncpy(&record[100], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "under_frequency_fast")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", (int)(10*atof(azResult[j*ncolumn+1])));
						strncpy(&record[103], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "over_frequency_fast")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%03d", (int)(10*atof(azResult[j*ncolumn+1])));
						strncpy(&record[106], buf, 3);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "voltage_triptime_fast")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%02d", (int)(100*atof(azResult[j*ncolumn+1])));
						strncpy(&record[109], buf, 2);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "voltage_triptime_slow")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%04d", (int)(100*atof(azResult[j*ncolumn+1])));
						strncpy(&record[111], buf, 4);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "frequency_triptime_fast")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%02d", (int)(100*atof(azResult[j*ncolumn+1])));
						strncpy(&record[115], buf, 2);
						continue;
					}
					if(!strcmp(azResult[j*ncolumn], "frequency_triptime_slow")){
						memset(buf, '\0', sizeof(buf));
						sprintf(buf, "%04d", (int)(100*atof(azResult[j*ncolumn+1])));
						strncpy(&record[117], buf, 4);
						continue;
					}
				}
			}
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}

	ecu_type = get_type();
	if(2 == ecu_type)
	{
		//MX
		strncpy(&record[64], "082118124155551600600649020300", 30);
	}
	else if(1 == ecu_type)
	{
		//NA
		strncpy(&record[64], "181239221298551600600649020300", 30);
	}
	else
	{
		//SAA
		strncpy(&record[64], "149217221278451500500549020300", 30);
	}
	//ECU级别的最大功率
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"SELECT * FROM powerall");
	for(i=0; i<3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
		{
			if(nrow)
				sprintf(buf, "%03d", atoi(azResult[5]));
			strncpy(&record[94], buf, 3);
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}
	sqlite3_close(db);
	strcat(record, "END");
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%05d", strlen(record));
	strncpy(&record[5], buf, 5);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A121：（新的）上传逆变器级别的交流保护参数
 * 功能：
 *  上传新的逆变器级别的交流保护参数和范围
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp 	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int new_response_inverter_ac_protection(int fd_sock, char *timestamp, int stat_flag)
{
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};
	char record[65535] = {'\0'};
	char buf[1024] = {'\0'};
	int i, j;
	int item = 121;

	/* Head */
	strcpy(record, "APS1300000A121AAA0");
	/* ECU Message */
	strcat(record, ecuid);
	strcat(record, "0000");//number
	strcat(record, timestamp);
	strcat(record, "END");

	sqlite3_open("/home/database.db", &db);
	strcpy(sql,"SELECT * FROM protection_parameters");

	for(i=0; i<3; i++)
	{
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn,	&zErrMsg))
		{
			for(j=1; j<=nrow; j++)
			{
				memset(buf, '\0', sizeof(buf));
				sprintf(buf,"%s%03d%03d%03d%03d%05d%03d%03d%03d%03d%02d%04d%02d%04dEND",
						azResult[j*ncolumn],
						azResult[j*ncolumn + 4],
						azResult[j*ncolumn + 5],
						azResult[j*ncolumn + 8],
						azResult[j*ncolumn + 9],
						azResult[j*ncolumn + 14],
						azResult[j*ncolumn + 2],
						azResult[j*ncolumn + 3],
						azResult[j*ncolumn + 6],
						azResult[j*ncolumn + 7],
						azResult[j*ncolumn + 10],
						azResult[j*ncolumn + 11],
						azResult[j*ncolumn + 12],
						azResult[j*ncolumn + 13]);
				strcat(record, buf);
				strcat(record, "END");
			}
			memset(buf, '\0', sizeof(buf));
			sprintf(buf, "%04d", nrow);
			strncpy(&record[30], buf, 4);
			sqlite3_free_table(azResult);
			break;
		}
		sqlite3_free_table(azResult);
	}
	sqlite3_close(db);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%05d", strlen(record));
	strncpy(&record[5], buf, 5);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A122：（新的）设置逆变器的交流保护参数
 * 功能：
 *  设置新的逆变器的交流保护参数
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff 	EMA传给ECU的设置信息
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 *  0 成功
 *  1 失败
 ***************************************************/
int new_set_ac_protection(int fd_sock, char *readbuff, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[1024] = {'\0'};
	char record[65535] = {'\0'};
	char buf[128] = {'\0'};
	char inverter_no[13] = {'\0'};
	char timestamp[15] = {'\0'};
	int ack_flag = 0, num = 0, i = 0;
	int item = 122;

	//Timestamp
	strncpy(timestamp, &readbuff[30], 14);
	//number
	strncpy(buf, &readbuff[44], 3);
	num = atoi(buf);

	//打开数据库//
	if (sqlite3_open("/home/database.db", &db))
	{
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}

	strcpy(sql,"CREATE TABLE IF NOT EXISTS set_protection_parameters_inverter(id VARCHAR(256), parameter_name VARCHAR(256), parameter_value REAL, set_flag INTEGER, primary key(id, parameter_name))");
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );

	if(num == 0)
	{
	/* 设置全部逆变器 */
		//under_voltage_slow
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[50], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('under_voltage_slow', %d, 1)", atoi(buf));
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//over_voltage_slow
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[53], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('over_voltage_slow', %d, 1)", atoi(buf));
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//under_frequency_slow
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[56], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('under_frequency_slow', %.1f, 1)", atof(buf)/10);
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//over_frequency_slow
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[59], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('over_frequency_slow', %.1f, 1)", atof(buf)/10);
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//grid_recovery_time
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[62], 5);
		if(strncmp(buf, "AAAAA", 5)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('grid_recovery_time', %d, 1)", atoi(buf));
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//under_voltage_fast
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[67], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('under_voltage_fast', %d, 1)", atoi(buf));
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//over_voltage_fast
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[70], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('over_voltage_fast', %d, 1)", atoi(buf));
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//under_frequency_fast
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[73], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('under_frequency_fast', %.1f, 1)", atof(buf)/10);
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//over_frequency_fast
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[76], 3);
		if(strncmp(buf, "AAA", 3)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('over_frequency_fast', %.1f, 1)", atof(buf)/10);
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//voltage_triptime_fast
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[79], 2);
		if(strncmp(buf, "AA", 2)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('voltage_triptime_fast', %.2f, 1)", atof(buf)/100);
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//voltage_triptime_slow
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[81], 4);
		if(strncmp(buf, "AAAA", 4)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('voltage_triptime_slow', %.2f, 1)", atof(buf)/100);
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//frequency_triptime_fast
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[85], 2);
		if(strncmp(buf, "AA", 2)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('frequency_triptime_fast', %.2f, 1)", atof(buf)/100);
			my_sqlite3_exec(db, sql, zErrMsg);
		}
		//frequency_triptime_slow
		memset(buf, '\0', sizeof(buf));
		strncpy(buf, &readbuff[87], 4);
		if(strncmp(buf, "AAAA", 4)){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "REPLACE INTO set_protection_parameters (parameter_name, parameter_value, set_flag) VALUES('frequency_triptime_slow', %.2f, 1)", atof(buf)/100);
			my_sqlite3_exec(db, sql, zErrMsg);
		}

		sqlite3_close(db);
	}
	else
	{
	/* 设置单台或多台逆变器 */
		for(i=0; i<num; i++)
		{
			memset(inverter_no, '\0', sizeof(buf));
			strncpy(inverter_no, &readbuff[94 + i*12], 12);

			//under_voltage_slow
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[50], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'under_voltage_slow', %d, 1) ", inverter_no, atoi(buf));
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//over_voltage_slow
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[53], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'over_voltage_slow', %d, 1) ", inverter_no, atoi(buf));
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//under_frequency_slow
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[56], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'under_frequency_slow', %.1f, 1) ", inverter_no, atof(buf)/10);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//over_frequency_slow
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[59], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'over_frequency_slow', %.1f, 1) ", inverter_no, atof(buf)/10);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//grid_recovery_time
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[62], 5);
			if(strncmp(buf, "AAAAA", 5)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'grid_recovery_time', %d, 1) ", inverter_no, atoi(buf));
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//under_voltage_fast
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[67], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'under_voltage_fast', %d, 1) ", inverter_no, atoi(buf));
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//over_voltage_fast
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[70], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'over_voltage_fast', %d, 1) ", inverter_no, atoi(buf));
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//under_frequency_fast
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[73], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'under_frequency_fast', %.1f, 1) ", inverter_no, atof(buf)/10);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//over_frequency_fast
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[76], 3);
			if(strncmp(buf, "AAA", 3)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'over_frequency_fast', %.1f, 1) ", inverter_no, atof(buf)/10);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//voltage_triptime_fast
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[79], 2);
			if(strncmp(buf, "AA", 2)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'voltage_triptime_fast', %.2f, 1) ", inverter_no, atof(buf)/100);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//voltage_triptime_slow
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[81], 4);
			if(strncmp(buf, "AAAA", 4)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'voltage_triptime_slow', %.2f, 1) ", inverter_no, atof(buf)/100);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//frequency_triptime_fast
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[85], 2);
			if(strncmp(buf, "AA", 2)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'frequency_triptime_fast', %.2f, 1) ", inverter_no, atof(buf)/100);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
			//frequency_triptime_slow
			memset(buf, '\0', sizeof(buf));
			strncpy(buf, &readbuff[87], 4);
			if(strncmp(buf, "AAAA", 4)){
				memset(sql, '\0', sizeof(sql));
				sprintf(sql, "REPLACE INTO set_protection_parameters_inverter (id, parameter_name, parameter_value, set_flag) VALUES('%s', 'frequency_triptime_slow', %.2f, 1) ", inverter_no, atof(buf)/100);
				my_sqlite3_exec(db, sql, zErrMsg);
			}
		}
		sqlite3_close(db);
	}

	/* Head */
	strcpy(record, "APS1300052A100AAA0");
	/* EMA_ECU_ACK */
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A122%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
		new_response_ecu_ac_protection(fd_sock, "00000000000000", 1); //A120
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
		close(fd_sock);
		fd_sock = create_socket();
		connect_socket(fd_sock);
		new_response_ecu_ac_protection(fd_sock, "00000000000000", 0); //A120
	}

	return 0;
}


/***************************************************
 * A123 上传逆变器异常状态事件
 * 功能：
 * 	上传逆变器异常状态事件，并处理EMA下发的命令
 * 返回值：
 * 	0 成功
 ***************************************************/
int response_inverter_status()
{
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[1024] = {'\0'};
	char buff[1024] = {'\0'};
	char recvbuf[65535] = {'\0'};
	int sockfd, item, timestamp_num, msg_length, i, j, k;
	int cmd_flag = 0;//记录EMA应答消息之后是否有命令

	//创建SOCKET
	sockfd = create_socket();

	//连接服务器
	if(connect_socket(sockfd)){
		return 1;
	}

	//打开数据库
	if (sqlite3_open("/home/record.db", &db)){
		close(sockfd);
		return 2;
	}

	//查询数据
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT item,result FROM inverter_status WHERE flag=1");
	for (i = 0; i < 3; i++)
	{//读取数据库(失败时重复三遍)，获得所有flag=1的result数据及对应的item
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{//读取成功
			for(j = 1; j <= nrow; j++)
			{//逐个发送
				if (-1 != send(sockfd, azResult[j*ncolumn + 1], strlen(azResult[j*ncolumn + 1]), 0))
				{//发送成功，打印发送的数据，并准备接收EMA应答
					print2msg("Sent", azResult[j*ncolumn + 1]);
					memset(recvbuf, '\0', sizeof(recvbuf));
					if(-1 != recv_response(sockfd, recvbuf))
					{//接收成功，操作数据库
						if (!strncmp(&recvbuf[10], "A123", 4) && !strncmp(&recvbuf[18], "1", 1))
						{//将收到应答的item数据的flag标记为2
							item = atoi(azResult[j*ncolumn]);
							memset(sql, '\0', sizeof(sql));
							sprintf(sql, "UPDATE inverter_status SET flag=2 WHERE item=%d", item);
							my_sqlite3_exec(db, sql, zErrMsg);
						}
						//获取已被EMA保存的数据的时间戳个数
						memset(buff, '\0', sizeof(buff));
						strncpy(buff, &recvbuf[19], 2);
						timestamp_num = atoi(buff);
						for(k=0; k<timestamp_num; k++)
						{//将已被EMA保存的数据删除
							strncpy(buff, &recvbuf[k*14+21], 14);
							memset(sql, '\0', sizeof(sql));
							sprintf(sql, "DELETE FROM inverter_status WHERE date_time='%s'", buff);
							my_sqlite3_exec(db, sql, zErrMsg);
						}

						//处理EMA下发的命令
						msg_length = 24+14*timestamp_num;
						if(strlen(recvbuf) > msg_length)
						{//EMA应答消息之后带有命令，需要处理该命令，且发送完毕后需要询问
							cmd_flag = 1;
							execute_cmd(sockfd, &recvbuf[msg_length], 1);
						}
						else
						{//EMA应答消息之后没有命令，发送完毕后不需要询问
							cmd_flag = 0;
						}

						sleep(1);
						continue;
					}
					else
					{//接收数据失败，退出该函数
						sqlite3_free_table(azResult);
						sqlite3_close(db);
						close(sockfd);
						return 3;
					}
				}
			}
			//发送完毕，清除缓存，关闭数据库
			sqlite3_free_table(azResult);
			sqlite3_close(db);
			close(sockfd);
			if(cmd_flag == 1)
			{//询问是否还有命令(普通轮询)
				process_cmd();
			}
			return 0;
		}
		//读取失败，延时后重新读取
		sqlite3_free_table(azResult);
		usleep(100000);
	}
	//关闭数据库
	sqlite3_close(db);
	close(sockfd);
	return 4;
}


/***************************************************
 * 重新上传逆变器异常状态（A123衍生）
 * 功能：
 *	1.查询是否有flag=2的数据
 *	2.若有，发送一条“读取EMA已存数据的时间戳”的命令
 *	3.将已被EMA保存的数据删除
 *	4.删除后，将flag=2的数据改为flag=1
 * 返回值：
 * 	0 成功
 * 	1 失败
 ***************************************************/
int resend_inverter_status()
{
	FILE *fp;
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[1024] = {'\0'};
	char buf[1024] = {'\0'};
	char record[65535]= {'\0'};
	char recvbuf[65535] = {'\0'};
	int i, j;
	int sockfd;
	int timestamp_num = 0;

	if (!sqlite3_open("/home/record.db", &db))
	{//1.查询是否有flag=2的数据
		memset(sql, '\0', sizeof(sql));
		sprintf(sql, "SELECT date_time FROM inverter_status WHERE flag=2");
		for (i = 0; i < 3; i++)
		{
			if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
			{
				if(nrow < 1)
				{//如果没有flag=2的数据，则退出该函数
					sqlite3_free_table(azResult);
					return 0;
				}
				sqlite3_free_table(azResult);
				//2.发送一条“读取EMA已存数据的时间戳”的命令
				//创建SOCKET
				sockfd = create_socket();
				//建立连接
				if(connect_socket(sockfd))return 1;
				//准备数据
				strcpy(record, "APS13AAA51A123AAA0");
				strcat(record, ecuid);
				strcat(record, "000000000000000000END\n");
				//发送数据
				if (!send_msg(sockfd, record))
				{//发送成功，并准备接收EMA应答
					memset(recvbuf, '\0', sizeof(recvbuf));
					if(-1 != recv_response(sockfd, recvbuf))
					{//接收成功，操作数据库
						if (!strncmp(&recvbuf[10], "A123", 4))
						{
							memset(buf, '\0', sizeof(buf));
							strncpy(buf, &recvbuf[19], 2);
							timestamp_num = atoi(buf);
							for(j=0; j<timestamp_num; j++)
							{//3.将已被EMA保存的数据删除
								memset(buf, '\0', sizeof(buf));
								strncpy(buf, &recvbuf[j*14+21], 14);
								memset(sql, '\0', sizeof(sql));
								sprintf(sql, "DELETE FROM inverter_status WHERE date_time='%s'", buf);
								my_sqlite3_exec(db, sql, zErrMsg);
							}
							//4.将flag=2的数据改为flag=1
							memset(sql, '\0', sizeof(sql));
							sprintf(sql, "UPDATE inverter_status SET flag=1 WHERE flag=2");
							my_sqlite3_exec(db, sql, zErrMsg);
						}
					}
					else
					{//接收数据失败，退出该函数
						sqlite3_close(db);
						close(sockfd);
						return 1;
					}
				}
				//处理完毕，清除缓存，关闭数据库，退出函数
				sqlite3_close(db);
				close(sockfd);
				return 0;
			}
			//读取失败，延时后重新读取
			sqlite3_free_table(azResult);
			usleep(100000);
		}
		//关闭数据库
		sqlite3_close(db);
	}
	return 0;
}


/***************************************************
 * 查询inverter_status表（A123衍生）
 * 功能：
 * 	查询record.db数据库中的inverter_status表中
 * 	是否有flag=1的数据
 * 返回值：
 * 	0 有数据
 * 	1 没有数据
 ***************************************************/
int query_inverter_status()
{
	sqlite3 *db;
	char **azResult;
	int nrow, ncolumn;
	char *zErrMsg = 0;
	char sql[1024] = {'\0'};
	int result = 1;
	int i;

	//打开数据库
	if (sqlite3_open("/home/record.db", &db)){
		return 2;
	}

	//查询数据
	memset(sql, '\0', sizeof(sql));
	sprintf(sql, "SELECT item,result FROM inverter_status WHERE flag=1");
	for (i = 0; i < 3; i++)
	{//读取数据库(失败时重复三遍)，获得所有flag=1的result数据及对应的item
		if (SQLITE_OK == sqlite3_get_table(db, sql, &azResult, &nrow, &ncolumn, &zErrMsg))
		{//读取成功
			if(nrow)
			{//有数据
				result = 0;
			}
			break;
		}
		//读取失败，延时100毫秒后重新读取
		sqlite3_free_table(azResult);
		usleep(100000);
	}

	//关闭数据库
	sqlite3_close(db);

	return result;
}


/***************************************************
 * A124 读取电网环境
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp：	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 参数：
 * 	ack_flag：设置结果标志位(0成功; 1格式错误; 2格式正确，操作失败)
 * 返回值：
 * 	0 成功
 * 	1 失败
 ***************************************************/
int read_grid_environment(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	int item = 124;
	int i;
	int ack_flag = 0;

	//读取逆变器电网环境
	fp = fopen("/tmp/get_grid_environment.conf", "w");
	if(fp){
		fputs("ALL", fp);
		fclose(fp);
	}
	else{
		ack_flag = 1;
	}

	//向EMA发送反馈信息
	/* Head */
	strcpy(record, "APS1300052A100AAA0");

	/* EMA_ECU_ACK */
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A124%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A125 电网环境设置
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff：	EMA传给ECU的设置信息
 *  stat_flag 	标志位(是否为状态上报)
 * 参数：
 * 	ack_flag：设置结果标志位(0成功; 1格式错误; 2格式正确，操作失败)
 * 返回值：
 * 	0 成功
 * 	1 失败
 ***************************************************/
int set_grid_environment(int fd_sock, char *readbuff, int stat_flag)
{
	sqlite3 *db;
	char *zErrMsg = 0;
	char sql[128] ={'\0'};
	FILE *fp;
	char record[128] = {'\0'};
	char buf[128] = {'\0'};
	char timestamp[15] = {'\0'};
	int ack_flag = 0;
	int type, num, i;
	int item = 125;

	char inverter_id[13] = {'\0'};
	int grid_env;

	//获取设置类型：设置所有逆变器"0",设置指定逆变器"1"
	strncpy(buf, &readbuff[30], 1);
	type = atoi(buf);

	//获取指定逆变器台数
	strncpy(buf, &readbuff[31], 4);
	num = atoi(buf);

	//获取时间戳
	strncpy(timestamp, &readbuff[35], 14);

	if(type == 0){
		//设置所有逆变器，存入配置文件
		grid_env = atoi(&readbuff[52]);
		fp = fopen("/tmp/set_grid_environment.conf", "w");
		if(fp){
			memset(buf, '\0', sizeof(buf));
			sprintf(buf, "ALL,%d", grid_env);
			fputs(buf, fp);
			fclose(fp);
		}
		else{
			ack_flag = 2;
		}
	}
	else if(type == 1){
		//设置指定逆变器，存入数据库
		sqlite3_open("/home/database.db", &db);
		for(i=0; i<num; i++){
			strncpy(inverter_id, &readbuff[i*16+52], 12);
			grid_env = atoi(&readbuff[i*16+64]);
			sprintf(sql, "REPLACE INTO grid_environment "
					"(id, set_value, set_flag) VALUES('%s', %d, 1) ", inverter_id, grid_env);
			sqlite3_exec(db, sql, 0, 0, &zErrMsg);
		}
		sqlite3_close(db);
	}
	else{
		ack_flag = 1;
	}

	//向EMA发送反馈信息
	/* Head */
	strcpy(record, "APS1300052A100AAA0");

	/* EMA_ECU_ACK */
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A125%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A126 读取IRD状态
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp：	时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 参数：
 * 	ack_flag：设置结果标志位(0成功; 1格式错误; 2格式正确，操作失败)
 * 返回值：
 * 	0 成功
 * 	1 失败
 ***************************************************/
int read_ird(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	int item = 126;
	int i;
	int ack_flag = 0;

	//读取逆变器IRD
	fp = fopen("/tmp/get_ird.conf", "w");
	if(fp){
		fputs("ALL", fp);
		fclose(fp);
	}
	else{
		ack_flag = 1;
	}

	//向EMA发送反馈信息
	/* Head */
	strcpy(record, "APS1300052A100AAA0");

	/* EMA_ECU_ACK */
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A126%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A127 IRD控制设置
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff：	EMA传给ECU的设置信息
 *  stat_flag 	标志位(是否为状态上报)
 * 参数：
 * 	ack_flag：设置结果标志位(0成功; 1格式错误; 2格式正确，操作失败)
 * 返回值：
 * 	0 成功
 * 	1 失败
 ***************************************************/
int set_ird(int fd_sock, char *readbuff, int stat_flag)
{
	sqlite3 *db;
	char *zErrMsg = 0;
	char sql[128] = {'\0'};
	FILE *fp;
	char record[128] = {'\0'};
	char buf[128] = {'\0'};
	char timestamp[15] = {'\0'};
	int ack_flag = 0;
	int type, num, i;
	int item = 127;

	char inverter_id[13] = {'\0'};
	int ird;

	//获取设置类型：设置所有逆变器"0",设置指定逆变器"1"
	strncpy(buf, &readbuff[30], 1);
	type = atoi(buf);
	printf("type=%d", type);

	//获取指定逆变器台数
	strncpy(buf, &readbuff[31], 4);
	num = atoi(buf);

	//获取时间戳
	strncpy(timestamp, &readbuff[35], 14);

	if(type == 0){
		//设置所有逆变器，存入配置文件
		ird = atoi(&readbuff[52]);
		fp = fopen("/tmp/set_ird.conf", "w");
		if(fp){
			memset(buf, '\0', sizeof(buf));
			sprintf(buf, "ALL,%d", ird);
			fputs(buf, fp);
			fclose(fp);
		}
		else{
			ack_flag = 2;
		}
	}
	else if(type == 1){
		//设置指定逆变器，存入数据库
		sqlite3_open("/home/database.db", &db);
		for(i=0;i<num;i++){
			strncpy(inverter_id, &readbuff[i*16+52], 12);
			ird = atoi(&readbuff[i*16+64]);
			sprintf(sql, "REPLACE INTO ird "
					"(id, set_value, set_flag) VALUES('%s', %d, 1) ", inverter_id, ird);
			sqlite3_exec(db, sql, 0, 0, &zErrMsg);
		}
		sqlite3_close(db);
	}
	else{
		ack_flag = 1;
	}

	//向EMA发送反馈信息
	/* Head */
	strcpy(record, "APS1300052A100AAA0");

	/* EMA_ECU_ACK */
	strcat(record, ecuid);//ecuid
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A127%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A128 EMA主动查询逆变器的信号强度
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  readbuff：	EMA传给ECU的设置信息
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 * 	0 成功
 * 	1 失败
 ***************************************************/
int read_signal_strength(int fd_sock, char *readbuff, int stat_flag)
{
	FILE *fp;
	sqlite3 *db;
	char *zErrMsg = 0;
	char sql[1024] = { '\0' };
	char buf[1024] = { '\0' };
	char record[65535] = { '\0' };
	char timestamp[15] = { '\0' };
	char inverter_id[13] = {'\0'};
	int ack_flag = 0;
	int item = 128;
	int type, num, i;

	//读取类型（0：全部 1：指定）
	strncpy(buf, &readbuff[30], 1);
	type = atoi(buf);

	//时间戳
	strncpy(timestamp, &readbuff[31], 14);

	//打开数据库
	if(sqlite3_open("/home/database.db", &db))
	{//打开失败
		sqlite3_close(db);
		ack_flag = 1;
	}
	else
	{//打开成功
		if(!type)
		{//读取所有逆变器的信号强度
			if(NULL != (fp = fopen("/tmp/read_all_signal_strength.conf","w")))
			{
				fputs("ALL", fp);
				fclose(fp);
				fp = NULL;
			}
		}
		else
		{//读取指定逆变器的信号强度

			//取得逆变器数量
			memset(buf, 0, strlen(buf));
			strncpy(buf, &readbuff[48], 4);
			num = atoi(buf);

			for(i=0; i<num; i++)
			{
				//取得逆变器ID
				strncpy(inverter_id, &readbuff[i*12+52], 12);

				//将需要读取的逆变器的flag置1
				memset(sql, 0, strlen(sql));
				sprintf(sql, "REPLACE INTO signal_strength (id, set_flag) VALUES('%s', 1) ", inverter_id);
				sqlite3_exec(db, sql, 0, 0, &zErrMsg);
			}
		}
		//关闭数据库
		sqlite3_close(db);

		//在配置文件中写入标志位
		if(NULL != (fp = fopen("/tmp/upload_signal_strength.conf","w")))
		{
			fputs("1", fp);
			fclose(fp);
			fp = NULL;
		}
	}

/* Head */
	strcpy(record, "APS13AAA52A100AAA0");
/* EMA_ECU_ACK */
	//ECU_ID
	strcat(record, ecuid);
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "A128%s%dEND", timestamp, ack_flag);
	strcat(record, buf);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * A129 上报系统电网质量
 * 输入：
 *  fd_sock 	SOCKET标志符
 *  timestamp：	EMA传给ECU的设时间戳
 *  stat_flag 	标志位(是否为状态上报)
 * 返回值：
 * 	0 成功
 * 	1 失败
 ***************************************************/
int response_grid_quality(int fd_sock, char *timestamp, int stat_flag)
{
	FILE *fp;
	char record[65535] = { '\0' };
	char buf[100] = { '\0' };
	int item = 129;
	int grid_quality;
	int i;

/* Head */
	strcpy(record, "APS13AAAAAA129AAA0");

/* ECU_Message */
	//ECU_ID
	strcat(record, ecuid);

	//Grid_Quality
	if (NULL == (fp = fopen("/etc/yuneng/plc_grid_quality.txt", "r")))
	{
		printerrmsg("ecuid.conf");
		strcat(record, "0");
	}
	else
	{
		fgets(buf, sizeof(buf), fp);
		grid_quality = atoi(buf);
		fclose(fp);
		fp = NULL;
		memset(buf, 0, strlen(buf));
		sprintf(buf, "%d", grid_quality);
		strcat(record, buf);
	}

	//Timestamp
	strcat(record, timestamp);

	//ECU_Msg_End
	strcat(record, "END");

	//计算消息长度，写入消息头，在消息末尾加回车符
	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "%d", strlen(record));
	for (i = 5; i > strlen(buf); i--)
	{
		strncpy(&record[10 - i], "A", 1);
	}
	strncpy(&record[10 - i], buf, i);
	strcat(record, "\n");

	//判断标志位(选择不同处理情况)
	if(stat_flag == 1)
	{//stat_flag为1，将查询结果存入数据库
		if(save_process_result(item,record))return 1;
	}
	else
	{//普通情况，将结果发送给EMA
		if(send_msg(fd_sock, record))return 1;
	}

	return 0;
}


/***************************************************
 * 获取ECU本地时间
 * 功能：
 * 	获取ECU本地时间，计算ECU轮询时间
 * 返回值：
 * 	成功则返回秒数
 ***************************************************/
int get_ecu_time()
{
	int seconds = time((time_t*)NULL);
	return seconds;
}


/***************************************************
 * 获取ECU本地时间的小时值
 ***************************************************/
int get_hour()
{
	time_t tm;         //实例化time_t结构
	struct tm timenow; //实例化tm结构指针

	time(&tm);
	memcpy(&timenow,localtime(&tm), sizeof(timenow));
	printdecmsg("Hour:", timenow.tm_hour);

	return timenow.tm_hour;
}


/***************************************************
 * 执行EMA下发的命令
 * 功能：
 * 	解析出消息的命令号
 * 	根据命令号调用相应的函数
 * 输入：
 *  sockfd		SOCKET标志符
 *  recvbuf		以APS开头END结尾的一段消息
 *  stat_flag	标志符
 * 返回值：
 *  0 设置成功
 ***************************************************/
int execute_cmd(int sockfd, char *recvbuf, int stat_flag)
{
	char timestamp[15] = {'\0'};
	char cmd_buff[5] = {'\0'};
	int cmd_id;

	//解析消息的命令号
	strncpy(cmd_buff, &recvbuf[10], 4);
	deletechar('A', cmd_buff);
	cmd_id = atoi(cmd_buff);
	if(101 == cmd_id){
		strncpy(cmd_buff, &recvbuf[30], 4);
		deletechar('A', cmd_buff);
		cmd_id = atoi(cmd_buff);
	}

	//调用对应命令的执行函数
	switch(cmd_id)
	{
		case 100:
			//没有命令
			printmsg("No Command");
			close(sockfd);
			return 1;

		case 102:
			//上报ECU版本号及逆变器信息
			strncpy(timestamp, &recvbuf[34], 14);
			response_inverter_info(sockfd, timestamp, stat_flag);
			break;

		case 103:
			//设置ECU的逆变器列表
			set_inverter_id(sockfd, recvbuf, stat_flag);
			break;

		case 104:
			//上报ECU本地时间以及时区
			strncpy(timestamp, &recvbuf[34], 14);
			response_time_zone(sockfd, timestamp, stat_flag);
			break;

		case 105:
			//设置ECU的时区
			set_time_zone(sockfd, recvbuf, stat_flag);
			break;

		case 106:
			//上传ECU的通信配置参数
			strncpy(timestamp, &recvbuf[34], 14);
			response_network_info(sockfd, timestamp, stat_flag);
			break;

		case 107:
			//设置ECU的通信配置参数
			set_network_info(sockfd, recvbuf, stat_flag);
			break;

		case 108:
			//对ECU执行自定义命令
			set_control_cmd(sockfd, recvbuf, stat_flag);
			break;

		case 109:
			//设置逆变器的交流保护参数
			set_ac_protection(sockfd, recvbuf, stat_flag);
			break;

		case 110:
			//设置逆变器的最大功率
			set_maxpower(sockfd, recvbuf, stat_flag);
			break;

		case 111:
			//设置逆变器开关机
			set_turn_on_off(sockfd, recvbuf, stat_flag);
			break;

		case 112:
			//设置GFDI
			set_gfdi(sockfd, recvbuf, stat_flag);
			break;

		case 113:
			//上传ECU级别的交流保护参数
			strncpy(timestamp, &recvbuf[34], 14);
			response_ecu_ac_protection(sockfd, timestamp, stat_flag);
			break;

		case 114:
			//上传逆变器级别的交流保护参数
			strncpy(timestamp, &recvbuf[34], 14);
			response_inverter_ac_protection(sockfd, timestamp, stat_flag);
			break;

		case 115:
			//上传逆变器GFDI状态
			strncpy(timestamp, &recvbuf[34], 14);
			response_gfdi_status(sockfd, timestamp, stat_flag);
			break;

		case 116:
			//上传逆变器开关机状态
			strncpy(timestamp, &recvbuf[34], 14);
			response_on_off_status(sockfd, timestamp, stat_flag);
			break;

		case 117:
			//上传逆变器最大功率
			strncpy(timestamp, &recvbuf[34], 14);
			response_max_power(sockfd, timestamp, stat_flag);
			break;

		case 118:
			//首次连接EMA上报ECU信息
			strncpy(timestamp, &recvbuf[34], 14);
			response_first_time_info(sockfd, timestamp, stat_flag);
			break;

		case 119:
			//设置ECU_EMA通讯开关
			set_ecu_flag(sockfd, recvbuf, stat_flag);
			break;

		case 120:
			//（新的）上传ECU级别的交流保护参数
			strncpy(timestamp, &recvbuf[34], 14);
			new_response_ecu_ac_protection(sockfd, timestamp, stat_flag);
			break;

		case 121:
			//（新的）上传逆变器级别的交流保护参数
			strncpy(timestamp, &recvbuf[34], 14);
			new_response_inverter_ac_protection(sockfd, timestamp, stat_flag);
			break;

		case 122:
			//（新的）设置逆变器的交流保护参数
			new_set_ac_protection(sockfd, recvbuf, stat_flag);
			break;

		case 123:
			//上传逆变器异常状态事件（已独立于此循环外）
			break;

		case 124:
			//上报逆变器的电网环境（）
			strncpy(timestamp, &recvbuf[34], 14);
			read_grid_environment(sockfd, timestamp, stat_flag);
			break;

		case 125:
			//设置逆变器的电网环境
			set_grid_environment(sockfd, recvbuf, stat_flag);
			break;

		case 126:
			//上报逆变器的IRD（）
			strncpy(timestamp, &recvbuf[34], 14);
			read_ird(sockfd, timestamp, stat_flag);
			break;

		case 127:
			//设置逆变器的IRD
			set_ird(sockfd, recvbuf, stat_flag);
			break;

		case 128:
			//EMA主动查询逆变器的信号强度
			read_signal_strength(sockfd, recvbuf, stat_flag);
			break;

		case 129:
			//EMA主动查询系统的电网质量
			strncpy(timestamp, &recvbuf[34], 14);
			response_grid_quality(sockfd, timestamp, stat_flag);
			break;

		default:
			//没有该命令
			printmsg("No Such Command");
			close(sockfd);
			return 2;
	}
	return 0;
}

/***************************************************
 * 处理EMA下发的命令
 * 功能：
 *  建立SOCKET连接
 *  收发消息命令
 * 	调用执行命令函数
 * 返回值：
 *  0 设置成功
 *
 ***************************************************/
int process_cmd()
{
	int sockfd;
	char recvbuf[65535] = {'\0'};
	char timestamp[15] = {'\0'};
	int cmd_id;

//	//创建SOCKET
//	sockfd = create_socket();
//	//连接服务器
//	if(connect_socket(sockfd))return 1;

	//向EMA请求与处理命令
	while(1)
	{
		//创建SOCKET
		sockfd = create_socket();
		//连接服务器
		if(connect_socket(sockfd))return 1;

		//发送请求命令
		if(send_ack(sockfd)){
			close(sockfd);
			return 2;
		}

		//接收命令
		memset(recvbuf, '\0', sizeof(recvbuf));
		if(-1 != recv_response(sockfd, recvbuf))
		{
			//执行命令
			if(execute_cmd(sockfd, recvbuf, 0)){
				return 0;
			}
		}
		else
		{   //接收超时
			close(sockfd);
			return 0;
		}
		close(sockfd);
		sleep(1);
	}
}

/**************************************************
 * 主函数
 **************************************************/
int main(int argc, char *argv[])
{
	FILE *fp;
	char buff[1024] = {'\0'};
	int ecu_flag = 1;
	int ecu_time = 0;


	/**********************
	 * 初始化
	 *********************/
	//获取ECU的ID号
	if (NULL == (fp = fopen("/etc/yuneng/ecuid.conf", "r")))
	{
		printerrmsg("ecuid.conf");
		return -1;
	}
	fgets(ecuid, 13, fp);
	fclose(fp);
	fp = NULL;

	//获取ECU的通讯开关flag
	if(NULL != (fp = fopen("/etc/yuneng/ecu_flag.conf", "r")))
	{
		fgets(buff, sizeof(buff), fp);
		ecu_flag = atoi(buff);
		fclose(fp);
		fp = NULL;
	}

	//获取ECU轮询时间
	if (NULL != (fp = fopen("/etc/yuneng/controlclient.conf", "r")))
	{
		while(1)
		{
			memset(buff, '\0', sizeof(buff));
			fgets(buff, sizeof(buff), fp);
			if(!strlen(buff))
				break;
			if(!strncmp(buff, "Report_Interval", 15))
			{
				Minutes = atoi(&buff[16]);
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/**********************
	 * ECU轮询
	 *********************/
	while(1)
	{
		if(1 == get_hour())
		{//每天1点时向EMA确认逆变器异常状态是否被存储
			resend_inverter_status();
		}

		//查询是否有逆变器异常状态事件
		if(!query_inverter_status())
		{
			//记录ECU连接EMA时间
			ecu_time = get_ecu_time();

			//上报逆变器异常状态事件，并处理EMA下发的命令
			response_inverter_status();

			//上报处理结果
			response_result();
		}
		else if((get_ecu_time() - ecu_time) >= (Minutes * 60))
		{
			//记录本次ECU连接EMA时间
			ecu_time = get_ecu_time();

			//上报设置结果
			if(ecu_flag){
				response_result();
			}

			//处理EMA下发的命令
			process_cmd();
		}
		sleep(300);
	}
}
