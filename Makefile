all : main offlinetests

#slowest, get in queue first for performance in parallel compiles.
OBJS+=lib/stb_include_custom.o lib/stb_image.o lib/tcc_single_file.o \
	lib/cnrbtree.o lib/tccengine_link.o lib/tcccrash_link.o \
	lib/symbol_enumerator_link.o lib/cnhash_link.o lib/jsmn.o \
	lib/os_generic_link.o

OBJS+=rawdraw/CNFGXDriver.o rawdraw/CNFGFunctions.o

OBJS+=src/cnovr.o src/chew.o src/cnovrparts.o src/cnovrmath.o src/cnovrutil.o \
	src/cnovrindexedlist.o src/cnovropenvr.o src/cnovrtcc.o \
	src/cnovrtccinterface.o src/cnovrfocus.o

CFLAGS:=-Iopenvr/headers -Irawdraw -DCNFGOGL -Iinclude -g -Icntools/cnhash -Ilib
LDFLAGS+=-lX11 -lGL -ldl -lm -lpthread -lXext
LDFLAGS+=./openvr/lib/linux64/libopenvr_api.so
#LDFLAGS+=./libopenvr_api.so

CFLAGS +=-Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-result -Wno-string-plus-int
CFLAGS +=-O1 -g -Ilib/tinycc -Icntools/cnrbtree -DOSG_NOSTATIC


#CFLAGS+=-Os -ffunction-sections -fdata-sections
#LDFLAGS+=-Wl,--gc-sections

CC=gcc

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
#objdump -t main | grep .text | cut -c 35- | gawk --non-decimal-data '{ $1 = sprintf("%08d", "0x" $1) } 1'

