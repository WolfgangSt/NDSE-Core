#ifndef _IOREGS_H_
#define _IOREGS_H_

#include <vector>
//#include <boost/smart_ptr/detail/spinlock.hpp>
#include <boost/thread/mutex.hpp>

#include "basetypes.h"
#include "fifo.h"
#include "MemRegion.h"

struct ioregs
{
private:
	//boost::detail::spinlock ipc_mutex; // incompatible with *nix version as most have other boost
	boost::mutex ipc_mutex;

	unsigned long ipc;         // needs a mutex!
	unsigned long ipc_fifocnt; // needs a mutex!
	fifo<16> ipc_fifo;         // needs a mutex!
	std::vector<io_callback> readcbs;
	std::vector<io_callback> writecbs;
public:
	ioregs();
	void set_ipc(unsigned long remote_ipc);
	unsigned long get_ipc() /*const*/;
	unsigned long get_fifocnt() const;
	unsigned long get_fifostate() const; // gets full/empty bits
	bool pop_fifo(unsigned long &value); // trys to pop a value
	void push_fifo(unsigned long value); // pushs a value into fifo
	void flag_fifo(unsigned long state); // updates with remote state

	void add_callback(io_callback r, io_callback w);
	void readcb(unsigned long addr, unsigned long &value);
	void writecb(unsigned long addr, unsigned long &value);
};

struct REGISTERS9_1: public memory_region< PAGING::KB<8> >, public ioregs
{
	const char* names[SIZE];

	REGISTERS9_1( const char *name, unsigned long color, unsigned long priority );
	void store32(unsigned long addr, unsigned long value);
	void store16(unsigned long addr, unsigned long value);
	void store8(unsigned long addr, unsigned long value);
	void store32_array(unsigned long addr, int num, unsigned long *data);
	unsigned long load32(unsigned long addr);
	unsigned long load16u(unsigned long addr);
	unsigned long load16s(unsigned long addr);
	unsigned long load8u(unsigned long addr);
	void load32_array(unsigned long addr, int num, unsigned long *data);
};

struct REGISTERS7_1: public memory_region< PAGING::KB<8> >, public ioregs
{
	const char* names[SIZE];
	unsigned long spicnt;
	unsigned long spidata;
	unsigned long spidatanum;
	unsigned long spi_in[4];
	int tx, ty, dtx, dty;

	void touch_setxy(int x, int y);
	void set_spicnt(unsigned long value);
	void set_spidat(unsigned long value);

	REGISTERS7_1( const char *name, unsigned long color, unsigned long priority );
	void store32(unsigned long addr, unsigned long value);
	void store16(unsigned long addr, unsigned long value);
	void store8(unsigned long addr, unsigned long value);
	void store32_array(unsigned long addr, int num, unsigned long *data);
	unsigned long load32(unsigned long addr);
	unsigned long load16u(unsigned long addr);
	unsigned long load16s(unsigned long addr);
	unsigned long load8u(unsigned long addr);
	void load32_array(unsigned long addr, int num, unsigned long *data);
};

struct TRANSFER9: public memory_region<PAGING>
{
	TRANSFER9( const char *name, unsigned long color, unsigned long priority );
	void store32(unsigned long addr, unsigned long value);
	void store16(unsigned long addr, unsigned long value);
	void store8(unsigned long addr, unsigned long value);
	void store32_array(unsigned long addr, int num, unsigned long *data);
	unsigned long load32(unsigned long addr);
	unsigned long load16u(unsigned long addr);
	unsigned long load16s(unsigned long addr);
	unsigned long load8u(unsigned long addr);
	void load32_array(unsigned long addr, int num, unsigned long *data);
};


struct TRANSFER7: public memory_region<PAGING>
{
	TRANSFER7( const char *name, unsigned long color, unsigned long priority );
	void store32(unsigned long addr, unsigned long value);
	void store16(unsigned long addr, unsigned long value);
	void store8(unsigned long addr, unsigned long value);
	void store32_array(unsigned long addr, int num, unsigned long *data);
	unsigned long load32(unsigned long addr);
	unsigned long load16u(unsigned long addr);
	unsigned long load16s(unsigned long addr);
	unsigned long load8u(unsigned long addr);
	void load32_array(unsigned long addr, int num, unsigned long *data);
};

#endif
