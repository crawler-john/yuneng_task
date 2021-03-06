#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "sqlite3.h"
#include "variation.h"
#include "debug.h"

extern ecu_info ecu;

sqlite3 *db=NULL;
sqlite3 *tmpdb=NULL;

int getdbsize(void){			//获取数据库的文件大小，如果数据库超过30M后，每插入一条新的数据，就删除数据库中最早的数据
	struct stat fileinfo;
	int filesize=0;

	stat("/home/record.db", &fileinfo);
	filesize=fileinfo.st_size;//filesize.st_size/1024;

	printdecmsg("Size of database", filesize);

	if(filesize >= 30000000)
		return 1;
	else
		return 0;
}

sqlite3 *init_database()			//数据库初始化
{
	char sql[1024] = {'\0'};
	char *zErrMsg = 0;

	sqlite3_open("/home/database.db", &db);	//create a database

	//建立id表：用于存储逆变器的ID号，短地址，型号，版本号，绑定标志位，ZigBee版本号，Flag标志位
	strcpy(sql,"CREATE TABLE IF NOT EXISTS id"
			"(item INTEGER, id VARCHAR(16), short_address INTEGER, "
			"model INTEGER, software_version INTEGER, "
			"bind_zigbee_flag INTEGER, turned_off_rpt_flag INTEGER, "
			"flag INTEGER, primary key(id))");
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);

	//建立Event表：用于存储逆变器status事件
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS Event"
			"(eve VARCHAR(100), device VARCHAR(20), date VARCHAR(20));");
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);

	//建立ltpower表：用于保存系统历史发电量lifetime_power
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS ltpower"
			"(item INTEGER, power REAL)");
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);

	//建立tdpower表：用于保存系统当天发电量today_power
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS tdpower"
			"(date VARCHAR(10) , todaypower REAL)");
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);

	//建立power表：用于保存逆变器的最大功率设置和固定功率设置
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS power"
			"(item INTEGER, id VARCHAR(15), "
			"limitedpower INTEGER, limitedresult INTEGER, "
			"stationarypower INTEGER, stationaryresult INTEGER, flag INTEGER)");
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);

	//建立powerall表：用于保存所有逆变器的最大功率设置和固定功率设置，以及系统最大功率设置
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS powerall"
			"(item INTEGER, maxpower INTEGER, fixedpower INTEGER, sysmaxpower INTEGER)");
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);

	//建立set_protection_parameters表：用于保存ECU级别交流保护的设置值
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS set_protection_parameters"
			"(parameter_name VARCHAR(256), parameter_value REAL, set_flag INTEGER, primary key(parameter_name))");
	sqlite3_exec_3times(db, sql);

	//建立set_protection_parameters_inverter表：用于保存逆变器级别交流保护参数的设置值
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS set_protection_parameters_inverter"
			"(id VARCHAR(256), parameter_name VARCHAR(256), parameter_value REAL, set_flag INTEGER, primary key(id, parameter_name))");
	sqlite3_exec_3times(db, sql);

	//建立ird表：用于保存逆变器的IRD设置值
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS ird"
			"(id VARCHAR(256), result INTEGER, set_value INTEGER, set_flag INTEGER, primary key(id))");
	sqlite3_exec_3times(db, sql);

	//建立clear_gfdi表：用于保存清除逆变器GFDI保护的标志位
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS clear_gfdi "
			"(id VARCHAR(256), set_flag INTEGER, primary key (id))");
	sqlite3_exec_3times(db, sql);

	//建立process_result表：用于保存ECU级别的处理结果，待发送给EMA
	memset(sql, 0, sizeof(sql));
	strcpy(sql, "CREATE TABLE IF NOT EXISTS process_result"
			"(item INTEGER, result VARCHAR, flag INTEGER, "
			"PRIMARY KEY(item))");
	sqlite3_exec_3times(db, sql);

	//建立inverter_process_result表：用于保存逆变器级别的处理结果，待发送给EMA
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS inverter_process_result"
			"(item INTEGER, inverter VARCHAR, result VARCHAR, flag INTEGER, "
			"PRIMARY KEY(item, inverter))");
	sqlite3_exec_3times(db, sql);

	//建立protection_parameters表：用于保存ECU级别交流保护参数设置值
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS protection_parameters"
			"(id VARCHAR(15), type INTEGER, "
			"under_voltage_fast INTEGER, over_voltage_fast INTEGER, "
			"under_voltage_slow INTEGER, over_voltage_slow INTEGER, "
			"under_frequency_fast REAL, over_frequency_fast REAL, "
			"under_frequency_slow REAL, over_frequency_slow REAL, "
			"voltage_triptime_fast REAL, voltage_triptime_slow REAL, "
			"frequency_triptime_fast REAL, frequency_triptime_slow REAL, "
			"grid_recovery_time INTEGER, regulated_dc_working_point INTEGER, "
			"under_voltage_stage_2 INTEGER, voltage_3_clearance_time REAL, "
			"power_factor REAL,relay_protect REAL,"
			"start_time INTEGER, set_flag INTEGER, primary key(id))");
	sqlite3_exec_3times(db, sql);

	//建立turn_on_off表：用于保存远程开关逆变器的标志位
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS turn_on_off "
			"(id VARCHAR(256), set_flag INTEGER, primary key (id))");
	sqlite3_exec_3times(db, sql);

	//建立update_inverter表：用于保存远程升级逆变器的标志位
	memset(sql, 0, sizeof(sql));
	strcpy(sql,"CREATE TABLE IF NOT EXISTS update_inverter "
			"(id VARCHAR(256), update_result INTEGER, "
			"update_time VARCHAR(256), update_flag INTEGER, "
			"primary key (id))");
	sqlite3_exec_3times(db, sql);


	return db;
}

