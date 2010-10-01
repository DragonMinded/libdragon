#ifndef __exception__h__
#define __exception__h__

#include <stdint.h>

enum
{
	EXCEPTION_TYPE_UNKNOWN = 0,
	EXCEPTION_TYPE_RESET,
	EXCEPTION_TYPE_CRITICAL
};

typedef volatile struct TRegBlock/*DO NOT modify the order unless editing inthandler.S*/
{
	volatile uint64_t gpr[32];
	volatile uint32_t sr;
	volatile uint32_t epc;
	volatile uint64_t hi;
	volatile uint64_t lo;
	volatile uint64_t fc31;
	volatile uint64_t fpr[32];
}TRegBlock;

typedef struct TExceptionBlock
{
	int32_t type;/*Unknown,reset/etc*/
	const char* info;/*type of exception (points to exception map + calculated offset)*/
	volatile const TRegBlock* regs;/*reg block*/
}TExceptionBlock;

#ifdef __cplusplus
extern "C" {
#endif

void register_exception_handler( void (*cb)(TExceptionBlock*) );

#ifdef __cplusplus
}
#endif

#endif


