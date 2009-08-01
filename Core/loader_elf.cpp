#include "loader_elf.h"
#include "MemMap.h"
#include "SourceDebug.h"

#include <assert.h>
#include <libelf/libelf.h>
#include <libdwarf/libdwarf.h>
#include <libdwarf/dwarf.h>
#include <vector>

#ifdef WIN32
#include <io.h>
#define read _read
#define lseek _lseek
#define open _open
#define close _close
#else
#include <unistd.h>
#endif

void loader_elf::init()
{
	if (elf_version(EV_CURRENT) == EV_NONE)
	{
		assert(0);
	}
}

bool loader_elf::is_valid(int fd)
{
	Elf *arf = elf_begin( fd, ELF_C_READ, 0 );
	if (!arf)
		return false;
	bool result = elf_kind(arf) == ELF_K_ELF;
	elf_end(arf);
	return result;
}

struct stream_elf
{
	struct context
	{
		int fd;
		Elf32_Addr  addr;
		Elf32_Ehdr *ehdr;
		Elf32_Phdr *phdr;
	};
	static void process(memory_block *b, char *mem, int sz, context &ctx)
	{
		if ((b->flags & memory_block::PAGE_INVALID) == 0)
		{
			if (read( ctx.fd, mem, sz ) != sz) // read
			{
				// problem
			}
			b->dirty();
		} else
		{
			// problem
		}

	}
};

static bool load_debug(Elf *elf);
bool loader_elf::load(int fd, util::load_result &res, util::load_hint lh)
{
	stream_elf::context sd;

	sd.fd = fd;
	Elf *arf = elf_begin( fd, ELF_C_READ, 0 );
	if (!arf)
		return false;

	sd.ehdr = elf32_getehdr(arf);
	sd.phdr = elf32_getphdr(arf);
	size_t nump;
	if (elf_getphnum(arf, &nump) != 1)
	{
		elf_end(arf);
		return false;
	}

	logging<_DEFAULT>::log("Loading ELF");
	for (size_t i = 0; i < nump; i++, sd.phdr++)
	{
		if (!(sd.phdr->p_flags & PT_LOAD))
			continue;
		logging<_DEFAULT>::logf("Loading %08X - %08X: ", 
			sd.phdr->p_paddr, sd.phdr->p_paddr + sd.phdr->p_filesz);
		lseek(fd, sd.phdr->p_offset, SEEK_SET);

		sd.addr = sd.phdr->p_paddr;

		switch (lh)
		{
		case util::LH_ARM7:
			res.flags |= util::LOAD_ARM7;
			res.arm7_entry = sd.ehdr->e_entry;
			memory_map<_ARM7>::process_memory<stream_elf>( 
				sd.addr,
				sd.phdr->p_filesz, 
				sd );
			break;
		case util::LH_ARM9:
		default:
			res.flags |= util::LOAD_ARM9;
			res.arm9_entry = sd.ehdr->e_entry;
			memory_map<_ARM9>::process_memory<stream_elf>( 
				sd.addr,
				sd.phdr->p_filesz, 
				sd );
			break;
		}
	}

	
	//sd.sect_hdr = elf32_getshdr(sd.sect);
	
	::load_debug(arf);
	elf_end(arf);
	return true;
}

//typedef std::map<Dwarf_Addr, source_info> LineMap;
typedef std::vector<unsigned int> FileMap;

static Dwarf_Debug debug; // trade less stack overhead for thread safety!
static FileMap filemap; // maps CU file index to global file index
//static LineMap linemap;

// this globals (only) prevents loading multiple debug infos concurrently


