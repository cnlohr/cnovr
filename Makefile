all : main offlinetests

OBJS+=rawdraw/CNFGXDriver.o rawdraw/CNFGFunctions.o cntools/cnhash/cnhash.o
OBJS+=lib/stb_include_custom.o lib/stb_image.o

OBJS+=src/cnovr.o src/chew.o src/cnovrparts.o src/cnovrmath.o src/cnovrutil.o
OBJS+=src/cnovrindexedlist.o src/cnovropenvr.o

CFLAGS:=-Iopenvr/headers -Irawdraw -DCNFGOGL -Iinclude -g -Icntools/cnhash -Ilib
LDFLAGS+=-lX11 -lGL -ldl -lm -lpthread
LDFLAGS+=openvr/lib/linux64/libopenvr_api.so

CFLAGS +=-Wall -Wno-unused-variable -Wno-unused-function

#CFLAGS+=-Os -ffunction-sections -fdata-sections
#LDFLAGS+=-Wl,--gc-sections

#CC=tcc
#CFLAGS+=-DTCC


%.o : %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

main : $(OBJS) src/main.o 
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

offlinetests : $(OBJS) src/offlinetests.o 
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)	

clean :
	rm -rf *.o *~ $(OBJS) src/*.o
	rm -rf main


#motherloade
#objdump -t main | grep .text | cut -c 35- | gawk --non-decimal-data '{ $1 = sprintf("%08d", "0x" $1) } 1' | grep -V stbi | sort | cut -f 1 -d' ' | paste -sd+ | bc

