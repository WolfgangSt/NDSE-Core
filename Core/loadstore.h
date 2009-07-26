#ifndef _LOADSTORE_H_
#define _LOADSTORE_H_

struct load_stores
{
	void* store32;
	void* store16;
	void* store8;
	void* store32_array;
	void* load32;
	void* load16u;
	void* load16s;
	void* load8u;
	void* load32_array;
};


#endif