// loads a DIE
// todo: add type loading, currently only loads
// code <-> address mappings
static void process_die(Dwarf_Die die)
{
	Dwarf_Error err;
	Dwarf_Half tag;
	Dwarf_Addr lopc, hipc;
	Dwarf_Attribute attr;
	Dwarf_Unsigned file;
	Dwarf_Unsigned line;

	if (dwarf_tag(die, &tag, &err) != DW_DLV_OK)
		return; // Die is invalid

	switch (tag)
	{
	case DW_TAG_compile_unit:
		return;
	}

	// try to load DW_AT_decl_file and optionally DW_AT_decl_line
	
	// try loading DW_AT_decl_file
	file = 0;
	if (dwarf_attr(die, DW_AT_decl_file, &attr, &err) == DW_DLV_OK)
	{
		if (dwarf_formudata(attr, &file, &err) != DW_DLV_OK)
			file = 0;
		dwarf_dealloc( debug, attr, DW_DLA_ATTR );
	} else  
	{
		// try DW_AT_call_file
		if (dwarf_attr(die, DW_AT_call_file, &attr, &err) == DW_DLV_OK)
		{
			if (dwarf_formudata(attr, &file, &err) != DW_DLV_OK)
				file = 0;
			dwarf_dealloc( debug, attr, DW_DLA_ATTR );
		} 
	}

	// load DW_AT_decl_line if available
	line = 0;
	if (dwarf_attr(die, DW_AT_decl_line, &attr, &err) == DW_DLV_OK)
	{
		if (dwarf_formudata(attr, &line, &err) != DW_DLV_OK)
			line = 0;
		dwarf_dealloc( debug, attr, DW_DLA_ATTR );
	} else
	{
		// try DW_AT_call_line
		if (dwarf_attr(die, DW_AT_call_line, &attr, &err) == DW_DLV_OK)
		{
			if (dwarf_formudata(attr, &line, &err) != DW_DLV_OK)
				line = 0;
			dwarf_dealloc( debug, attr, DW_DLA_ATTR );
		}
	}

	if (dwarf_lowpc(die, &lopc, &err) != DW_DLV_OK)
		return; // has no address (not of interest for debugging atm)
		        // those are typedefs etc
		        // they need special loading later on

	// retrieve upper bound memory address
	if (dwarf_highpc(die, &hipc, &err) != DW_DLV_OK)
		hipc = lopc; // set to low bound if none specified

	// retrieve DIEs name if any
	char *name;
	if (dwarf_diename(die, &name, &err) != DW_DLV_OK)
		name = 0;

	if ((file == 0) && (name == 0))
	{
		//Dwarf_Off off;
		//dwarf_dieoffset(die, &off, &err);
		//std::cout << "." << std::hex << off << std::dec;
		return; // not interested in those atm
	}
	
	
	// gathered all needed information add the address range
	source_info *si = source_info::allocate();
	if (file == 0)
		si->fileno = -1;
	else si->fileno = filemap[(unsigned int)file - 1];
	si->lineno = (int)line;
	si->colno  = -1;
	if (name)
	{
		si->symbol = name;
		/*
		// DEBUG DEBUG DEBUG CODE
		if (si->symbol == "main")
			breakpoints<_ARM9, IS_THUMB>::toggle( lopc, 0 );
		*/
	}


	// DONT ADD ANYMORE ONLY USE LINEINFOS FOR THE MOMENT
	//source_debug::add( (unsigned long)lopc, (unsigned long)hipc, si, false );

	// Deprecated: single line infos get merged in by ::add_line call
	/*
	if ((file == 0) || (line < 0))
	{
		// try to pull the information from linemap

		std::cout << "Range: 0x" << std::hex << lopc << " - 0x" << hipc;
		LineMap::iterator loit = linemap.lower_bound((unsigned long)lopc);
		LineMap::iterator hiit;
		if (hipc != lopc)
			hiit = linemap.upper_bound((unsigned long)hipc - 1);
		else hiit = linemap.upper_bound((unsigned long)hipc);
		for (LineMap::iterator it = loit; it != hiit; ++it)
		{
			std::cout << "\n\t 0x" << std::hex << it->first << ": " << std::dec;
			int fileno = it->second.fileno;
			if (fileno >= 0)
				std::wcout << source_debug::filenames[fileno];
			std::cout << ":" << it->second.lineno;
		}
	}
	*/

	


	/*
	// deprecated: readd later, try resolving using the line -> address 
	// mapping table in case the symbol has no file/line associated
	LineMap::iterator loit = source_debug::line_map.lower_bound((unsigned long)lopc);
	LineMap::iterator hiit = source_debug::line_map.upper_bound((unsigned long)hipc);
	for (LineMap::iterator it = loit; it != hiit; ++it)
	{
		std::cout << "\n\t";
		int fileno = it->second.fileno;
		if (fileno >= 0)
			std::wcout << source_debug::filenames[fileno].absoluteFilePath().toStdWString();
		std::cout << ":" << it->second.lineno;
		
		//std::cout << tag;
		if (name) std::cout << ":" << name;
	}*/


	if (name)
		dwarf_dealloc( debug, name, DW_DLA_STRING );
}