int record_db_init()			//数据库初始化
{
	char sql[1024]={'\0'};
	sqlite3 *db;

	sqlite3_open("/home/record.db", &db);

	strcpy(sql, "CREATE TABLE IF NOT EXISTS inverter_status (item INTEGER PRIMARY KEY AUTOINCREMENT, result VARCHAR, date_time VARCHAR, flag INTEGER)");
	sqlite3_exec_3times(db, sql);

	strcpy(sql, "CREATE TABLE IF NOT EXISTS inverter_status_single (item INTEGER PRIMARY KEY AUTOINCREMENT, id VARCHAR, result VARCHAR, date_time VARCHAR, flag INTEGER)");
	sqlite3_exec_3times(db, sql);

	strcpy(sql, "CREATE TABLE IF NOT EXISTS Data(item INTEGER, record VARCHAR(74400), resendflag VARCHAR(20), date_time VARCHAR(16))");
	sqlite3_exec_3times(db, sql);

	sqlite3_close(db);

	return 0;
}

sqlite3 *init_tmpdb(inverter_info *firstinverter)
{
	int i;
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	char **azResult;
	int nrow,ncolumn;
	struct inverter_info_t *curinverter = firstinverter;

	sqlite3_open("/home/tmpdb", &tmpdb);	//create a database

	strcpy(sql,"CREATE TABLE IF NOT EXISTS powerinfo(id VARCHAR(15), preaccgen REAL, preaccgenb REAL, preaccgenc REAL, preaccgend REAL, pre_output_energy REAL, pre_output_energyb REAL, pre_output_energyc REAL, preacctime INTEGER, last_report_time VARCHAR(16), primary key(id));");	//create data table
	sqlite3_exec( tmpdb , sql , 0 , 0 , &zErrMsg );
	print2msg("Create tmpdb", zErrMsg);

	memset(sql, '\0', sizeof(sql));
	strcpy(sql, "SELECT id FROM powerinfo;");
	sqlite3_get_table( tmpdb , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
	sqlite3_free_table( azResult );
	if(0 == nrow){
		for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(curinverter->id)); i++, curinverter++){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "INSERT INTO powerinfo VALUES ('%s', 0, 0, 0, 0, 0, 0, 0, 0, '20130101000000');", curinverter->id);
			sqlite3_exec(tmpdb, sql, 0, 0, &zErrMsg);
		}

		return 0;
	}
	for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(curinverter->id)); i++, curinverter++){
		memset(sql, '\0', sizeof(sql));
		sprintf(sql, "SELECT preaccgen,preaccgenb,preaccgenc,preaccgend,pre_output_energy,pre_output_energyb,pre_output_energyc,preacctime,last_report_time FROM powerinfo WHERE id='%s';", curinverter->id);
		sqlite3_get_table( tmpdb , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
		if(1 == nrow){
			curinverter->preaccgen = atof(azResult[9]);
			curinverter->preaccgenb = atof(azResult[10]);
			curinverter->preaccgenc = atof(azResult[11]);
			curinverter->preaccgend = atof(azResult[12]);
			curinverter->pre_output_energy = atof(azResult[13]);
			curinverter->pre_output_energyb = atof(azResult[14]);
			curinverter->pre_output_energyc = atof(azResult[15]);
			curinverter->preacctime = atoi(azResult[16]);
			strcpy(curinverter->last_report_time , azResult[17]);
		}
		sqlite3_free_table( azResult );
		if(0 == nrow){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "INSERT INTO powerinfo VALUES('%s', 0, 0, 0, 0, 0, 0, 0, 0, '20130101000000');", curinverter->id);
			sqlite3_exec(tmpdb, sql, 0, 0, &zErrMsg);
			curinverter->preaccgen = 0;
			curinverter->preaccgenb = 0;
			curinverter->preaccgenc = 0;
			curinverter->preaccgend = 0;
			curinverter->pre_output_energy = 0;
			curinverter->pre_output_energyb = 0;
			curinverter->pre_output_energyc = 0;
			curinverter->preacctime = 0;
			strcpy(curinverter->last_report_time , "20130101000000");
		}
	}

	return tmpdb;
}

