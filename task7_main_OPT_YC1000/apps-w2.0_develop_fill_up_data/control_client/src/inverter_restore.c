#include <stdio.h>
#include <string.h>
#include "remote_control_protocol.h"
#include "mydebug.h"
#include "mydatabase.h"
#include "myfile.h"

/* 设置指定台数逆变器的还原标志 */
int set_restore_num(const char *msg, int num)
{
	sqlite3 *db;
	int i, j, grid_env, err_count = 0;
	char inverter_id[13] = {'\0'};
	char sql[1024] = {'\0'};

	if(!open_db("/home/database.db", &db))
	{
		snprintf(sql, sizeof(sql), "CREATE TABLE IF NOT EXISTS restore_inverters(id VARCHAR(15) PRIMARY KEY, restore_result INTEGER, restore_time VARCHAR(256), restore_flag INTEGER)");
		create_table(db, sql);

		for(i=0; i<num; i++)
		{
			//获取一台逆变器的ID号
			strncpy(inverter_id, &msg[i*12], 12);

			memset(sql, 0, sizeof(sql));
			snprintf(sql, sizeof(sql), "REPLACE INTO restore_inverters "
					"(id, restore_flag) VALUES ('%s', 1) ", inverter_id, grid_env);

			//保存还原标志位
			if(replace_data(db, sql) < 0)
				err_count++;
		}
		close_db(db);
	}
	return err_count;
}

/* 【A134】EMA设置逆变器还原 */
int set_inverter_restore(const char *recvbuffer, char *sendbuffer)
{
	int ack_flag = SUCCESS;
	int type, num, grid_env;
	char timestamp[15] = {'\0'};

	//获取设置类型标志位: 0还原所有逆变器（注：目前没有还原所有这个功能），1还原指定逆变器
	type = msg_get_int(&recvbuffer[30], 1);
	//获取逆变器数量
	num = msg_get_int(&recvbuffer[31], 4);
	//获取时间戳
	strncpy(timestamp, &recvbuffer[35], 14);

	switch(type)
	{
		case 0:
			//不能还原所有逆变器
			ack_flag = UNSUPPORTED;
			break;
		case 1:
			//检查格式（逆变器数量）
			if(!msg_num_check(&recvbuffer[52], num, 12, 0)){
				ack_flag = FORMAT_ERROR;
			}
			else{
				//设置指定逆变器，存入数据库
				if(set_restore_num(&recvbuffer[52], num) > 0)
					ack_flag = DB_ERROR;
			}
			break;
		default:
			ack_flag = FORMAT_ERROR;
			break;
	}
	//拼接应答消息
	msg_ACK(sendbuffer, "A134", timestamp, ack_flag);
	return 0;
}