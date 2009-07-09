CC =		g++
AR =		ar
#ARFLAGS =	@ARFLAGS@
QT_ROOT = /opt/qtsdk-2009.02/qt
QT_INC = $(QT_ROOT)/include
QT_LIB = $(QT_ROOT)/lib
INCLUDES =	-I. -I../contrib/itl/trunk/itl/src -I../contrib/distorm64 -I../contrib/libelf -I../contrib/libdwarf -I$(QT_INC) -I$(QT_INC)/QtCore
CFLAGS = -fPIC -g -O2 -fvisibility=hidden $(INCLUDES) -DNDSE -DEXPORT
LIBS = -lQtCore
LIBDIRS = -L$(QT_LIB)
LDFLAGS = $(LIBDIRS) $(LIBS)

SRCS=Breakpoint.cpp Compiler.cpp Disassembler.cpp HLE.cpp \
 	Interrupt.cpp io_observe.cpp loader_elf.cpp loader_nds.cpp \
 	Mem.cpp NDSE.cpp PhysMem.cpp Util.cpp runner.cpp SourceDebug.cpp \
	signal2/nixsig.cpp
#OBJS	:=	$(addprefix Debug/,$(notdir $(SRCS:.cpp=.o)))
OBJS	:=	$(addprefix Debug/,$(SRCS:.cpp=.o))

all: Debug/libNDSE.a Debug/libNDSE.so


#OUTFILE = $(addprefix Debug/,$(notdir $(src:.cpp=.o)))
#CCLINE = echo $(CC) $(CFLAGS) -c $(src) -o $(OUTFILE)
#BOJBS=$(foreach src,$(SRCS),$(OUTFILE): $(src)"\n"$(CCLINE)"\n\n")
#$(BOBJS)

#$(OBJS): $(SRCS)
#	#-mkdir Debug
#	echo $(CC) $(CFLAGS) -c $@.cpp -o $@

$(OBJS): %.o:
	$(CC) $(CFLAGS) -c $(subst Debug/,,$*.cpp) -o $@


Debug/libNDSE.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

Debug/libNDSE.so: $(OBJS)
	$(CC) -shared -Wl,-soname=libNDSE.so $(LDFLAGS) -o $@ $(OBJS)

clean:
	-rm Debug/signal2/*.o
	-rm Debug/*.o

test:
	echo $(BOJBS)