int update_tmpdb(inverter_info *firstinverter)
{
	int i;
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	struct inverter_info_t *curinverter = firstinverter;

	for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(curinverter->id)); i++, curinverter++){
		if(1 == curinverter->dataflag){
			memset(sql, '\0', sizeof(sql));
			sprintf(sql, "UPDATE powerinfo SET preaccgen=%f,preaccgenb=%f,preaccgenc=%f,preaccgend=%f,pre_output_energy=%f,pre_output_energyb=%f,pre_output_energyc=%f,preacctime=%d,last_report_time='%s' WHERE id='%s';", curinverter->preaccgen, curinverter->preaccgenb,curinverter->preaccgenc,curinverter->preaccgend, curinverter->pre_output_energy,curinverter->pre_output_energyb,curinverter->pre_output_energyc,curinverter->preacctime,curinverter->last_report_time, curinverter->id);
			sqlite3_exec(tmpdb, sql, 0, 0, &zErrMsg);
		}
	}
	return 0;
}

int sqlite3_exec_3times(sqlite3 *db, char *sql)
{
	int i;
	char *zErrMsg = 0;

	for(i=0; i<3; i++){
		if(SQLITE_OK == sqlite3_exec(db, sql , 0, 0, &zErrMsg))
			return 0;
		else{
			printmsg(zErrMsg);
			usleep(500000);
		}
	}

	return -1;
}

int sqlite3_exec_100times(sqlite3 *db, char *sql)
{
	int i;
	char *zErrMsg = 0;

	for(i=0; i<100; i++){
		if(SQLITE_OK == sqlite3_exec(db, sql , 0, 0, &zErrMsg))
			return 0;
		else{
			printmsg(zErrMsg);
			usleep(500000);
		}
	}

	return -1;
}

int save_inverter_parameters_result(inverter_info *inverter, int item, char *inverter_result)
{
	sqlite3 *db=NULL;
	char sql[65535];

	if(SQLITE_OK != sqlite3_open("/home/database.db", &db))
		return -1;

	if(!sqlite3_exec_3times(db, sql))
	{
		if(item==145)
			sprintf(sql, "REPLACE INTO inverter_process_result(item, inverter, result, flag) VALUES(%d, '%s', '%s', 3)", item, inverter->id, inverter_result);
		else
			sprintf(sql, "REPLACE INTO inverter_process_result(item, inverter, result, flag) VALUES(%d, '%s', '%s', 1)", item, inverter->id, inverter_result);
		sqlite3_exec_3times(db, sql);
	}
	sqlite3_close(db);

	return 0;
}


int save_inverter_parameters_result2(char *id, int item, char *inverter_result)
{
	sqlite3 *db=NULL;
	char sql[65535];

	if(SQLITE_OK != sqlite3_open("/home/database.db", &db))
		return -1;

	sprintf(sql, "REPLACE INTO inverter_process_result(item, inverter, result, flag) VALUES(%d, '%s', '%s', 1)", item, id, inverter_result);
	sqlite3_exec_3times(db, sql);

	sqlite3_close(db);

	return 0;
}


