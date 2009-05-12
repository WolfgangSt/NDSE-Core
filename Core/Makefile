CC =		g++
AR =		ar
#ARFLAGS =	@ARFLAGS@
QT_ROOT = /opt/qtsdk-2009.02/qt
QT_INC = $(QT_ROOT)/include
QT_LIB = $(QT_ROOT)/lib
INCLUDES =	-I. -I../contrib/itl/trunk/itl/src -I../contrib/distorm64 -I../contrib/libelf -I$(QT_INC) -I$(QT_INC)/QtCore
CFLAGS =	-g -O2 $(INCLUDES)
LIBS = -lQtCore
LIBDIRS = -L$(QT_LIB)
LDFLAGS = $(LIBDIRS) $(LIBS)

SRCS=Breakpoint.cpp Compiler.cpp Disassembler.cpp HLE.cpp \
 	Interrupt.cpp io_observe.cpp loader_elf.cpp loader_nds.cpp \
 	Mem.cpp NDSE.cpp PhysMem.cpp Util.cpp
OBJS	:=	$(addprefix Debug/,$(notdir $(SRCS:.cpp=.o)))

all: Debug/NDSE.so

$(OBJS): $(SRCS)
	-mkdir Debug
	$(CC) $(CFLAGS) -c $< -o $@

Debug/NDSE.so:	$(OBJS)
	$(CC) -shared $(LDFLAGS) $(OBJS) -o $@

clean:
	-rm Debug/*.o





