#ifndef _SOURCEDEBUG_
#define _SOURCEDEBUG_

// transparent interface for source level debugging
// infos will currently only be loaded through DWARF library
// as this is based on the time the DWARF loader was written
// the internal structures are very close to DWARFs structures
// they might change in future however to adapt to other debugging 
// formats

// this unit contains all code needed for
// for address -> source file(s)/line(s) mappings using ITL
// and source file/line -> address(es) mapping
#include <list>
#include <itl/itl_value.hpp>
#include <itl/split_interval_map.hpp>
#include <algorithm>
#include <deque>
#include <vector>

#include "SourceInfo.h"

// add some independant class for file resource container helper
// that normalizes filenames and returns handles to possible
// duplicated names
// problems:
// QFileInfo's == has problems (see QT doc)
// especially when the files are not existent == is undefined
// for this purpose currently a normalized wstring is used
// which doesnt resolve symbolic links



class source_set
{
public:
	typedef std::list<source_info*> info_set;
	info_set set;

	bool operator == (const source_set &other) const { return set == other.set; }
	void operator += (const source_set &other) 
	{ 
		//info_set tmp = other.set;
		//set.merge( tmp );
		set.insert( set.end(), other.set.begin(), other.set.end() );
	}

	void operator -= (const source_set &other) 
	{ 
		/*
		info_set::iterator end = std::set_difference( set.begin(), set.end(), 
			other.set.begin(), other.set.end(), set.begin() );
		set.erase( end, set.end() );
		*/
		for (info_set::const_iterator it = other.set.begin(); 
			it != other.set.end(); ++it)
			set.remove(*it);
	}
};

// address -> source mapping
typedef itl::interval_map<unsigned long, source_set> DebugMap;

typedef std::list<source_info*> source_infos;

struct source_fileinfo
{
	std::wstring filename;
	std::vector<source_infos*> lineinfo;
};

struct source_debug
{
	// clear all debug informations
	// adding without clearing merges debug infos, 
	// possibly overwriting old values
	static DebugMap debug_map;
	static std::deque<source_fileinfo> files;

	// not used yet!
	static void clear();
	static void add(unsigned long start, unsigned long end, source_info* info, bool include_end);
	
	static int add_source(const std::string &src);
	static void debug_print();
	static const source_set* line_for(unsigned long addr);
};


#endif