float get_lifetime_power()			//读取系统历史发电量
{
	float lifetime_power = 0.0;
	char sql[100] = {'\0'};
	char *zErrMsg = 0;
	char **azResult;
	int nrow, ncolumn, item;


	memset(sql, '\0', sizeof(sql));
	strcpy(sql,"SELECT count(power) FROM ltpower ");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
//	printf("ltpower:nrow=%d,ncolumn=%d,value=%d\n", nrow, ncolumn, atoi(azResult[1]));
	if (atoi(azResult[1]) == 0) {
		memset(sql, '\0', sizeof(sql));
		strcpy(sql,"INSERT INTO ltpower VALUES(1, 0.0)");
		sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	}
	memset(sql, '\0', sizeof(sql));
	strcpy(sql,"SELECT power FROM ltpower");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
	lifetime_power = atof(azResult[1]);
	sqlite3_free_table( azResult );
	printfloatmsg("lifetime generation", lifetime_power);

	return lifetime_power;
}

void update_life_energy(float lifetime_power)			//更新系统历史发电量
{
	char sql[100]={'\0'};
	char *zErrMsg=0;

	sprintf(sql,"UPDATE ltpower SET power=%f WHERE item=1 ",lifetime_power);
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );

	print2msg("zErrMsg",zErrMsg);
	printfloatmsg("Update lifetime power", lifetime_power);
}

int get_protect_data_from_db(char *presetdata_yc500_yc200,char *presetdata_yc1000)			//读取用户设置的配置参数值
{
	char sql[100] = {'\0'};
	char *zErrMsg=0;
	int nrow=0,ncolumn=0;
	char **azResult;
	int lv1, uv1, lv2, uv2;
	float temp;


	strcpy(sql, "SELECT * FROM protect_parameters");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );


	temp = atof(azResult[5]) * 26204.64;
	if((temp-(int)temp)>=0.5)
		lv1 = temp + 1;
	else
		lv1 = temp;


	temp = atof(azResult[6]) * 26204.64;
	if((temp-(int)temp)>=0.5)
		uv1 = temp + 1;
	else
		uv1 = temp;


	presetdata_yc500_yc200[0] = lv1/65535;
	presetdata_yc500_yc200[1] = lv1%65535/256;
	presetdata_yc500_yc200[2] = lv1%256;
	presetdata_yc500_yc200[3] = uv1/65535;
	presetdata_yc500_yc200[4] = uv1%65535/256;
	presetdata_yc500_yc200[5] = uv1%256;



	presetdata_yc500_yc200[6] = (2237500/atoi(azResult[7]))/256;
	presetdata_yc500_yc200[7] = (2237500/atoi(azResult[7]))%256;
	presetdata_yc500_yc200[8] = (2237500/atoi(azResult[8]))/256;
	presetdata_yc500_yc200[9] = (2237500/atoi(azResult[8]))%256;
	presetdata_yc500_yc200[10] = atoi(azResult[9])/256;
	presetdata_yc500_yc200[11] = atoi(azResult[9])%256;


	printhexmsg("presetdata_yc500_yc200", presetdata_yc500_yc200, 12);

	temp = atof(azResult[5]) * 11614.45;
	if((temp-(int)temp)>=0.5)
		lv2 = temp + 1;
	else
		lv2 = temp;


	temp = atof(azResult[6]) * 11614.45;
	if((temp-(int)temp)>=0.5)
		uv2 = temp + 1;
	else
		uv2 = temp;


	presetdata_yc1000[0] = lv2/65535;
	presetdata_yc1000[1] = lv2%65535/256;
	presetdata_yc1000[2] = lv2%256;
	presetdata_yc1000[3] = uv2/65535;
	presetdata_yc1000[4] = uv2%65535/256;
	presetdata_yc1000[5] = uv2%256;


	presetdata_yc1000[6] = (2560000/atoi(azResult[7]))/256;
	presetdata_yc1000[7] = (2560000/atoi(azResult[7]))%256;
	presetdata_yc1000[8] = (2560000/atoi(azResult[8]))/256;
	presetdata_yc1000[9] = (2560000/atoi(azResult[8]))%256;
	presetdata_yc1000[10] = atoi(azResult[9])/256;
	presetdata_yc1000[11] = atoi(azResult[9])%256;

	printhexmsg("presetdata_yc1000", presetdata_yc1000, 12);

	sqlite3_free_table( azResult );

	return 0;
}
/*
int save_protect_result(inverter_info *inverter, int pro_vol_l, int pro_vol_u, int pro_fre_l, int pro_fre_u, int pro_time)
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;

	sprintf(sql, "INSERT INTO protect_data_result (protect_voltage_low, protect_voltage_up, protect_frequency_low, protect_frequency_up, protect_time) VALUES (%d, %d, %f, %f, %d)", pro_vol_l, pro_vol_u, pro_fre_l, pro_fre_u, pro_time);
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	print2msg("Save protect result", zErrMsg);
}
*/

