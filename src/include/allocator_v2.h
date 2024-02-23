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
        // number of bytes allocated
        uint32_t allocation_size: 30;
    } fields;
} __heap_sector_data_t;

#define __heap_minimum_allocation_size sizeof(__heap_sector_data_t)<<2

#define __heap_maximum_allocation_size (1<<30)-1

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
        sector_cur -= 1;
        if ( byte_idx == user_byte_idx ) { return sector_cur; }
    } while ( sector_cur->fields.next_sector_exists );

    return NULL;
}

size_t heap_used_bytes() {
    __heap_sector_data_t*  sector_cur = (__heap_sector_data_t*)__heap_top;
    size_t ret = 0;

    while ( 1 ) {
        ret += sector_cur->fields.allocation_size + sizeof(__heap_sector_data_t);
        if ( !sector_cur->fields.next_sector_exists ) { break; }
        sector_cur -= 1;
    }

    return ret;
}

size_t heap_n_allocs() {
    size_t n_allocations = 0;
    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    
    if ( !sector_cur->fields.next_sector_exists && sector_cur->fields.allocated ) {
        n_allocations = 1;
    }

    while( sector_cur->fields.next_sector_exists ) {
        n_allocations += 1;
        sector_cur -= 1;
    }

    return n_allocations;
}

size_t heap_size() {
    return ((char*)__heap_top) - ((char*)__heap_base);
}

void __remove_empty_chunks() {

    // number of sectors to move by
    int n_sector_shift = 0;
    __heap_sector_data_t* sector_cur;
    __heap_sector_data_t* sector_move_start;

    while (sector_cur->fields.next_sector_exists) {

        // increment the shift ammount while there are empty sectors
        if ( !sector_cur->fields.allocation_size ) {
            n_sector_shift++;
        }

        // once we find a sector that is used, set the sector shift start to here
        if ( n_sector_shift > 0 && sector_cur->fields.allocation_size > 0 ) {
            sector_move_start = sector_cur;
            while ( sector_cur->fields.next_sector_exists && sector_cur->fields.allocation_size > 0 ) {
                sector_cur -= 1;
            }
            int sector_dist = sector_move_start - sector_cur;
            for ( int i = sector_dist; i > 0; i-- ) {
                sector_cur[i + n_sector_shift] = sector_cur[i];
            }
            
        }

    }

}

void* memalloc(size_t size) {

    __allocdebugprintf("memalloc init:\n");

    if ( size > __heap_maximum_allocation_size ) {
        __allocdebugprintf("\tallocation size is greater than the maximum, exiting\n");
        return NULL;
    }

    size_t size_alloc = size > __heap_minimum_allocation_size ? size : __heap_minimum_allocation_size;

    // search for existing sector that could work
    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    
    __heap_sector_data_t* smallest_viable_sector = NULL;
    uint32_t smallest_viable_sector_size = INT32_MAX;

    __allocdebugprintf("\tsearching for first available sector\n");

    while( sector_cur->fields.next_sector_exists ) {
        if ( !sector_cur->fields.allocated && sector_cur->fields.allocation_size < smallest_viable_sector_size && sector_cur->fields.allocation_size >= size_alloc ) {
            smallest_viable_sector_size = sector_cur->fields.allocation_size;
            smallest_viable_sector = sector_cur;
        }
        sector_cur -= 1;
    }

    if ( smallest_viable_sector != NULL ) {

        __allocdebugprintf("\tfound pre-existing sector that works\n\tdone\n");
        smallest_viable_sector->fields.allocated = 1;
        return __user_ptr_from_sector(smallest_viable_sector);

    }

    __allocdebugprintf("\tfinding the top of the heap\n");

    // find the end of the data segment of the heap
    void* heap_data_end = (void*)((char*)__heap_base + __heap_used_bytes + size);
    if ( heap_data_end > (void*)((char*)sector_cur-sizeof(__heap_sector_data_t)) ) {     
        __allocdebugprintf("\tERROR: not enough space in the heap!\n");
        return NULL; 
    }

    if ( (void*)sector_cur == __heap_top && !sector_cur->fields.allocated && !sector_cur->fields.next_sector_exists ) {
        __allocdebugprintf("\tallocating heap base\n");
        sector_cur->fields.allocated = 1;
        sector_cur->fields.allocation_size = size_alloc;
        sector_cur->fields.next_sector_exists = 0;
        __allocdebugprintf("\tdone\n");
        return __user_ptr_from_sector(sector_cur);
    }

    sector_cur->fields.next_sector_exists = 1;

    __allocdebugprintf("\tinitializing next sector\n");

    // iterate to the next sector
    sector_cur -= 1;

    sector_cur->fields.allocation_size = size;
    sector_cur->fields.allocated = 1;
    sector_cur->fields.next_sector_exists = 0;

    __allocdebugprintf("\tdone\n");

    return __user_ptr_from_sector(sector_cur);

}

