#include "../src/regs.S"

.set noreorder

#Set all symbols as weak to allow for these symbols to not exist
.weak dl_test_sym
.weak dlopen
.weak dfs_open

.data

#Declare three pointers to imported symbols
.global dl_test_ptr #Pointer to dl_test_sym
dl_test_ptr:
.word dl_test_sym
.global dlopen_ptr #Pointer to dlopen
dlopen_ptr:
.word dlopen
.global dfs_open_ptr #Pointer to dfs_open
dfs_open_ptr:
.word dfs_open