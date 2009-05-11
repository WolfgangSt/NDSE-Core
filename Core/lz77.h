#ifndef _LZ77_H_
#define _LZ77_H_

// code based on gbalzss
template <typename mixin> class lz77: public mixin
{
	enum { N = 4096 };
	enum { F = 18 };
	enum { THRESHOLD = 2 };

	unsigned char text_buf[N + F - 1];
public:
	void decode(void)
	{
		int r, z;
		unsigned long flags;
		unsigned long decomp_size;
		unsigned long cur_size = 0;

		// discard gba header info
		mixin::get_header();
		unsigned long gbaheader = 0;
		unsigned char *c = (unsigned char*)&gbaheader;
		c[0] = mixin::get8();
		c[1] = mixin::get8();
		c[2] = mixin::get8();
		c[3] = mixin::get8();
		decomp_size = gbaheader >> 8;
		

		for (unsigned int i = 0; i < N - F; i++) 
			text_buf[i] = 0xFF;
		
		r = N - F;
		flags = z = 7;
		
		while (cur_size < decomp_size )
		{
			flags <<= 1;
			z++;
			if (z == 8)
			{
				flags = mixin::get8();
				z = 0;
			}
			if (!(flags & 0x80))
			{
				unsigned char c = mixin::get8();
				mixin::put8(c);
				text_buf[r++] = c;
				r &= (N - 1); 
				cur_size++;
			} else 
			{
				unsigned long i = mixin::get8();
				unsigned long j = mixin::get8();
				j |= (i << 8) & 0x0F00;
				i >>= 4;
				i += THRESHOLD;

				for (unsigned int k = 0; k <= i; k++)
				{
					unsigned char c = text_buf[(r-j-1) & (N - 1)];
					mixin::put8(c);
					text_buf[r++] = c;
					r &= (N - 1);
					cur_size++;
				}
			}
		}
	}


};

#endif
