
#include <set>

#pragma warning(push)
#pragma warning(disable: 4127)
#include "SourceDebug.h"
//#include <QString>
//#include <QDir>
#pragma warning(pop)
#include <algorithm>

#include <iostream> // for debug out



DebugMap source_debug::debug_map;
std::deque<source_fileinfo> source_debug::files;

void source_debug::clear()
{
	// collect all source_info*'s to release
	std::set<source_info*> items;
	for (DebugMap::iterator it = debug_map.begin(); it != debug_map.end(); ++it)
	{
		source_set::info_set &s = it->second.set;
		items.insert(s.begin(), s.end());
	}

	// clear all mapping entries
	debug_map.clear();
	files.clear();

	// release all items
	for (std::set<source_info*>::iterator it = items.begin(); it != items.end(); ++it)
		source_info::free(*it);
}

void source_debug::add(unsigned long start, unsigned long end, source_info* info, bool include_end)
{
	source_set set;
	set.set.push_back(info);
	if (include_end)
		debug_map += std::make_pair( itl::closed_interval(start, end), set );
	else debug_map += std::make_pair( itl::rightopen_interval(start, end), set );
}

int source_debug::add_source(const std::string &src)
{
	// most important: nomalize filename before anything else
	// as it could be that different objects use different names
	// pointing to the same source file!
	// it is assumed that the input filename is either ascii or utf8 format
	//
	// Note: QFileInfo is a bad error prone replacement see .h for details

	/*
	QString qsrc(src.c_str());
	QDir d(qsrc);
	//QString s = d.canonicalPath(); // fails on non existant files
	QString s = d.absolutePath();
	std::wstring name = s.toStdWString();
	*/

	// WARNING: THIS IS A TEMPORARY HACK TO GET RID OF QT DEPENDANCIES
	//          FIX THIS ASAP AS THIS CONFLICTS WITH SOURCE LEVEL DEBUGGING
	//          UTF8 INPUT IS _IGNORED_ TOO

	std::wstring name(src.begin(), src.end());

	// search and return index if already added
	int idx = 0;
	for (std::deque<source_fileinfo>::const_iterator it = files.begin(); 
		it != files.end(); ++it, idx++)
	{
		if (it->filename == name)
			return idx;
	}
	idx = (int)files.size();
	source_fileinfo fi;
	fi.filename = name;
	files.push_back(fi);
	return idx;
}


void source_debug::debug_print()
{
	// debug out stored information 
	/*
	for (DebugMap::iterator it = debug_map.begin(); it != debug_map.end(); ++it)
	{
		source_set::info_set &s = it->second.set;
		std::cout << "Address: 0x" << std::hex << it->first.lower() 
			<< "- 0x" << it->first.upper() << std::dec << "\n";
		for (source_set::info_set::iterator it = s.begin(); it != s.end(); ++it)
		{
			source_info *si = *it;
			// output the source hit

			//int lineno; // line number
			//int fileno; // file index (-1 = unknown)
			//int colno;  // -1 = unknown
			std::cout << "\t" << si->symbol << " - ";
			if ((si->fileno < 0) || (si->fileno >= (int)filenames.size()))
				std::cout << "<unknown file>";
			else std::wcout << filenames[si->fileno];
			std::cout << ":" << si->lineno << ":" << si->colno << "\n";
		}
	}
	*/
}

const source_set* source_debug::line_for(unsigned long addr)
{
	// supply a < operator for efficient search
	//std::binary_search( debug_map.begin(), debug_map.end(), addr, ... );
	
	for (DebugMap::iterator it = debug_map.begin(); it != debug_map.end(); ++it)
	{
		if (it->first.lower() > addr)
			return 0;
		if (it->first.contains(addr))
			return &it->second;
	}
	
	return 0;
}
