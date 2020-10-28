PROJECT:=fbxcopentest
SRCS:=main.c fbxcopen.c puff.c

CFLAGS+=-g #-DFBXCUSEPUFF -I.

all : $(PROJECT)

$(PROJECT) : $(SRCS)
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ -lz

clean :
	rm -rf *.o *~ $(PROJECT)

