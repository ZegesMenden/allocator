#include <stdlib.h>
#include <stdio.h>
#include "include/allocator_v2.h"

int main() {

    void* hbase = malloc(2048);

    memalloc_init(hbase, 2048);

    char* test0_ptr = (char*)memalloc(100);
    char* test1_ptr = (char*)memalloc(300);
    char* test2_ptr = (char*)memalloc(100);
    char* test3_ptr = (char*)memalloc(300);
    char* test4_ptr = (char*)memalloc(600);

    memfree(test3_ptr);
    memfree(test4_ptr);
    memfree(test4_ptr);

    memprint();

    free(hbase);


    return 0;

}