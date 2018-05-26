BIN_DIR = .
SRC_DIR_COM = ../../../src/com
SRC_DIR_CORE = ../../../src/core
SRC_FILES = $(wildcard ./*.c)
SRC_FILES += $(SRC_DIR_COM)/encdec.c
OBJS = $(patsubst %.c, %.o, $(SRC_FILES))

INC_DIR += -I ../../../inc/core
INC_DIR += -I ../../../inc/com
INC_DIR += -I /usr/include/mysql

LINKS = -lssl -lcrypto -L/usr/lib/mysql -lmysqlclient -pthread -lrt
CFLAGS += $(INC_DIR)
CFLAGS += $(LINKS)

TARGET = client

$(TARGET):$(OBJS)
	gcc -o $(BIN_DIR)/$(TARGET) $(OBJS) $(LINKS)

$(OBJS):%.o:%.c
	gcc -c $(CFLAGS) $< -o $@

clean:
	-rm -f $(OBJS) $(BIN_DIR)/$(TARGET)
