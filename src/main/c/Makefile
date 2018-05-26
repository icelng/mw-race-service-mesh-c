BIN_DIR = ./bin
SRC_DIR = ./src
SRC_CORE_FILES = $(wildcard $(SRC_DIR)/core/*.c)
SRC_COM_FILES = $(wildcard $(SRC_DIR)/com/*.c)
DEPENDECE = $(SRC_COM_FILES:.c=.d)
DEPENDECE += $(SRC_CORE_FILES:.c=.d)
OBJS = $(patsubst %.c, %.o, $(SRC_COM_FILES) $(SRC_CORE_FILES))

INC_DIR += -I ./inc/core
INC_DIR += -I ./inc/com

CFLAGS +=  $(INC_DIR)
LINKS = -pthread -lrt

TARGET = server

$(TARGET):$(OBJS)
	gcc -o $(BIN_DIR)/$(TARGET) $(OBJS) $(LINKS)

include $(DEPENDECE)

%.d:%.c
	-@set -e;rm -f $@;\
	$(CC) -M $(CFLAGS) $< > $@.$$$$;\
	sed 's,\($*\)\.o[:]*,\1.o $@ :,g' < $@.$$$$ > $@;\
	rm -f $@.*

.PHONY:clean

clean:
	-rm -f $(OBJS) $(DEPENDECE) $(BIN_DIR)/$(TARGET)
