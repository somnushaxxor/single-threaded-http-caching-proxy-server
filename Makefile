.PHONY: compile format clean all
sources = main.cpp FdUtils.cpp SignalHandler.cpp Proxy.cpp ClientConnectionHandler.cpp Connection.cpp CacheManager.cpp CacheEntry.cpp ServerConnectionHandler.cpp

all: compile format

compile: ${sources}
	g++ -o proxy ${sources}

format:
	clang-format -i --style=WebKit *.h *.cpp

clean:
	rm -f proxy