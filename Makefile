BINARY = servo_test
VPATH := ../uart1
OBJS += servo_test.o uart1.o

CFLAGS += -ggdb3 -Os -Wall -I../uart1

include ../opencm3.mk