void* memrealloc(void* ptr, size_t size) {

    __allocdebugprintf("realloc init:\n");

    __heap_sector_data_t* sector_realloc = __heap_sector_from_user_pointer(ptr);

    

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

    if ( (void*)dealloc_sector == __heap_top ) {
        __allocdebugprintf("\theap top, done\n");
        return; 
    }    

    __heap_sector_data_t* sector_prev = dealloc_sector + 1;
    __heap_sector_data_t* sector_next = dealloc_sector - 1;

    if ( sector_next->fields.allocated && sector_prev->fields.allocated ) {
        __allocdebugprintf("\tdone\n");
        return;
    }

    // if 2 adjacent sectors are free (merges 3 sectors)
    if ( !sector_next->fields.allocated && !sector_prev->fields.allocated ) {

        __allocdebugprintf("\tmerging three sectors\n");
        
        uint32_t total_free_bytes = dealloc_sector->fields.allocation_size + \
                                    sector_prev->fields.allocation_size + \
                                    sector_next->fields.allocation_size;
        
        if ( total_free_bytes > __heap_maximum_allocation_size ) {
            uint32_t split_bytes = total_free_bytes - __heap_maximum_allocation_size;

            sector_next->fields.allocation_size = split_bytes/2;
            dealloc_sector->fields.allocation_size = split_bytes/2 + split_bytes&1;
            sector_prev->fields.allocation_size = __heap_maximum_allocation_size;
            
            __allocdebugprintf("\tdone\n");
            return;
        }

        sector_next->fields.allocation_size = 0;
        sector_prev->fields.allocation_size = total_free_bytes;
        dealloc_sector->fields.allocation_size = 0;

        __allocdebugprintf("\tdone\n");
        
        return;
    }

    // if the previous sector is free
    if ( !sector_prev->fields.allocated ) {

        __allocdebugprintf("\tmerging current sector and previous\n");

        uint32_t total_free_bytes = dealloc_sector->fields.allocation_size;
        total_free_bytes += sector_prev->fields.allocation_size;

        if ( total_free_bytes > __heap_maximum_allocation_size ) {
            
            sector_prev->fields.allocation_size = __heap_maximum_allocation_size;
            dealloc_sector->fields.allocation_size = total_free_bytes - __heap_maximum_allocation_size;

            __allocdebugprintf("\tdone\n");
            return;
        }

        dealloc_sector->fields.allocation_size = 0;
        sector_prev->fields.allocation_size = total_free_bytes;

    }

    // if the next sector is free
    if ( !sector_next->fields.allocated ) {

        __allocdebugprintf("\tmerging current sector and next\n");

        uint32_t total_free_bytes = dealloc_sector->fields.allocation_size;
        total_free_bytes += sector_next->fields.allocation_size;

        if ( total_free_bytes > __heap_maximum_allocation_size ) {
            
            sector_next->fields.allocation_size = __heap_maximum_allocation_size;
            dealloc_sector->fields.allocation_size = total_free_bytes - __heap_maximum_allocation_size;

            __allocdebugprintf("\tdone\n");
            return;
        } 

        dealloc_sector->fields.allocation_size = 0;
        sector_next->fields.allocation_size = total_free_bytes;

    }

}

void memprint() {

    __heap_sector_data_t* sector_cur = (__heap_sector_data_t*)__heap_top;
    int sector_idx = 0;
    int next_sector_exists = 0;
    do {
        next_sector_exists = sector_cur->fields.next_sector_exists;
        __allocdebugprintf("sector %i:\n", sector_idx);
        __allocdebugprintf("\tsize: %i\n", sector_cur->fields.allocation_size);
        __allocdebugprintf("\tallocated: %i\n", sector_cur->fields.allocated);
        sector_cur -= 1;
        sector_idx++;
    } while (next_sector_exists);

}

#endif