void save_record(char sendbuff[], char *date_time)			//ECU发送记录给EMA的同时，本地也保存一份
{
    char sql[1024]={'\0'};
    char db_buff[MAXINVERTERCOUNT*74+100]={'\0'};
	sqlite3 *db;
	int nrow = 0, ncolumn = 0, i=0, res;
	char **azResult;
	char *zErrMsg=0;

    sqlite3_open("/home/record.db", &db);
	if(strlen(sendbuff)>0) {
		if(1 == getdbsize()) {
			memset(sql, '\0', sizeof(sql));
			strcpy(sql, "DELETE FROM Data WHERE rowid=(SELECT MIN(rowid) FROM Data)");
			sqlite3_exec_100times(db , sql);
		}
		if(1){
			memset(sql, '\0', sizeof(sql));
			strcpy(sql, "SELECT item FROM Data");
			for(i=0;i<3;i++)
			{
				if(SQLITE_OK == sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg ))
					break;
				sleep(1);
			}
			memset(db_buff,'\0',sizeof(db_buff));
	   	 	if(nrow==0)
				sprintf(db_buff,"INSERT INTO Data (item, record, resendflag, date_time) VALUES( %d , '%s', '1', '%s');", 1, sendbuff, date_time);
	    	else
				sprintf(db_buff,"INSERT INTO Data (item, record, resendflag, date_time) VALUES( %d , '%s', '1', '%s');", atoi(azResult[nrow])+1, sendbuff, date_time);

			res=sqlite3_exec( db , db_buff , 0 , 0 , &zErrMsg );
			print2msg("Insert record into database", zErrMsg);
			if(SQLITE_OK != res){
				sleep(2);
				sqlite3_exec( db , db_buff , 0 , 0 , &zErrMsg );
				print2msg("Insert record into database again", zErrMsg);
			}

	      	sqlite3_free_table( azResult );
		}
	}
	sqlite3_close(db);
}

int get_id_from_db(inverter_info *firstinverter)	//获取逆变器的ID号
{
	char sql[100]={'\0'};
	char *zErrMsg=0;
	int nrow = 0, ncolumn = 0, nrow2 = 0, ncolumn2 = 0;
	char **azResult;
	int i;
	inverter_info *inverter = firstinverter;

	strcpy(sql,"SELECT id,short_address,model,bind_zigbee_flag,turned_off_rpt_flag FROM id");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
	print2msg("Get inverter info from database", zErrMsg);

	for(i=1;i<=nrow;i++)
	{
		strcpy(inverter->id, azResult[i*ncolumn]);
		if(NULL==azResult[i*ncolumn+1])
		{
			inverter->shortaddr = 0;		//未获取到短地址的逆变器把短地址赋值为0.ZK
		}
		else
		{
			inverter->shortaddr = atoi(azResult[i*ncolumn+1]);
		}

		if(NULL==azResult[i*ncolumn+2])
		{
			inverter->model = 0;		//未获取到机型码的逆变器把型号赋值为0.ZK
		}
		else
		{
			inverter->model = atoi(azResult[i*ncolumn+2]);
		}

		if(NULL==azResult[i*ncolumn+3])
		{
			inverter->bindflag = 0;		//未绑定的逆变器把标志位赋值为0.ZK
		}
		else
		{
			inverter->bindflag = atoi(azResult[i*ncolumn+3]);
		}

		if(NULL==azResult[i*ncolumn+4])
		{
			inverter->zigbee_version = 0;		//没有获取到ZIGBEE版本号的逆变器赋值为0.ZK
		}
		else
		{
			inverter->zigbee_version = atoi(azResult[i*ncolumn+4]);
		}
		inverter++;
	}

	inverter = firstinverter;
	printmsg("--------------");
	for(i=1; i<=nrow; i++, inverter++)
		printdecmsg(inverter->id, inverter->shortaddr);
	printmsg("--------------");
	printdecmsg("total", nrow);
	sqlite3_free_table( azResult );

	strcpy(sql,"SELECT id FROM id WHERE short_address IS NULL");
	sqlite3_get_table( db , sql , &azResult , &nrow2 , &ncolumn2 , &zErrMsg );

	printmsg("--------------");
	for(i=1; i<=nrow2; i++)
		printmsg(azResult[i*ncolumn2]);
	printmsg("--------------");
	printdecmsg("no_shortaddr", nrow2);
	sqlite3_free_table( azResult );

	return nrow;
}