// recursivley loads all DIE's
static void load_die(Dwarf_Die die)
{
	Dwarf_Error err;
	Dwarf_Die sub;

	process_die(die);
	// if Die has a child, load it
	if (dwarf_child(die, &sub, &err) == DW_DLV_OK)
		load_die(sub);
	// as logn the Die has siblings, load them
	if (dwarf_siblingof(debug, die, &sub, &err) == DW_DLV_OK)
		load_die(sub);

	dwarf_dealloc(debug, die, DW_DLA_DIE);
}

struct dwarf_lineinfo
{
	Dwarf_Unsigned lineno, fileno;
	Dwarf_Signed colno;
	Dwarf_Addr addr;
	Dwarf_Bool begin, end;
};

static void load_lineinfo(Dwarf_Line line, dwarf_lineinfo &res)
{
	Dwarf_Error err;
	if (dwarf_lineno( line, &res.lineno, &err ) != DW_DLV_OK)
		res.lineno = 0;
	if (dwarf_line_srcfileno( line, &res.fileno, &err ) != DW_DLV_OK)
		res.fileno = 0;
	if (dwarf_lineoff( line, &res.colno, &err) != DW_DLV_OK)
		res.colno = -1;
	if (dwarf_lineaddr( line, &res.addr, &err ) != DW_DLV_OK)
		res.addr = 0;
	if (dwarf_linebeginstatement( line, &res.begin, &err) != DW_DLV_OK)
		res.begin = 0;
	if (dwarf_lineendsequence( line, &res.end, &err) != DW_DLV_OK)
		res.end = 0;

}

