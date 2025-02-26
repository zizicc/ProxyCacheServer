CC = g++
CFLAGS = -O3
LIBS = -lpthread
DEPS = ClientHandler.h FakeCache.h HttpRequest.h HttpResponse.h Logger.h ProxyServer.h RequestHandler.h 
OBJECTS = ClientHandler.o FakeCache.o HttpRequest.o HttpResponse.o Logger.o ProxyServer.o RequestHandler.o proxy.o

all: proxy

proxy: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f proxy *.o