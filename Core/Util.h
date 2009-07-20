#ifndef _UTIL_H_
#define _UTIL_H_

class util
{
private:
	static const unsigned short crc16tab[256];
public:
	enum load_hint { LH_UNKNOWN, LH_ARM7, LH_ARM9, LH_RAW7, LH_RAW9 };
	enum { 
		LOAD_ARM7 = 0x1, 
		LOAD_ARM9 = 0x2 
	}; // flags
	struct load_result
	{
		unsigned long flags;
		unsigned long arm9_entry;
		unsigned long arm7_entry;
	};

	static unsigned short get_crc16(unsigned short crc, char* mem, int len);
	static bool load_file(const char *filename, load_result &res, load_hint lh);
};

#endif
