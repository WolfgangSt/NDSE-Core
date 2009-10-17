CC =		g++
AR =		ar
#ARFLAGS =	@ARFLAGS@
INCLUDES =	-I. -I../itl/itl/src -I../distorm64 -I../libdwarf/libelf -I../libdwarf/libdwarf
CFLAGS = -fPIC -g -O2 -fvisibility=hidden $(INCLUDES) -DNDSE -DEXPORT
LDFLAGS =

SRCS=Breakpoint.cpp Compiler.cpp Disassembler.cpp HLE.cpp \
 	Interrupt.cpp io_observe.cpp loader_elf.cpp loader_nds.cpp \
 	Mem.cpp NDSE.cpp PhysMem.cpp Util.cpp runner.cpp SourceDebug.cpp \
	signal2/nixsig.cpp
OBJS	:=	$(SRCS:.cpp=.o)

all: libNDSE.a libNDSE.so

$(OBJS): %.o:
	$(CC) $(CFLAGS) -c $*.cpp -o $@


libNDSE.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

libNDSE.so: $(OBJS)
	$(CC) -shared -Wl,-soname=libNDSE.so $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f signal2/*.o
	rm -f *.o
	rm -f libNDSE.a
	rm -f libNDSE.so* 





