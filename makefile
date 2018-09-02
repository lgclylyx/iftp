CC = g++

CFLAGS = -std=c++11 -I include/

LLIB = -lpthread -lcrypt

LIB = lib/RIO.a

OBJS = main.o inet_util/inet_util.o global/global.o worker/worker.o  lock_map/lock_map.o \
		conf/conf.o log/log.o strop/strop.o 

ALL: iftp

iftp: $(OBJS)
	$(CC) -o $@ $^ $(LLIB) $(LIB)

main.o: main.cc
	$(CC) $(CFLAGS) -c -o $@ $<

log/log.o: log/log.cc
	$(CC) $(CFLAGS) -c -o $@ $< $(LLIB)

strop/strop.o: strop/strop.cc
	$(CC) $(CFLAGS) -c -o $@ $< 

#依赖strop.o
conf/conf.o: conf/conf.cc
	$(CC) $(CFLAGS) -c -o $@ $<

#依赖log.o
inet_util/inet_util.o: inet_util/inet_util.cc
	$(CC) $(CFLAGS) -c -o $@ $< 

#依赖lock_map.o, worker.o
global/global.o: global/global.cc
	$(CC) $(CFLAGS) -c -o $@ $<

lock_map/lock_map.o: lock_map/lock_map.cc
	$(CC) $(CFLAGS) -c -o $@ $< $(LLIB)

#依赖strop.o, log.o, conf.o
worker/worker.o: worker/worker.cc
	$(CC) $(CFLAGS) -c -o $@ $<

rm:
	rm $(OBJS)