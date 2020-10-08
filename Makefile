all : main overlay offlinetests headless minimal openvrless fullres \
	compiledintest

#slowest, get in queue first for performance in parallel compiles.
OBJS+=lib/stb_include_custom.o lib/stb_image.o lib/tcc_single_file.o \
	lib/cnrbtree.o lib/tccengine_link.o lib/tcccrash_link.o \
	lib/symbol_enumerator_link.o lib/cnhash_link.o lib/jsmn.o \
	lib/os_generic_link.o cntools/vlinterm/vlinterm.o

OBJS+=rawdraw/CNFG.o

OBJS+=src/cnovr.o src/ovrchew.o src/cnovrparts.o src/cnovrmath.o src/cnovrutil.o \
	src/cnovrindexedlist.o src/cnovropenvr.o src/cnovrtcc.o \
	src/cnovrtccinterface.o src/cnovrfocus.o src/cnovrcanvas.o \
	src/cnovrterminal.o \
	./libopenvr_api.so


CFLAGS := -Iopenvr/headers -Irawdraw -DCNFGOGL -Iinclude -g -Icntools/cnhash -Ilib
CFLAGS += -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-result -Wno-string-plus-int
CFLAGS += -O2 -g -Ilib/tinycc -Icntools/cnrbtree -DOSG_NOSTATIC -DCNFGCONTEXTONLY -Icntools/vlinterm

#Note: If you are operating on older OpenGL Implementations, uncomment
# the following line, this will prevent the #line directive from being
# used.  #line makes it easier to do GLSL debugging.
#CFLAGS += -DSTB_INCLUDE_LINE_NONE

#Linux
CC=gcc
LDFLAGS+=-lX11 -lGL -ldl -lm -lpthread -lXext -rdynamic -Wl,--wrap=fopen 
LDFLAGS+= -Wl,-rpath,.

#You can get it from ./openvr/lib/linux64/libopenvr_api.so

#if you need it extra small...
#CFLAGS+=-Os -ffunction-sections -fdata-sections
#LDFLAGS+=-Wl,--gc-sections -s

#Windows
#LDFLAGS:=-lgdi32 -luser32 -lopengl32 ./openvr_api.lib -ldbghelp
#CC=x86_64-w64-mingw32-gcc

%.o : %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

libopenvr_api.so : openvr/lib/linux64/libopenvr_api.so
	cp openvr/lib/linux64/libopenvr_api.so .


main : $(OBJS) src/main.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

overlay : $(OBJS) src/overlay.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

fullres : $(OBJS) src/fullres.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

openvrless : $(OBJS) src/openvrless.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

headless : $(OBJS) src/headless.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

minimal : $(OBJS) src/minimal.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

offlinetests : $(OBJS) src/offlinetests.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

compiledintest : src/compiledintest.o \
	lib/stb_include_custom.o lib/stb_image.o \
	lib/cnrbtree.o lib/cnhash_link.o lib/jsmn.o \
	lib/os_generic_link.o cntools/vlinterm/vlinterm.o \
	rawdraw/CNFG.o src/disable_tcc.o \
	lib/tcccrash_link.o lib/symbol_enumerator_link.o \
	src/cnovr.o src/ovrchew.o src/cnovrparts.o src/cnovrmath.o src/cnovrutil.o \
	src/cnovrindexedlist.o src/cnovropenvr.o \
	src/cnovrfocus.o src/cnovrcanvas.o \
	src/cnovrterminal.o \
	./libopenvr_api.so
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~ $(OBJS) src/*.o
	rm -rf main


#motherloade
#objdump -t main | grep .text | cut -c 35- | gawk --non-decimal-data '{ $1 = sprintf("%08d", "0x" $1) } 1' | grep -V stbi | sort | cut -f 1 -d' ' | paste -sd+ | bc
#objdump -t main | grep .text | cut -c 35- | gawk --non-decimal-data '{ $1 = sprintf("%08d", "0x" $1) } 1'

