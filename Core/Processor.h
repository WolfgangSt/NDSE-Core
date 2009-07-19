#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_

#include "ArmContext.h"

template<typename T> class processor
{
private:
	typedef memory_map<T> mem;
public:
	static emulation_context context;

	// holds the current page ARM code gets executed on
	// updated in HLE branch command
	// this has a race condition when used during branches
	// when updating but not having jumped yet ... (fix this!)
	//
	// example:
	// mov ecx, new_arm_addr
	// call HLE_branch   - This updates last_page and returns the 
	//                     host destination in eax
	// ...               - a stop after the last_page update and before
	//                     the following jmp will expose a serious
	//                     race condition:
	//                     eip still points to the old page
	//                     stack does not contain any return to new page
	//                     => resolver fails resolve ARM address
	// jmp eax


	// this might be solved the following way:
	// replace the final jmp eax of a pageflip
	// with a sequence as:
	// push eax - at this moment the current page (through eip)
	//            as well as the new page (through stack)
	//            will be considered during resolve stage
	// ...      - change last_page here to the new page
	//            a resolve behind this point will resolve to the
	//            instruction we are gonna jump to with the ret behind
	// ...      - This place has a race condition when beeing stopped at
    //            it should not be a practical problem in practise however
	// ret      - this effectivley flips to the new page

	// or this way:
	// add a last_last_page holding the page before the last_page
	// thus (with small additional check overhead) a too early update
	// could be recovered

	static memory_block* last_page;

	static void reset_mapping()
	{
		memory::initializer<T>::initialize_mapping();
	}

	static void reset()
	{
		reset_mapping();
	}
};

template<typename T> emulation_context processor<T>::context;
template<typename T> memory_block* processor<T>::last_page;

#endif