int get_inverterid_only(inverter_info *firstinverter)		//获取逆变器的ID号
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	int nrow = 0, ncolumn = 0;
	char **azResult;
	int i;
	struct inverter_info_t *inverter = firstinverter;

	strcpy(sql,"SELECT id FROM id");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );

	for(i=0;i<nrow;i++)
	{
		strcpy(inverter->id, azResult[i+1]);
		inverter++;
	}
	sqlite3_free_table( azResult );

	return nrow;
}
/*int update_flag(sqlite3 *db, struct inverter_info_t *inverter)
{
	char sql[100] = {'\0'};
	char *zErrMsg=0;

	sprintf(sql,"UPDATE id SET flag=1 WHERE ID=%s ", inverter->id);
      	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
}*/

void save_inverter_id(char *inverterid,int short_addr)					//保存逆变器的ID号
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;


	sprintf(sql,"REPLACE INTO id (id, short_address) VALUES('%s', %d);",inverterid, short_addr);			//ZK
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	print2msg("Save inverter id to database",zErrMsg);
}

int resolve_and_update_inverter_protect_parameter(inverter_info *inverter, char * protect_data_result)					//解析和更新逆变器的保护参数
{
	float temp;
//	int protect_voltage_min;			//内围电压保护下限
//	int protect_voltage_max;			//内围电压保护上限
//	float protect_frequency_min;		//频率保护下限
//	float protect_frequency_max;		//频率保护上限
//	int recovery_time;					//并网恢复时间/开机时间
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	int nrow,ncolumn,i;
	char **azResult;


	if((1==inverter->model)||(2==inverter->model)||(3==inverter->model)||(4==inverter->model))	//电压保护下限
		temp = (protect_data_result[3]*65535 + protect_data_result[4]*256 + protect_data_result[5])/26204.64;

	if((5==inverter->model)||(6==inverter->model))
		temp = (protect_data_result[3]*65535 + protect_data_result[4]*256 + protect_data_result[5])/11614.45;

	if((temp-(int)temp)>0.5)
		inverter->protect_voltage_min = (int)temp +1;
	else
		inverter->protect_voltage_min = (int)temp;

	if((1==inverter->model)||(2==inverter->model)||(3==inverter->model)||(4==inverter->model))	//电压保护上限
		temp = (protect_data_result[6]*65535 + protect_data_result[7]*256 + protect_data_result[8])/26204.64;
	if((5==inverter->model)||(6==inverter->model))
		temp = (protect_data_result[6]*65535 + protect_data_result[7]*256 + protect_data_result[8])/11614.45;
	if((temp-(int)temp)>0.5)
		inverter->protect_voltage_max = (int)temp +1;
	else
		inverter->protect_voltage_max = (int)temp;

	if((1==inverter->model)||(2==inverter->model)||(3==inverter->model)||(4==inverter->model))
		inverter->protect_frequency_min = 223750.0/(protect_data_result[9]*256 + protect_data_result[10]);
	if((5==inverter->model)||(6==inverter->model))
		inverter->protect_frequency_min = 256000.0/(protect_data_result[9]*256 + protect_data_result[10]);



	if((1==inverter->model)||(2==inverter->model)||(3==inverter->model)||(4==inverter->model))
		inverter->protect_frequency_max = 223750.0/(protect_data_result[11]*256 + protect_data_result[12]);
	if((5==inverter->model)||(6==inverter->model))
		inverter->protect_frequency_max = 256000.0/(protect_data_result[11]*256 + protect_data_result[12]);

	inverter->recovery_time = protect_data_result[13]*256 + protect_data_result[14];

	sprintf(sql,"UPDATE id SET actual_min_protect_voltage=%d,actual_max_protect_voltage=%d,actual_min_protect_frequency=%.1f,actual_max_protect_frequency=%.1f,actual_boot_time=%d WHERE id='%s' ",inverter->protect_voltage_min,inverter->protect_voltage_max,inverter->protect_frequency_min,inverter->protect_frequency_max,inverter->recovery_time,inverter->id);


	if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
	{
		print2msg("update_inverter_protect_parameter",zErrMsg);
		return 1;
	}
	else
	{
		print2msg("update_inverter_protect_parameter",zErrMsg);
		return -1;
	}

}

