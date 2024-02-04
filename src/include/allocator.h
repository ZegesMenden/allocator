#ifndef ALLOC_H
#define ALLOC_H

#define allocator_debug_enable

#ifdef allocator_debug_enable
    #include <stdio.h>
    #define __allocdebugprintf(...) printf(__VA_ARGS__)
#else
    #define __allocdebugprintf(...)
#endif

// pointer to the heap base
void* __heap_base;

// number of allocation sectors in the heap
#define __heap_n_sectors 1

// size of allocation sectors
#define __heap_sector_alignment 256

#define __heap_size __heap_n_sectors*__heap_sector_alignment

/// @brief 
typedef struct __heap_sector_t {

    size_t sectors_used;
    struct __heap_sector_t* prev;
    struct __heap_sector_t* next;

} __heap_sector_t;

/// @brief 
/// @param heap_base 
void alloc_init(void* heap_base) {

    __heap_base = heap_base;

    __allocdebugprintf("heap size is %llu bytes (%llu sectors)\n", __heap_n_sectors*__heap_sector_alignment, __heap_n_sectors);
    __allocdebugprintf("heap sector size is %llu bytes\n", __heap_sector_alignment);
    __allocdebugprintf("heap start pointer is %p\n", heap_base);

    // this might not work
    __heap_sector_t* heap_sector_start = (__heap_sector_t*)&__heap_base;

    // initialize the first heap sector
    heap_sector_start->sectors_used = 0;
    heap_sector_start->prev = NULL;
    heap_sector_start->next = NULL;

}

// returns pointer to the end of the heap
void* __alloc_heap_end() {
    return (void*)((char*)__heap_base + (__heap_n_sectors*__heap_sector_alignment));
}

// finds the top pointer in the heap
void* __alloc_heap_top_ptr() {
    __heap_sector_t* heap_sector = (__heap_sector_t*)&__heap_base;

    // find the end of the heap
    while ( heap_sector->next != NULL ) {
        heap_sector = heap_sector->next;
    }

    return (void*)heap_sector;
}

// returns a pointer to the data section of the heap from a sector
void* __alloc_user_ptr_from_sector(__heap_sector_t* sector) {
    // does this even work?
    return (void*)(sector+1);
}

// returns the number of free bytes in the heap
size_t __alloc_get_free_space(__heap_sector_t *top_ptr) {
    return (((char*)__alloc_heap_end() + sizeof(__heap_sector_t)) - (char*)top_ptr);
}

// returns the number of free bytes in the heap
size_t __alloc_get_free_sectors(__heap_sector_t *top_ptr) {
    return __alloc_get_free_space(top_ptr)*__heap_sector_alignment;
}

void *alloc(size_t size) {

    size_t size_real = size + sizeof(__heap_sector_t);

    // number of sectors needed to store size bytes
    size_t sectors_needed = (size_real/__heap_sector_alignment) + ((size_real%__heap_sector_alignment) != 0);

    // get the top pointer
    __heap_sector_t* top_sector = (__heap_sector_t*)__alloc_heap_top_ptr();

    // get the next pointer (but don't initialize it yet)
    __heap_sector_t *next = (__heap_sector_t*)((char*)__alloc_user_ptr_from_sector(top_sector)+(top_sector->sectors_used*__heap_sector_alignment));

    // if there is space for the allocation
    if ( __alloc_get_free_sectors(top_sector) >= sectors_needed ) {

        __allocdebugprintf("allocating %llu sectors (%llu bytes)\n", sectors_needed, size_real);        

        next->prev = top_sector;
        next->next = NULL;
        next->sectors_used = sectors_needed;

        return __alloc_user_ptr_from_sector(next);

    } else {        
        __allocdebugprintf("FATAL: not enough memory for allocation!\nbytes available: %llu\nbytes needed: %llu\n", __alloc_get_free_space(top_sector), size_real);
        return NULL;
    }
}

#endif // ALLOC_H