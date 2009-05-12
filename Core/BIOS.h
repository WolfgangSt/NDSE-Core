#ifndef _BIOS_H_
#define _BIOS_H_

template <typename T> class bios
{
private:
	struct f_crc16
	{
		typedef unsigned short context;
		static void process(memory_block *, char *addr, int len, unsigned short &ctx)
		{
			ctx = util::get_crc16( ctx, addr, len );
		}
	};
public:
	static unsigned short get_crc16(unsigned short crc, unsigned long addr, int len)
	{
		assert( !(addr & 1) );
		assert( !(len & 1) );
		memory_map<T>::template process_memory<f_crc16>( addr, len, crc );
		return 0;
	}
};

#endif
