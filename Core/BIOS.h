#ifndef _BIOS_H_
#define _BIOS_H_

// merge this file into HLE as other BIOS routines are defined there, too

template <typename T> class bios
{
private:
	/*! internal CRC calculation functor */
	struct f_crc16
	{
		typedef unsigned short context;
		static void process(memory_block *, char *addr, int len, unsigned short &ctx)
		{
			ctx = util::get_crc16( ctx, addr, len );
		}
	};
public:
	/*! calculates the CRC16 of the given memory region 
	  \param crc initial crc to chain with
	  \param addr start address of the region (must be 2 byte aligned)
	  \param len length in bytes of the region (must be 2 byte aligned)
	  \return the chained crc value
	*/
	static unsigned short get_crc16(unsigned short crc, unsigned long addr, int len)
	{
		assert( !(addr & 1) );
		assert( !(len & 1) );
		memory_map<T>::template process_memory<f_crc16>( addr, len, crc );
		return 0;
	}
};

#endif
