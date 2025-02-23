CC = g++
CFLAGS = -O3
LIBS = -lpthread
DEPS = ProxyServer.h ClientHandler.h CacheManager.h
OBJECTS = ProxyServer.o ClientHandler.o CacheManager.o proxy.o

all: proxy

proxy: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f proxy *.o