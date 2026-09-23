/**
 * @file mg_ex.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __MG_EX_H
#define __MG_EX_H

#include "gl_internal.h"
#include "magma.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Extended version of #mg_draw which also suppports #GL_QUADS and #GL_QUAD_STRIP. */
void mg_ex_draw(const mg_input_assembly_parms_t *input_assembly_parms, uint32_t count, uint32_t first, GLenum mode);

/** @brief Extended version of #mg_draw_indexed which also suppports #GL_QUADS and #GL_QUAD_STRIP. */
void mg_ex_draw_indexed(const mg_input_assembly_parms_t *input_assembly_parms, const uint16_t *indices, uint32_t count, int32_t offset, GLenum mode);

#ifdef __cplusplus
}
#endif

#endif
