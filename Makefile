all : main

OBJS:=src/main.o rawdraw/CNFGXDriver.o rawdraw/CNFGFunctions.o
OBJS+=src/cnovr.o src/chew.o src/cnovrparts.o src/cnovrmath.o


CFLAGS:=-Iopenvr/headers -Irawdraw -DCNFGOGL -Iinclude -g
LDFLAGS+=-lX11 -lGL -ldl -lm 
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

