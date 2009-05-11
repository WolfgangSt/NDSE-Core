#include "loader_elf.h"
#include "MemMap.h"

#include <libelf/libelf.h>
#include <assert.h>

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
		Elf32_Ehdr *ehdr;
		Elf_Scn    *sect;
		Elf32_Shdr *sect_hdr;
		char       *sect_name;
		Elf_Data   *data;
		Elf32_Addr  addr;
		bool        failed;
		char       *buffer;
	};
	static void process(memory_block *b, char *mem, int sz, context &ctx)
	{
		if ((b->flags & memory_block::PAGE_INVALID) == 0)
		{
			memcpy( mem, ctx.buffer, sz );
			b->dirty();
		} else
		{
			// problem
		}
		ctx.buffer += sz;
	}
};

bool loader_elf::load(int fd, util::load_result &res)
{
	stream_elf::context sd;
	Elf *arf = elf_begin( fd, ELF_C_READ, 0 );
	if (!arf)
		return false;

	sd.ehdr = elf32_getehdr(arf);
	res.arm9_entry = sd.ehdr->e_entry;
	sd.sect = elf_getscn(arf, 0);
	while (sd.sect) // loop over all sections
	{
		sd.sect_hdr = elf32_getshdr(sd.sect);
		sd.sect_name = elf_strptr(arf, sd.ehdr->e_shstrndx, sd.sect_hdr->sh_name);
		sd.data = elf_getdata( sd.sect, 0 );
		while (sd.data)
		{
			sd.addr = sd.sect_hdr->sh_addr + sd.data->d_off;
			sd.failed = false;
			if ((sd.data->d_size > 0) && (sd.data->d_buf))
			{
				// stream up the section
				sd.buffer = (char*)sd.data->d_buf;
				
				//logging<_ARM9>::logf("streaming section from %08X to %08X", sd.addr, sd.addr + sd.data->d_size);
				memory_map<_ARM9>::process_memory<stream_elf>( 
					sd.addr,
					(int)sd.data->d_size, 
					sd );
			}
			sd.data = elf_getdata( sd.sect, sd.data );
		}
		sd.sect = elf_nextscn(arf, sd.sect);
	}

	elf_end(arf);
	return true;
}