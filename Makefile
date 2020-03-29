CC := g++
CXX := g++
CFLAGS := -std=c++17 -Og -Wall -g -MMD -DDEBUG
CXXFLAGS := -std=c++17 -Og -Wall -g -MMD -DDEBUG

all: ptunnel

ptunnel: ptunnel.o connect.o iomanage.o log.o network.o proxy.o socks5.o tea.o

test-tea: test-tea.o tea.o

clean:
	rm -f *.o *.d

distclean: clean
	rm -f ptunnel test-tea

-include *.d
