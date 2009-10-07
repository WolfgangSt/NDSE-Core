#ifndef _NAMESPACES_H_
#define _NAMESPACES_H_

//! Defines the maximum number of CPUs to emulate
enum { MAX_CPU = 2 };

//! Typed name for CPU0
struct _ARM9 { enum { VALUE = 0 }; };

//! Typed name for CPU1
struct _ARM7 { enum { VALUE = 1 }; };

//! Default namespace (used for logging)
struct _DEFAULT {};

//! Flat namespace (used for flat emulation)
struct _FLAT {};

//! CPU index to named type lookup helper
template <int n> struct CPU {};
template <> struct CPU<0> { typedef _ARM9 T; };
template <> struct CPU<1> { typedef _ARM7 T; };


// TODO: long term fixups:
// replace all members that are like "arm7 arm9"
// with arrays[MAX_CPU] to simplify later CPU extensions
// use boost::mpl iterators to init/process them

#endif
