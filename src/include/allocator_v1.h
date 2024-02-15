#ifndef ALLOC_V1_H
#define ALLOC_V1_H

#define allocator_debug_enable

#ifdef allocator_debug_enable
    #include <stdio.h>
    #define __allocdebugprintf(...) printf(__VA_ARGS__)
#else
    #define __allocdebugprintf(...)
#endif

#define __ULL_SIZE_MAX 0xffffffffffffffff

// pointer to the heap base
void* __heap_base;

// number of allocation sectors in the heap
#define __heap_n_sectors 128

// size of allocation sectors
#define __heap_sector_alignment 256

#define __heap_size __heap_n_sectors*__heap_sector_alignment

typedef struct __heap_sector_t {

    size_t sectors_used;
    struct __heap_sector_t* prev;
    struct __heap_sector_t* next;

} __heap_sector_t;

// returns pointer to the end of the heap
void* __alloc_heap_end() {
    return (void*)((char*)__heap_base + (__heap_n_sectors*__heap_sector_alignment));
}

// finds the top pointer in the heap
void* __alloc_heap_top_ptr() {
    __heap_sector_t* heap_sector = (__heap_sector_t*)__heap_base;

    // find the end of the heap
    while ( heap_sector->next != NULL ) {
        heap_sector = heap_sector->next;
    }

    return (void*)heap_sector;
}

size_t __find_sector_size(__heap_sector_t* sector) {
    
    if (sector->next != NULL) {
        return (((char*)sector->next) - ((char*)sector))/__heap_sector_alignment;
    }

}

// returns a pointer to the data section of the heap from a sector
void* __alloc_user_ptr_from_sector(__heap_sector_t* sector) {
    // does this even work?
    return (void*)(sector+1);
}

// returns the number of free bytes in the heap
size_t __alloc_get_free_space(__heap_sector_t *top_ptr) {
    return (((char*)__alloc_heap_end()) - ((char*)top_ptr));
} 

// returns the number of free sectors in the heap
size_t __alloc_get_free_sectors(__heap_sector_t *top_ptr) {
    return __alloc_get_free_space(top_ptr)/__heap_sector_alignment;
}

void memalloc_init(void* heap_base) {

    __heap_base = heap_base;

    __allocdebugprintf("heap size is %llu bytes (%llu sectors)\n", __heap_n_sectors*__heap_sector_alignment, __heap_n_sectors);
    __allocdebugprintf("heap sector size is %llu bytes\n", __heap_sector_alignment);
    __allocdebugprintf("heap start pointer is %p\n", __heap_base);

    // this might not work
    __heap_sector_t* heap_sector_start = (__heap_sector_t*)__heap_base;

    // initialize the first heap sector
    heap_sector_start->sectors_used = 0;
    heap_sector_start->prev = NULL;
    heap_sector_start->next = NULL;

    __allocdebugprintf("init done!\n");

}


void *memalloc(size_t size) {

    __allocdebugprintf("starting alloc\n");

    size_t size_real = size + sizeof(__heap_sector_t);

    // number of sectors needed to store size bytes
    size_t sectors_needed = (size_real/__heap_sector_alignment) + ((size_real%__heap_sector_alignment) != 0);

    __allocdebugprintf("\tattempting to find unused sectors\n");

    __heap_sector_t* sector_cur = (__heap_sector_t*)__heap_base;

    __heap_sector_t* smallest_valid_sector = NULL;
    size_t smallest_valid_sector_n = __ULL_SIZE_MAX;

    while (sector_cur->next != NULL) {

        if ( sector_cur->sectors_used == 0 ) {
            if ( __find_sector_size(sector_cur) >= sectors_needed ) {
                if ( __find_sector_size(sector_cur) < smallest_valid_sector_n ) {
                    smallest_valid_sector_n = __find_sector_size(sector_cur);
                    smallest_valid_sector = sector_cur;
                }
            }
        }
     
        sector_cur = sector_cur->next;
    }

    if ( smallest_valid_sector != NULL ) {

        __allocdebugprintf("\tlocated a valid pre-existing sector\n");
        smallest_valid_sector->sectors_used = sectors_needed;

        __allocdebugprintf("\tallocation success\n");
        return __alloc_user_ptr_from_sector(smallest_valid_sector);

    }

    __allocdebugprintf("\tfinding top sector\n");

    // get the top pointer
    __heap_sector_t* top_sector = (__heap_sector_t*)__alloc_heap_top_ptr();

    __allocdebugprintf("\tchecking to see if there is enough space for allocation\n");

    // if there is space for the allocation
    if ( __alloc_get_free_sectors(top_sector) >= sectors_needed ) {
        
        __allocdebugprintf("\tfinding address of the next pointer\n");

        // get the next pointer (but don't initialize it yet)
        __heap_sector_t* next = (__heap_sector_t*)(((char*)(top_sector))+(top_sector->sectors_used*__heap_sector_alignment));
        __allocdebugprintf("\tnext heap sector is at 0x%p (%llu bytes from top sector)\n", next, ((char*)next)-((char*)__heap_base));

        __allocdebugprintf("\tallocating %llu sectors (%llu bytes)\n", sectors_needed, size_real);        
        __allocdebugprintf("\tinitializing next pointer\n");        
 
        if ( next == __heap_base ) {
            __allocdebugprintf("\tallocating at heap base\n");

            next->prev = NULL;
            next->next = NULL;
            next->sectors_used = sectors_needed;
 
        } else {

            next->prev = top_sector;
            next->next = NULL;
            next->sectors_used = sectors_needed;

            top_sector->next = next;

        }       

        __allocdebugprintf("\tallocation success\n");

        return __alloc_user_ptr_from_sector(next);

    } else {        
        __allocdebugprintf("\tERROR: not enough memory for allocation!\nbytes available: %llu\nbytes needed: %llu\n", __alloc_get_free_space(top_sector), size_real);
        return NULL;
    }
}

