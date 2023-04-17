#include "../src/dlfcn_internal.h"

static uint32_t hilo_get_value(uint32_t *hi_inst, uint32_t *lo_inst)
{
	int16_t lo = *lo_inst & 0xFFFF;
	return ((*hi_inst & 0xFFFF) << 16)+lo;
}

static uint32_t jump_get_target(uint32_t *inst)
{
	return ((uint32_t)inst & 0xF0000000)|((*inst & 0x3FFFFFF) << 2);
}

void test_dl_ctors(TestContext *ctx) {
	//Open dl_test_ctors module
	void *handle = dlopen("rom:/dl_test_ctors.dso", RTLD_LOCAL);
	DEFER(dlclose(handle));
	//Find required symbol used to verify that constructors have run
	unsigned int *test_value = dlsym(handle, "dl_ctor_test_value");
	//Check if required symbol is found
	ASSERT(test_value, "Test value symbol not found");
	//Verify that module constructors have run
	ASSERT(*test_value == 0x456789AB, "Global constructors for modules did not execute");
}

void test_dladdr(TestContext *ctx) {
	//Open module for testing dladdr
	void *handle = dlopen("rom:/dl_test_syms.dso", RTLD_LOCAL);
	DEFER(dlclose(handle));
	//Find required symbol used to test dladdr with
	char *test_sym = dlsym(handle, "dl_test_sym");
	//Check if required symbol is found
	ASSERT(test_sym, "Failed to find module symbol needed to test dladdr");
	//Run dladdr on module symbol address
	Dl_info info;
	dladdr(test_sym, &info);
	//Verify that module symbol is correct
	ASSERT(info.dli_fname && strcmp(info.dli_fname, "rom:/dl_test_syms.dso") == 0, "dladdr failed to find correct module");
	ASSERT(info.dli_saddr && info.dli_saddr == test_sym, "dladdr failed to find correct symbol");
	//Try dladdr on main executable symbol
	dladdr((void *)dlopen, &info);
	//Verify that this works as expected
	ASSERT(!info.dli_sname, "dladdr should not provide symbol names for main executable symbols");
	ASSERT(!info.dli_fname, "dladdr should not provide module names for main executable symbols");
}

void test_dlclose(TestContext *ctx) {
	//Open modules dl_test_syms (symbols exported) and dl_test_imports (symbols not exported)
	void *handle1 = dlopen("rom:/dl_test_syms.dso", RTLD_GLOBAL);
	void *handle2 = dlopen("rom:/dl_test_imports.dso", RTLD_LOCAL);
	DEFER(dlclose(handle2)); //Will cause warning on command line upon exit when successful
	//Try closing the dl_test_syms module which the dl_test_imports module depends on
	dlclose(handle1);
	ASSERT(__dl_num_loaded_modules == 2, "dlclose closed used module");
	//Finally close the dl_test_imports module which implicitly also closes the dl_test_syms module
	dlclose(handle2);
	ASSERT(__dl_num_loaded_modules == 0, "dlclose failed to close all unused modules");
}

void test_dlsym_rtld_default(TestContext *ctx) {
	//Open both modules with their symbols exported
	void *handle1 = dlopen("rom:/dl_test_syms.dso", RTLD_GLOBAL);
	void *handle2 = dlopen("rom:/dl_test_imports.dso", RTLD_GLOBAL);
	DEFER(dlclose(handle2));
	DEFER(dlclose(handle1));
	//Do RTLD_DEFAULT symbol search of known duplicate symbol
	uint32_t *dl_test_ptr = dlsym(RTLD_DEFAULT, "dl_test_ptr");
	ASSERT(dl_test_ptr, "RTLD_DEFAULT search doesn't work"); //Check if symbol was found
	//Check if right symbol was found by RTLD_DEFAULT
	ASSERT(*dl_test_ptr == 0, "RTLD_DEFAULT search order wrong");
}

void test_dl_imports(TestContext *ctx) {
	//Open modules dl_test_syms (symbols exported) and dl_test_imports (symbols not exported)
	void *handle1 = dlopen("rom:/dl_test_syms.dso", RTLD_GLOBAL);
	void *handle2 = dlopen("rom:/dl_test_imports.dso", RTLD_LOCAL);
	DEFER(dlclose(handle1));
	DEFER(dlclose(handle2));
	//Find required symbols in both modules for testing imports
	char *test_sym = dlsym(handle1, "dl_test_sym");
	uint32_t *test_sym_ptr = dlsym(handle2, "dl_test_ptr");
	uint32_t *dlopen_ptr = dlsym(handle2, "dlopen_ptr");
	uint32_t *dfs_open_ptr = dlsym(handle2, "dfs_open_ptr");
	//Check if all required symbols are found
	ASSERT(test_sym, "Imported module symbol cannot be found");
	ASSERT(test_sym_ptr && dlopen_ptr && dfs_open_ptr, "Failed to find required symbols for testing module imports"); 
	//Check if imports between modules work properly
	ASSERT(*test_sym_ptr == (uint32_t)test_sym, "Imports between modules do not work properly");
	//Check if imports from the main executable work properly
	ASSERT((*dlopen_ptr) == (uint32_t)dlopen && (*dfs_open_ptr) == (uint32_t)dfs_open, "Main executable imports do not work properly");
}

void test_dl_relocs(TestContext *ctx) {
	//Open module to test relocations
	void *handle = dlopen("rom:/dl_test_relocs.dso", RTLD_LOCAL);
	DEFER(dlclose(handle));
	//Find required symbols to test relocations
	uint32_t *hilo = dlsym(handle, "dl_test_hilo_reloc");
	uint32_t *jump = dlsym(handle, "dl_test_jump_reloc");
	uint32_t *word = dlsym(handle, "dl_test_word_reloc");
	//Check if all required symbols are found
	ASSERT(hilo && jump && word, "Failed to find symbols for testing relocations");
	//Verify R_MIPS_HI16 and R_MIPS_LO16 relocations
	ASSERT(hilo_get_value(&hilo[0], &hilo[1]) == (uint32_t)jump+8, "Incorrect R_MIPS_HI16 and R_MIPS_LO16 handling");
	//Verify R_MIPS_26 relocations
	ASSERT(jump_get_target(&jump[0]) == (uint32_t)hilo+4, "Incorrect R_MIPS_26 relocation handling for JAL");
	ASSERT(jump_get_target(&jump[1]) == (uint32_t)jump+8, "Incorrect R_MIPS_26 relocation handling for J");
	//Verify R_MIPS_32 relocations 
	ASSERT((*word) == (uint32_t)hilo+4, "Incorrect R_MIPS_32 relocation handling");
}

void test_dl_syms(TestContext *ctx) {
	//Open module
	void *handle = dlopen("rom:/dl_test_syms.dso", RTLD_LOCAL);
	DEFER(dlclose(handle));
	//Find required symbols to test symbol lookup
	char *test_sym = dlsym(handle, "dl_test_sym");
	char *test_sym2 = dlsym(handle, "DLTestSym");
	//Check if both required symbols are found
	ASSERT(test_sym && test_sym2, "Failed to find required symbols");
	//Check if correct symbol is found
	ASSERT(strcmp(test_sym, "dl_test_sym") == 0 && strcmp(test_sym2, "DLTestSym") == 0, "Symbol searches do not work properly");
}
