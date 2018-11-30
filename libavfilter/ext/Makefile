all: zonekey_det.so

CXX_FLAGS= -c -g -O2 -fPIC -I../../ `pkg-config --cflags opencv`
LD_FLAGS= -shared ../libavfilter.a ../../libavutil/libavutil.a `pkg-config --libs opencv`

zonekey_det.so: zonekey_det.o
	${CXX} -o $@ zonekey_det.o ${LD_FLAGS}


zonekey_det.o: zonekey_det.cpp ../loadlib.h ../internal.h
	${CXX} -o $@ zonekey_det.cpp ${CXX_FLAGS}


clean:
	rm -f *.o
	rm -f zonekey_det.so
