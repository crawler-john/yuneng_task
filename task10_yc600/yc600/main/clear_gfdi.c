#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"
#include "variation.h"
#include <string.h>

int get_clear_gfdi_flag(char *id)
{
	char sql[100]={'\0'};
	char *zErrMsg=0;
	char **azResult;
	int nrow=0, ncolumn=0;
	sqlite3 *db;

	sqlite3_open("/home/database.db", &db);
	strcpy(sql, "SELECT id FROM clear_gfdi LIMIT 0,1");
	sqlite3_get_table( db , sql , &azResult , &nrow , &ncolumn , &zErrMsg );
	if(1 == nrow)
	{
		strcpy(id, azResult[1]);
	}
	sqlite3_free_table( azResult );
	sqlite3_close(db);

	return nrow;
}

int clear_clear_gfdi_flag(char *id)
{
	char sql[1024]={'\0'};
	char *zErrMsg=0;
	sqlite3 *db;

	sqlite3_open("/home/database.db", &db);
	sprintf(sql, "DELETE FROM clear_gfdi WHERE id='%s'", id);
	sqlite3_exec( db , sql , 0 , 0 , &zErrMsg );
	sqlite3_close(db);

	return 0;
}

int clear_gfdi(inverter_info *firstinverter)
{
	int i, j;
	char id[256] = {'\0'};
	inverter_info *curinverter = firstinverter;

	for(j=0; j<3; j++)
	{
		curinverter = firstinverter;
		memset(id, '\0', 256);
		if(!get_clear_gfdi_flag(id))
			break;

		clear_clear_gfdi_flag(curinverter->id);
		for(i=0; (i<MAXINVERTERCOUNT)&&(12==strlen(curinverter->id)); i++)
		{
			if(!strcmp(id, curinverter->id))
			{
			//	send_clean_gfdi(curinverter);
				zb_clear_gfdi(curinverter);
				print2msg(curinverter->id, "Clear GFDI");

				break;
			}
			curinverter++;
		}
	}

	return 0;
}
