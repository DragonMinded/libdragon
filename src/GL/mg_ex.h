#ifndef __MG_EX_H
#define __MG_EX_H

#include "gl_internal.h"
#include "magma.h"

#ifdef __cplusplus
extern "C" {
#endif

void mg_ex_draw(const mg_input_assembly_parms_t *input_assembly_parms, uint32_t count, uint32_t first, GLenum mode);
void mg_ex_draw_indexed(const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t count, int32_t offset, GLenum mode);

#ifdef __cplusplus
}
#endif

#endif
