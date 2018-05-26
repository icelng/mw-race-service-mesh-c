src_files = $(wildcard *.c)
objs = $(src_files:.c=.o)
dependece = $(src_files:.c=.d)

inc += ../../inc/core
inc += ../../inc/com
inc += /usr/include/mysql
inc += ../../lib/cJSON

CFLAGS += -I $(inc)
LINKS = -lssl -lcrypto -L/usr/lib/mysql -lmysqlclient -pthread -lrt

server:$(objs)
	gcc -o server $(objs) $(LINKS)

include $(dependece)

%.d:%.c
	-@set -e;rm -f $@;\
	$(CC) -M $(CFLAGS) $< > $@.$$$$;\
	sed 's,\($*\)\.o[:]*,\1.o $@ :,g' < $@.$$$$ > $@;\
	rm -f $@.$$$$

.PHONY:clean

clean:
	-rm -f $(objs) $(dependece) server
