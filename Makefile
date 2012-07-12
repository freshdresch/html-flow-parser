
CXX = g++
CXXFLAGS = -O2 -Wall -Wextra
LDLIBS = -lpcap -lpthread
HEADERS = tcpflow.h config.h sysdep.h tcpdemux.h md5.h
OBJECTS = datalink.o flow.o main.o tcpip.o util.o md5.o
PROGRAMS = html_parser tcpflow


all: ${PROGRAMS}

debug: CXXFLAGS := ${CXXFLAGS:-O2=-O0} -ggdb
debug: ${PROGRAMS}

html_parser: html_parser.o
	${CXX} ${CXXFLAGS} -std=c++0x -o html_parser html_parser.o -lz

html_parser.o: html_parser.cpp pna.h
	${CXX} ${CXXFLAGS} -std=c++0x -c html_parser.cpp

tcpflow: ${OBJECTS}
	${CXX} ${CXXFLAGS} -o tcpflow ${OBJECTS} ${LDLIBS}

.o: ${HEADERS}
	${CXX} ${CXXFLAGS} -c ${@:.o=.cpp}

md5.o: md5.c md5.h
	gcc -c md5.c 

clean:
	rm -f ${PROGRAMS} *.o *~
