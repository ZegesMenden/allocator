#ifndef ALLOC_V2_H
#define ALLOC_V2_H

#include <stdlib.h>
#include <stdint.h>

#define allocator_debug_enable

#ifdef allocator_debug_enable
    #include <stdio.h>
    #define __allocdebugprintf(...) printf(__VA_ARGS__)
#else
    #define __allocdebugprintf(...)
#endif

// pointer to the base of the heap, where allocations will start at
void* __heap_base;

// pointer to the top of the heap, where the sector data will start at
void* __heap_top;

size_t __heap_used_bytes = 0;
size_t __heap_max_size = 0;

typedef union {
    uint32_t raw;
    struct {
        uint32_t allocated: 1;
        // true if the next sector (below this one in memory) exists
        uint32_t next_sector_exists: 1;
        // true if the previous sector (above this one in memory) exists
        uint32_t prev_sector_exists: 1;
        // number of bytes allocated
        uint32_t allocation_size: 29;
    } fields;
} __heap_sector_data_t;

#define __heap_minimum_allocation_size sizeof(__heap_sector_data_t)<<2

#define __heap_maximum_allocation_size (2<<29)-1

void memalloc_init(void* heap_base, size_t heap_size) {
    
    // initialize heap variables
    __heap_base = heap_base;
    __heap_max_size = heap_size;
    __heap_top = (void*)((char*)__heap_base + __heap_max_size - sizeof(__heap_sector_data_t));
    
    // initialize top pointer
    __heap_sector_data_t* heap_top_ptr = (__heap_sector_data_t*)__heap_top;
    heap_top_ptr->fields.allocated = 0;
    heap_top_ptr->fields.allocation_size = 0;
    heap_top_ptr->fields.next_sector_exists = 0;
    heap_top_ptr->fields.prev_sector_exists = 0;

}

void* __user_ptr_from_sector(__heap_sector_data_t* sector) {
    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    char* data_ptr = (char*)__heap_base;
    while ( sector != sector_cur ) {
        data_ptr += sector_cur->fields.allocation_size;
        if ( !sector_cur->fields.next_sector_exists ) { return NULL; }
        sector_cur -= 1;
    }
    return (void*)data_ptr;
}

__heap_sector_data_t* __heap_sector_from_user_pointer(void* usr_ptr) {

    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    size_t user_byte_idx = (char*)usr_ptr - (char*)__heap_base;
    size_t byte_idx = 0;
    do {
        byte_idx += sector_cur->fields.allocation_size;
        if ( byte_idx == user_byte_idx ) { return sector_cur; }
        sector_cur -= 1;
    } while ( sector_cur->fields.next_sector_exists );

    return NULL;
}

void* memalloc(size_t size) {

    __allocdebugprintf("memalloc init:\n");

    if ( size > __heap_maximum_allocation_size ) {
        __allocdebugprintf("\tallocation size is greater than the maximum, exiting\n");
        return NULL;
    }

    // search for existing sector that could work
    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;

    __allocdebugprintf("\tsearching for top sector\n");

    while( sector_cur->fields.next_sector_exists ) {
        sector_cur -= 1;
    }

    __allocdebugprintf("\tfinding the top of the heap\n");

    // find the end of the data segment of the heap
    void* heap_data_end = (void*)((char*)__heap_base + __heap_used_bytes + size);
    if ( heap_data_end > (void*)((char*)sector_cur-sizeof(__heap_sector_data_t)) ) {     
        __allocdebugprintf("\tERROR: not enough space in the heap!\n");
        return NULL; 
    }

    sector_cur->fields.next_sector_exists = 1;

    __allocdebugprintf("\tinitializing next sector\n");

    // iterate to the next sector
    sector_cur -= 1;

    size_t size_alloc = size > __heap_minimum_allocation_size ? size : __heap_minimum_allocation_size;

    sector_cur->fields.allocation_size = size;

    sector_cur->fields.allocated = 1;
    sector_cur->fields.prev_sector_exists = 1;
    sector_cur->fields.next_sector_exists = 0;

    __allocdebugprintf("\tdone\n");

    return __user_ptr_from_sector(sector_cur);

}

void memfree(void* user_ptr) {

    __allocdebugprintf("memfree init:\n");
    
    __heap_sector_data_t* dealloc_sector = __heap_sector_from_user_pointer(user_ptr);
    if ( dealloc_sector == NULL ) { 
        __allocdebugprintf("\tcould not find heap sector cooresponding to user pointer\n");
        return; 
    }

    dealloc_sector->fields.allocated = 0;

    // if there is no sector following this one, delete it
    if ( !dealloc_sector->fields.next_sector_exists ) {
        __allocdebugprintf("\tdeleting top sector\n");
        dealloc_sector += 1; 
        dealloc_sector->fields.next_sector_exists = 0;
        __allocdebugprintf("\tdone\n");
        return;
    }

    if ( !dealloc_sector->fields.prev_sector_exists ) {
        __allocdebugprintf("\tdone\n");
        return; 
    }    

    int next_sector_allocated = 1;
    int prev_sector_allocated = 1;

    dealloc_sector += 1;
    prev_sector_allocated = dealloc_sector->fields.allocated;

    dealloc_sector -= 2;
    next_sector_allocated = dealloc_sector->fields.allocated;

    dealloc_sector += 1;

    if ( next_sector_allocated && prev_sector_allocated ) {
        __allocdebugprintf("\tdone\n");
        return;
    }

    // if 2 adjacent sectors are free (merges 3 sectors)
    if ( !next_sector_allocated && !prev_sector_allocated ) {
        
        uint64_t total_free_bytes = dealloc_sector->fields.allocation_size;
        dealloc_sector -= 1;
        total_free_bytes += dealloc_sector->fields.allocation_size;
        dealloc_sector += 2;
        total_free_bytes += dealloc_sector->fields.allocation_size;
        
        if ( total_free_bytes > __heap_maximum_allocation_size ) {
            uint64_t split_bytes = total_free_bytes - __heap_maximum_allocation_size;
            
            if ( split_bytes != 1 ) {
                dealloc_sector->fields.allocation_size = split_bytes>>1;
                dealloc_sector -= 1;
                dealloc_sector->fields.allocation_size = split_bytes>>1;
                dealloc_sector += 1;
            } else {
                dealloc_sector->fields.allocation_size = split_bytes>>1;
                dealloc_sector -= 1;
                dealloc_sector->fields.allocation_size = 0;
                dealloc_sector += 1;
            }

            dealloc_sector -= 1;
            dealloc_sector->fields.allocation_size = __heap_maximum_allocation_size;

            __allocdebugprintf("\tdone\n");
            return;

        }

        dealloc_sector->fields.allocation_size = 0;
        dealloc_sector -= 1;
        dealloc_sector->fields.allocation_size = 0;
        dealloc_sector += 2;
        dealloc_sector->fields.allocation_size = total_free_bytes;

        __allocdebugprintf("\tdone\n");
        
        return;
    }

    

}

#endif