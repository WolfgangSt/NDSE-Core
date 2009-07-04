#ifndef _BREAKPOINTBASE_H_
#define _BREAKPOINTBASE_H_

#include <map>

struct breakpoint_defs
{
	enum stepmode { 
		STEP_OVER_ARM, 
		STEP_INTO_ARM,
		STEP_OVER_EMU,
		STEP_INTO_EMU,
		STEP_OVER_SRC,
		STEP_INTO_SRC
	};

		class break_data;
	class break_info
	{
	public:
		bool used;    // normal breakpoint set?
		bool patched; // currently patched?
		char *pos;
		char original_byte;
		unsigned long subaddr;  // the subaddress the breakpoint was set on
		break_data *block;

		break_info()
		{
			used = false;
			patched = false;
			pos = 0;
		}
	};
	enum { MAX_SUBINSTRUCTIONS = 40 }; // enlarge if more is needed
	enum { MAX_SUBINSTRUCTIONS_DISTORM = 20 }; // enlarge if more is needed

	class break_data
	{
	public:
		unsigned long addr;     // the address the breakpoint was set on
		unsigned int jit_instructions;   // jit instructions this instruction has
		break_info jit_line[MAX_SUBINSTRUCTIONS];
		

#ifdef NDSE
		break_data()
		{
			for (unsigned int i = 0; i < MAX_SUBINSTRUCTIONS; i++)
			{
				break_info &bi = jit_line[i];
				bi.block = this;
				bi.subaddr = i;
			}
		}
#else
		explicit break_data()
		{
			assert(0);
		}
#endif

		void operator =( const break_data &other )
		{
			
			addr = other.addr;
			jit_instructions = other.jit_instructions;
			for (unsigned int i = 0; i < MAX_SUBINSTRUCTIONS; i++)
			{
				break_info &bi = jit_line[i];
				bi = other.jit_line[i];
				bi.block = this;
			}
		}

		static break_data* allocate() { return new break_data(); }
		static void free(break_data *bd) { delete bd; }
	};
protected:
	typedef std::map<char*, break_info*> debugger_table;

};

#endif