static bool load_debug(Dwarf_Debug dbg)
{
	Dwarf_Error err;

	Dwarf_Unsigned cu_header_length = 0;
	Dwarf_Half version_stamp = 0;
	Dwarf_Off abbrev_offset = 0;
	Dwarf_Half address_size = 0;
	Dwarf_Unsigned next_cu_offset = 0;

	// main routine to load debug infos
	
	// iterate over all compile units
	debug = dbg;
	
	while ((dwarf_next_cu_header(dbg, &cu_header_length, &version_stamp,
                                 &abbrev_offset, &address_size,
                                 &next_cu_offset, &err))
								 == DW_DLV_OK) 
	{
		// load all subprograms of the current compile unit (root DIE)
		Dwarf_Die root;
		if (dwarf_siblingof(dbg, 0, &root, &err) == DW_DLV_OK)
		{
			// load the CU root points at
			char **sources;
			Dwarf_Signed num;
			if (dwarf_srcfiles(root, &sources, &num, &err) == DW_DLV_OK)
			{
				filemap.resize((size_t)num);
				for (int i = 0; i < num; i++)
				{
					// add source info
					std::cout << "Found sourcefile: " << sources[i] << "\n";
					filemap[i] = source_debug::add_source(sources[i]);
					dwarf_dealloc( dbg, sources[i], DW_DLA_STRING );
				}
				dwarf_dealloc( dbg, sources, DW_DLA_LIST );
			}

			// todo: this is redundant information
			// the dwarf_srclines only contain lowpc's of the line
			// the needed mappings should be available through the DIEs within
			// each CU.
			// sourcelines that werent found within any DIE should be added
			// with a 0 range using source_debug::add after
			// loading the DIEs

			// load source lines as the DIE is gonna be deleted afterwards
			Dwarf_Line* lines;
			if (dwarf_srclines(root, &lines, &num, &err) != DW_DLV_OK)
				lines = 0;

			// according to the comments dwarf lines are continous between
			// begin and end blocks where end block addresses lie outside
			if (lines)
			{
				if (num > 0)
				{
					dwarf_lineinfo cur, next;
					load_lineinfo(lines[0], cur);
					for (int idx = 1; idx < num; idx++)
					{
						load_lineinfo(lines[idx], next);
						// process the infos about cur here


						// this aint really correct yet but a first approximation
						// todo: process begin/end informations (but how?)

						/*
						0x2000302 crti.asm 77:11
						0x2008254 crti.asm 88:10
						0x2008256 crti.asm 88:11
						*/

						if (cur.end)
						{
							cur = next;
							continue;
						}

						source_info *si = source_info::allocate();
						si->lopc = (unsigned long)cur.addr;
						si->hipc = (unsigned long)next.addr;
						si->lineno = (int)cur.lineno;
						si->fileno = -1;
						si->colno = (int)cur.colno;
						if (fileno > 0)
						{
							si->fileno = filemap[(unsigned int)cur.fileno-1];
							if (si->lineno >= 0)
							{
								std::vector<source_infos*> &lines = source_debug::files[si->fileno].lineinfo;
								if (si->lineno >= (int)lines.size())
									lines.resize(si->lineno + 1);
								source_infos* &sis = lines[si->lineno];
								if (!sis)
									sis = new source_infos();
								sis->push_back( si );
							}
						}						
						source_debug::add( (unsigned long)cur.addr, (unsigned long)next.addr, si, false );

						cur = next;
					}

					/*
					for (int idx = 1; idx < num; idx++)
					{
						dwarf_lineinfo l;
						load_lineinfo(lines[idx], l);
						std::cout << "0x" << std::hex << l.addr << std::dec << " ";	
						if (fileno > 0)
						{
							std::wstring &ws = source_debug::filenames[filemap[(unsigned int)l.fileno-1]];
							QString s = QString::fromStdWString(ws);
							QDir fi(s);
							std::wcout << fi.dirName().toStdWString();
						}
						std::cout << " "<< l.lineno << ":" << l.begin << l.end << "\n";
					}
					*/
					



				}
				dwarf_srclines_dealloc(dbg, lines, num);
			}

			// infos are pulled from the lines now!

			// load the CU DIE and extract all address mappings that could be found
			// this grants address -> symbol lookup
			load_die(root);
		}
	}

	filemap.clear();
	//linemap.clear();

	// most important step: load address -> source mappings
	// second step        : load typeinfos for python debugger
	// third step         : load stackframe infos to debug locals
	return false;
}

static bool load_debug(Elf *elf)
{
	Dwarf_Error err;
	Dwarf_Debug dbg;

	// wipe current debug infos
	// (would wipe arm7 when loaded first :()
	//source_debug::clear();

	if (dwarf_elf_init( elf, DW_DLC_READ, 0, 0, &dbg, &err ) != DW_DLV_OK)
		return false;
	load_debug(dbg);
	dwarf_finish( dbg, &err );
	source_debug::debug_print();
	return true;
}

bool loader_elf::load_debug(int fd)
{
	Dwarf_Error err;
	Dwarf_Debug dbg;
	if (dwarf_init( fd, DW_DLC_READ, 0, 0, &dbg, &err ) != DW_DLV_OK)
		return false;
	::load_debug(dbg); // calls dwarf_finish()
	return false;
}