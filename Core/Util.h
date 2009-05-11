#ifndef _UTIL_H_
#define _UTIL_H_

class util
{
private:
	static const unsigned short crc16tab[256];
public:

	struct load_result
	{
		unsigned long arm9_entry;
		unsigned long arm7_entry;
	};

	static unsigned short get_crc16(unsigned short crc, char* mem, int len);
	static bool load_file(const char *filename, load_result &res);
};

#endif
