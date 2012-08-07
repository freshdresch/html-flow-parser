
CXX = g++
CXXFLAGS = -O2 -Wall 
LDLIBS = -lpcap -lpthread
HEADERS = tcpflow.h config.h sysdep.h tcpdemux.h md5.h
OBJECTS = datalink.o flow.o main.o tcpip.o util.o md5.o
PROGRAMS = html_flow_parser tcpflow


all: ${PROGRAMS}

debug: CXXFLAGS := ${CXXFLAGS:-O2=-O0} -ggdb -Wextra
debug: ${PROGRAMS}

html_flow_parser: html_flow_parser.o
	${CXX} ${CXXFLAGS} -std=c++0x -o html_flow_parser html_flow_parser.o -lz

html_flow_parser.o: html_flow_parser.cpp html_flow_parser.h pna.h
	${CXX} ${CXXFLAGS} -std=c++0x -c html_flow_parser.cpp

tcpflow: ${OBJECTS}
	${CXX} ${CXXFLAGS} -o tcpflow ${OBJECTS} ${LDLIBS}

.o: ${HEADERS}
	${CXX} ${CXXFLAGS} -c ${@:.o=.cpp}

md5.o: md5.c md5.h
	gcc -c md5.c 

clean:
	rm -f ${PROGRAMS} *.o *~