void memfree(void* ptr) {

    __allocdebugprintf("initializing memory free\n");

    __heap_sector_t* sector = (__heap_sector_t*)__heap_base;

    __heap_sector_t* dealloc_sector = (__heap_sector_t*)(((char*)ptr)-sizeof(__heap_sector_t));
    
    // make sure this pointer is a heap sector
    while ( sector != dealloc_sector ) {
        
        // __allocdebugprintf("comparing %p with %p (%i bytes apart)\n", dealloc_sector, sector, (signed long long)((ptrdiff_t)((char*)dealloc_sector)-(ptrdiff_t)((char*)sector)));

        // this code should only run if the pointer isnt a heap sector
        if ( sector->next == NULL ) {
            __allocdebugprintf("\theap sector doesnt exist!\n");
            return;
        }

        sector = sector->next;
    }

    __allocdebugprintf("\tpointer found and validated\n");

    // if this is the base pointer and the only allocation
    if ( sector->next == NULL && sector->prev == NULL ) {
        __allocdebugprintf("\tfreeing base pointer\n");
        sector->sectors_used = 0;
        return;
    }

    // if this is the top pointer
    if ( sector->next == NULL ) {
        __allocdebugprintf("\tfreeing top pointer\n");
        sector->prev->next = NULL;
        sector->sectors_used = 0;
        return;
    }

    // merge with previous pointer

    if ( sector->prev->sectors_used != 0 ) { sector->prev->sectors_used += sector->sectors_used; }
    sector->prev->next = sector->next;
    sector->next->prev = sector->prev;

    __allocdebugprintf("\tmemory freed\n");

    return;

}

void *memrealloc(void* ptr, size_t size_new) {

    __allocdebugprintf("initializing memory reallocation\n");

    __heap_sector_t* sector = (__heap_sector_t*)__heap_base;

    __heap_sector_t* realloc_sector = (__heap_sector_t*)(((char*)ptr)-sizeof(__heap_sector_t));
    
    // make sure this pointer is a heap sector
    while ( sector != realloc_sector ) {
        
        // this code should only run if the pointer isnt a heap sector
        if ( sector->next == NULL ) {
            __allocdebugprintf("\theap sector doesnt exist!\n");
            return NULL;
        }

        sector = sector->next;
    }

    __allocdebugprintf("\tpointer found and validated\n");

    if ( size_new <= (realloc_sector->sectors_used*__heap_sector_alignment) - sizeof(__heap_sector_t) ) {
        __allocdebugprintf("\tcurrent sector is large enough to store the data needed\n");
        __allocdebugprintf("\trealloc success\n");
        return ptr;
    }

    void* userdata_ptr_new = memalloc(size_new);

    if ( userdata_ptr_new == NULL ) { 
        __allocdebugprintf("\trealloc failed!\n");
        return NULL;
    }

    __allocdebugprintf("\tcopying data to new sector\n");
    // copy data to new sector
    for ( size_t i = 0; i < realloc_sector->sectors_used * __heap_sector_alignment; i++ ) {
        ((char*)userdata_ptr_new)[i] = ((char*)__alloc_user_ptr_from_sector(realloc_sector))[i];
    }    

    __allocdebugprintf("\tfreeing old memory");
    memfree(ptr);
    __allocdebugprintf("\trealloc success\n");

    return userdata_ptr_new;
}

void memprint() {
    
    __heap_sector_t* sector = (__heap_sector_t*)__heap_base;
    int sector_n = 0;

    while(sector != NULL) {

        printf("sector %i:\n", sector_n);
        if ( sector->sectors_used ) {
            printf("\tsector is in use\n");
            printf("\tsize: %llu bytes (%llu sectors)\n", sector->sectors_used*__heap_sector_alignment, sector->sectors_used);           
        } else {
            ptrdiff_t sector_size = ((char*)sector->next) - ((char*)sector);
            printf("\tsector is not in use\n");
            printf("\tsize: %lls bytes (%lls sectors)\n", sector_size, sector_size/__heap_sector_alignment);
        }

        printf("\n");

        sector_n++;
        sector = sector->next;
    }

}

#endif // ALLOC_H