int update_inverter_addr(char *inverterid,int short_addr)					//更新填充逆变器的短地址
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	int nrow,ncolumn,i;
	char **azResult;

	sprintf(sql,"UPDATE id SET short_address=%d WHERE id='%s' ",short_addr,inverterid);
	if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
		return 1;
	else
		return -1;

}

void update_inverter_model_version(inverter_info *inverter)					//更新逆变器的机型码和软件版本号
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;

	sprintf(sql,"UPDATE id SET model=%d,software_version=%d WHERE id='%s' ",inverter->model,inverter->version,inverter->id);
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	sprintf(sql,"UPDATE update_inverter SET update_result=%d WHERE id='%s' ", inverter->version, inverter->id);
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	print2msg("update_inverter_model_version",zErrMsg);

}

void update_inverter_bind_flag(inverter_info *inverter)					//更新逆变器的绑定标志位
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;

	sprintf(sql,"UPDATE id SET bind_zigbee_flag=%d WHERE id='%s' ",1,inverter->id);
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	print2msg("update_inverter_bind_flag",zErrMsg);

}

void update_zigbee_version(inverter_info *inverter)					//更新zigbee的版本号
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;

	sprintf(sql,"UPDATE id SET turned_off_rpt_flag=%d WHERE id='%s' ",inverter->zigbee_version,inverter->id);
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	print2msg("update_inverter_zigbee_version",zErrMsg);

}

//更新逆变器的绑定标志位
void update_bind_zigbee_flag(int short_addr)
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;

	sprintf(sql, "UPDATE id SET bind_zigbee_flag=%d WHERE short_address=%d ", 1, short_addr);
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);
}

//更新ZigBee的版本号
void update_turned_off_rpt_flag(int short_addr, int zigbee_version)
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;

	sprintf(sql, "UPDATE id SET turned_off_rpt_flag=%d WHERE short_address=%d ", zigbee_version, short_addr);
	sqlite3_exec(db, sql, 0, 0, &zErrMsg);
}

void update_today_energy(float currentpower)			//更新系统当天的发电量，每次把当前一轮的发电量传入，把当天的发电量读出，相加后再存入数据库
{
	char sql[100]={'\0'};
	char *zErrMsg=0;
	int nrow,ncolumn,i;
	char **azResult;
	char date[10]={'\0'};
	float todaypower=0;


	getdate(date);
	strcpy(sql,"SELECT * FROM tdpower");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );

	if(nrow==0)
	{
		memset(sql,'\0',100);
		sprintf(sql,"INSERT INTO \"tdpower\" VALUES('%s',%f);",date,currentpower);
		sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
		print2msg("zErrMsg",zErrMsg);

	}
	else if(!strcmp(azResult[2],date))
	{
		todaypower=atof(azResult[3])+currentpower;
		memset(sql,'\0',100);
		sprintf(sql,"UPDATE tdpower SET todaypower=%f WHERE date='%s' ",todaypower,date);
		sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
		print2msg("zErrMsg",zErrMsg);
	}
	else
	{
		memset(sql,'\0',100);
		sprintf(sql,"DELETE FROM tdpower WHERE date = '%s'",azResult[2]);//
		sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
		memset(sql,'\0',100);
		sprintf(sql,"INSERT INTO \"tdpower\" VALUES('%s',%f);",date,currentpower);
		sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
		print2msg("zErrMsg",zErrMsg);
	}
	sqlite3_free_table( azResult );
}

/*int saverecord(char sendbuff[])
{
	insert_record(sendbuff);//database;
	
	return 0;
}*/

