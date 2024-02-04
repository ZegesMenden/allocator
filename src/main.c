#include <stdlib.h>
#include <stdio.h>
#include "include/allocator.h"

int main() {

    void* hbase = malloc(__heap_n_sectors*__heap_sector_alignment);

    alloc_init(hbase);

    char* test_ptr = (char*)alloc(300);

    free(hbase);

    return 0;

}