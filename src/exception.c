#include "exception.h"
#include <string.h>

static void (*__exception_handler)(TExceptionBlock*) = NULL;
extern const unsigned char* __baseRegAddr;

void register_exception_handler( void (*cb)(TExceptionBlock*))
{
	__exception_handler = cb;
}

const char* __get_exception_name(uint32_t etype)
{
	static const char* exceptionMap[] =
	{
		"Exception infomap not implemented",
		NULL,
	};

    /** @todo Implement exceptionMap , then calculate the offset */
	etype = 0;

	return exceptionMap[(int32_t)etype];
}

void __fetch_regs(TExceptionBlock* e,int32_t type)
{
	e->regs = (volatile const TRegBlock*) __baseRegAddr;
	e->type = type;
	e->info = __get_exception_name((uint32_t)e->regs->gpr[30]);
}

void __onCriticalException()
{
	TExceptionBlock e;
	
	if(!__exception_handler) { return; }

	__fetch_regs(&e,EXCEPTION_TYPE_CRITICAL);
	__exception_handler(&e);
}

void __onResetException()
{
	TExceptionBlock e;
	
	if(!__exception_handler) { return; }

	__fetch_regs(&e,EXCEPTION_TYPE_RESET);
	__exception_handler(&e);
}

