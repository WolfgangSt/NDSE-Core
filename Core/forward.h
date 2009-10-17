#ifndef _FORWARD_H_
#define _FORWARD_H_

template <typename T, typename U> class  breakpoints;
                                  struct breakpoint_defs;
template <typename T>             class  breakpoints_base;
template <typename T>             struct callstack_tracer;
template <typename U>             struct compiled_block;
template <typename T>             struct compiled_block_base;
                                  struct compile_info;
                                  class  compiler;
template <int n>                  struct CPU;
                                  class  disassembler;
                                  struct emulation_context;
template <typename T>             struct exception_context;
                                  class  Fiber;
template <int n>                  struct fifo;
template <typename T>             struct HLE;
                                  struct host_context;
template <typename T>             struct interrupt;
                                  struct ioregs;
								  struct IS_ARM;
								  struct IS_THUMB;
                                  struct jit_code;
template <typename T>             class  logging;
template <typename mixin>         class  lz77;
                                  struct memory;
                                  struct memory_block;
template <typename T>             class  memory_map;
template <typename size_type >    struct memory_region;
                                  struct memory_region_base;
								  struct PAGING;
template<typename T>              class  processor;
template <typename T>             struct runner;
                                  struct source_debug;
								  struct source_fileinfo;
								  struct source_info;
                                  class  source_set;
                                  struct syscontrol_contex;
								  class util;
template <typename T>             struct vram_region;

#endif

