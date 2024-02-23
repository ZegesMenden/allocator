#pragma once

#include <stdint.h>

#define gc_debug_enable

#define gc_initial_max_allocs 128

#ifdef gc_debug_enable
    #include <stdio.h>
    #define __gcdebugprintf(...) printf(__VA_ARGS__)
#else
    #define __gcdebugprintf(...)
#endif

#ifndef __gcallocfun
#error "garbage collector allocation function must be defined!"
#endif

#ifndef __gcreallocfun
#error "garbage collector reallocation function must be defined!"
#endif

#ifndef __gcfreefun
#error "garbage collector allocation function must be defined!"
#endif

#ifndef __gcnallocsfun
#error "garbage collector allocation function must be defined!"
#endif

// this is the platform specific code to get the current stack pointer
#define __gc_get_stackptr() __builtin_frame_address(0)

// this MUST be the first thing to run in program execution
#define gc_stack_init() __stack_base = (const uintptr_t*)__gc_get_stackptr()

uintptr_t* __stack_base;
uint8_t* __gc_alloc_map;
size_t __gc_allocs_allowed = gc_initial_max_allocs;

int gc_init() {
    
    __gc_allocs_allowed = (gc_initial_max_allocs/8 + 1) * 8;
    __gc_alloc_map = (uint8_t*)__gcallocfun(__gc_allocs_allowed);

    if ( __gc_alloc_map == NULL ) {
        __gcdebugprintf("ERROR: not enough memory to initialize GC!\n");
        return 0;
    }

    return 1;

}

void __gc_free_unused_memory() {

    __gcdebugprintf("GC free init\n");

    size_t n_allocs = __gcnallocsfun();
    __gcdebugprintf("\t%llu allocations\n", n_allocs);

    if ( n_allocs == 0 ) {
        __gcdebugprintf("\tthere is nothing in the heap...\n");
        return;
    }

    size_t n_flags = ((n_allocs/8) + 1);
    
    if ( n_flags * 8 > __gc_allocs_allowed ) {
        __gcdebugprintf("\tresizing alloc array\n");

    }

    uintptr_t* stack_start = (uintptr_t*)__gc_get_stackptr();

    for ( ; stack_start < __stack_base; stack_start += 1 ) {



    }

}