
typedef void (*func_ptr)(void);

extern func_ptr __CTOR_LIST__[];
extern func_ptr __CTOR_END__[];

void __do_global_ctors() {
	unsigned int tot_constructors = *(unsigned int*)(__CTOR_LIST__);
	for (void (**f)(void) = (void (**)(void))(__CTOR_LIST__ + 1); tot_constructors > 0; tot_constructors--, f++)
		(**f)();
}
