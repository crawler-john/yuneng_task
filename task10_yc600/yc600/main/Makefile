CC = /usr/local/sysroots/i686-arago-linux/usr/bin/arm-linux-gnueabihf-gcc
SRC = main.c debug.c zigbee.c resolve.c database.c datetime.c gprs.c display.c protocol.c remote_update.c  setpower.c save_historical_data.c check_data.c ema_control.c turn_on_off.c clear_gfdi.c set_protection_parameters.c set_protection_parameters_inverter.c set_ird.c channel.c bind_inverters.c
OBJ = main.exe
CFLAG = -I include/ -L lib/ -lsqlite3 -lpthread -lm

$(OBJ) : $(SRC)
	$(CC) -o $(OBJ) $(SRC) $(CFLAG)

strip :
	arm-linux-strip $(OBJ)
	
clean:
	rm $(OBJ)
