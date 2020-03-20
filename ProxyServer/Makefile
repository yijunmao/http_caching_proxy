TARGETS=ProxyServer

all: $(TARGETS)
clean:
	rm -f $(TARGETS)

ProxyServer: main.cpp Cache.h Proxy.h HttpRequest.h HttpResponse.h
	g++ -g -pthread -o  $@ $<
