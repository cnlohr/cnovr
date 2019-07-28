all : main

OBJS:=src/main.o rawdraw/CNFGXDriver.o rawdraw/CNFGFunctions.o cntools/cnhash/cnhash.o
OBJS+=src/cnovr.o src/chew.o src/cnovrparts.o src/cnovrmath.o src/cnovrutil.o


CFLAGS:=-Iopenvr/headers -Irawdraw -DCNFGOGL -Iinclude -g -Icntools/cnhash
LDFLAGS+=-lX11 -lGL -ldl -lm -lpthread
LDFLAGS+=openvr/lib/linux64/libopenvr_api.so

#CFLAGS+=-O0 -ffunction-sections -fdata-sections
#LDFLAGS+=-Wl,--gc-sections

#CC=tcc
#CFLAGS+=-DTCC


%.o : %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

main : $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ $(OBJS)
	rm -rf main

