#include <libdragon.h>

DL_DSO_DEPENDENCY("rom:/dl_test_syms.dso");

extern int dl_test_ptr;

int *dl_test_value = &dl_test_ptr;