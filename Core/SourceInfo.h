#ifndef _SOURCEINFO_H_
#define _SOURCEINFO_H_

struct source_info
{
	// contains whatever the lines mapped 
	int lineno; // line number
	int fileno; // file index (-1 = unknown)
	int colno;  // -1 = unknown
	unsigned long lopc, hipc;
	std::string symbol; // just for debugging the code atm

	// use a pool allocator if better memory performance is needed
	static source_info *allocate() { return new source_info(); }
	static void free(source_info *info) { delete info; }
};

#endif
