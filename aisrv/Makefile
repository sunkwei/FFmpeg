all: ai_img_srv

CXX_FLAGS= -O2 -g -c
LD_FLAGS= -g -O2 `pkg-config --libs opencv` `pkg-config --libs libccgnu2`


ai_img_srv: server.o analyse.o
	$(CXX) -o $@ $^ ${LD_FLAGS}

server.o: server.cpp analyse.h
	$(CXX) -o $@ ${CXX_FLAGS} server.cpp

analyse.o: analyse.cpp analyse.h
	$(CXX) -o $@ ${CXX_FLAGS} analyse.cpp

clean:
	rm -f ai_img_srv
	rm -f *.o