int saveevent(inverter_info *inverter, char *sendcommanddatatime)			//保存系统当前一轮出现的7种事件
{
	int i=0;
	char event_buff[100]={'\0'};
	char datatime[20]={'\0'};
	char *zErrMsg=0;
	char eventstring[20] = {'0'};
	char sql[1024]={'\0'};
	int nrow=0,ncolumn=0;
	char **azResult;
    sqlite3 *db;

    sqlite3_open("/home/database.db", &db);
	strcpy(sql, "SELECT COUNT(device) FROM event");

	for(i=0;i<3;i++)
	{
		if(SQLITE_OK == sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg ))
		{
			printf("SELECT COUNT(device): %s\n", zErrMsg);
			printf("nrow: %d\n", nrow);
			if(nrow > 0)
			{
				printf("result: %s\n", azResult[1]);
				if(atoi(azResult[1]) > 10000)
				{
					memset(sql, '\0', sizeof(sql));
					sprintf(sql, "DELETE FROM event WHERE date IN (SELECT date FROM event ORDER BY date LIMIT 1)");
					for(i=0;i<3;i++)
					{
						if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
						{
							printf("DELETE event: %s\n", zErrMsg);
							break;
						}
						else
							printf("DELETE event: %s\n", zErrMsg);
						sleep(1);
					}
				}
			}
			sqlite3_free_table(azResult);
			break;
		}
		printf("SELECT COUNT(device): %s\n", zErrMsg);
		sqlite3_free_table(azResult);
		sleep(1);
	}

	datatime[0] = sendcommanddatatime[0];
	datatime[1] = sendcommanddatatime[1];
	datatime[2] = sendcommanddatatime[2];
	datatime[3] = sendcommanddatatime[3];
	datatime[4] = '-';
	datatime[5] = sendcommanddatatime[4];
	datatime[6] = sendcommanddatatime[5];
	datatime[7] = '-';
	datatime[8] = sendcommanddatatime[6];
	datatime[9] = sendcommanddatatime[7];
	datatime[10] = ' ';
	
	datatime[11] = sendcommanddatatime[8];
	datatime[12] = sendcommanddatatime[9];
	datatime[13] = ':';
	datatime[14] = sendcommanddatatime[10];
	datatime[15] = sendcommanddatatime[11];
	datatime[16] = ':';
	datatime[17] = sendcommanddatatime[12];
	datatime[18] = sendcommanddatatime[13];
	
	for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(inverter->id)); i++){
		if(1 == inverter->dataflag)
		{
			if(0 != strcmp(inverter->status_web, "0000000000000000000000000"))
			{
				memset(event_buff, '\0', 100);
				sprintf(event_buff,"INSERT INTO event VALUES( '%s' , '%s' , '%s' );", inverter->status_web, inverter->id, datatime);
				sqlite3_exec( db , event_buff , 0 , 0 , &zErrMsg );
			}
		}
		inverter++;
	}

	sqlite3_close(db);
	return 0;
}

int save_process_result(int item, char *result)	//设置保护参数，功率等完成后，把结果保存到数据库中，control_client把结果发送到EMA
{
	sqlite3 *db;
	char *zErrMsg=0;
	char sql[1024];
	int i;

	sqlite3_open("/home/database.db", &db);

	if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg )) {
		memset(sql, '\0', sizeof(sql));
		sprintf(sql, "REPLACE INTO process_result (item, result, flag) VALUES(%d, '%s', 1)", item, result);
		for(i=0; i<3; i++) {
			if(SQLITE_OK == sqlite3_exec( db , sql , 0 , 0 , &zErrMsg ))
				break;
			else
				print2msg("INSERT INTO process_result", zErrMsg);
			sleep(1);
		}
	}
	else
		print2msg("CREATE TABLE process_result", zErrMsg);

	sqlite3_close(db);
	return 0;
}

int save_status(char *result, char *date_time)	//设置保护参数，功率等完成后，把结果保存到数据库中，control_client把结果发送到EMA
{
	sqlite3 *db;
	char *zErrMsg=0;
	char sql[65535]={'\0'};
	int i;

	sqlite3_open("/home/record.db", &db);

	//建立inverter_status表：用于保存逆变器的异常状态
	strcpy(sql,"CREATE TABLE IF NOT EXISTS inverter_status"
			"(item INTEGER PRIMARY KEY AUTOINCREMENT, result VARCHAR, date_time VARCHAR, flag INTEGER)");
	sqlite3_exec_3times(db, sql);

	memset(sql, 0, sizeof(sql));
	strcpy(sql, "DELETE FROM inverter_status WHERE item<(SELECT MAX(item)-10000 FROM inverter_status)");
	sqlite3_exec_3times(db, sql);

	memset(sql ,'\0', sizeof(sql));
	sprintf(sql, "INSERT INTO inverter_status (result, date_time, flag) VALUES('%s', '%s', 1)", result, date_time);
	sqlite3_exec_3times(db, sql);

	sqlite3_close(db);
	return 0;
}
