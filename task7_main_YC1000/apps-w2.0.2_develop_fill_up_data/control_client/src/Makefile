##编译器的名称
#CC = arm-linux-gcc
CC = /usr/local/sysroots/i686-arago-linux/usr/bin/arm-linux-gnueabihf-gcc

##输出可执行文件名称
output = control_client

##程序源文件:当前目录下的所有.c文件
src = $(wildcard *.c)

##链接文件
obj = $(src:%.c=%.o)

##编译器的编译选项
CFLAGS = -I ../include/

##连接器的库文件路径
#LDFLAGS = -L ../lib/arm-linux-gcc
LDFLAGS = -L ../lib/arm-linux-gnueabihf-gcc

##需要链接的库
LIBS = -lsqlite3

$(output) : $(obj)
	$(CC) -o $(output) $(obj) $(LDFLAGS) $(LIBS)
	rm *.o

.PHONY : clean
clean :
	rm $(output) $(obj)
