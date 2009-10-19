#ifndef _COMPILEDBLOCK_H_
#define _COMPILEDBLOCK_H_

// instances of compiled_block could be pool allocated
// if vmem fragmentation should stay lower

#include <cstring>
#include "Mem.h"

template <typename T>
struct compiled_block_base
{
	enum { REMAPS = PAGING::INST<T>::NUM };
	char *code;             // compiled code
	size_t code_size;       // size of compiled code
	memory_block *block;
	char *remap[REMAPS]; // remapping from ARM address to compiled code
};

#endif
