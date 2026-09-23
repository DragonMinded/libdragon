/**
 * @file array_convert.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __ARRAY_CONVERT_H
#define __ARRAY_CONVERT_H

#include "indices.h"
#include "array.h"

/** @brief Describes the output vertex layout for #array_convert */
typedef struct data_layout_s {
    /**
     * @brief The list of offsets for each vertex attribute. 
     * 
     * NOTE: These correspond 1:1 to the entries in \ref array_convert_parms_t.arrays and *not* to #array_type_t.
     */
    uint32_t offsets[ARRAY_COUNT];
    uint32_t stride; ///< The output vertex stride.
} data_layout_t;

/** @brief Parameter struct for #array_convert */
typedef struct array_convert_parms_s {
    const array_t *arrays[ARRAY_COUNT]; ///< List of arrays to convert from.
    uint32_t array_count;               ///< Number of entries in #arrays (Max is #ARRAY_COUNT).
    const data_layout_t *out_layout;    ///< Describes the desired layout of the output data.
    index_bounds_t range;               ///< The range of input vertices to convert. NOTE: Does *not* offset the output!
    void *out_buffer;                   ///< The output buffer.
} array_convert_parms_t;

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Unified function to convert vertex data from arrays into RSP-compatible format. */
void array_convert(array_convert_parms_t *parms);

#ifdef __cplusplus
}
#endif

#endif
