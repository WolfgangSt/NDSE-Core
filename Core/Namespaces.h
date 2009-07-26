#ifndef _NAMESPACES_H_
#define _NAMESPACES_H_

enum { MAX_CPU = 2 };
struct _ARM9 { enum { VALUE = 0 }; };
struct _ARM7 { enum { VALUE = 1 }; };

struct _DEFAULT {};
struct _FLAT {};


template <int n> struct CPU {};
template <> struct CPU<0> { typedef _ARM9 T; };
template <> struct CPU<1> { typedef _ARM7 T; };


// TODO: long term fixups:
// replace all members that are like "arm7 arm9"
// with arrays[MAX_CPU] to simplify later CPU extensions
// use boost::mpl iterators to init/process them

#endif
