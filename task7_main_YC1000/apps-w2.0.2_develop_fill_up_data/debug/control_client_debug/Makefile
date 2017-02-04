CC = arm-linux-gnueabihf-gcc
src = control_client.c
obj = control_client
CFLAGS = -lsqlite3 -ldl -lpthread -I include/ -L lib/

$(obj):$(src)
	$(CC) -o $(obj) $(src) $(CFLAGS)
control_client_monitor : control_client_monitor.c
	$(CC) -o control_client_monitor control_client_monitor.c

strip:
	arm-linux-gnueabihf-strip $(obj) control_client_monitor

clean:
	rm $(obj) control_client_monitor
