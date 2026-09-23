/**
 * @file data_view.h
 * @author Dennis Heinze <dennis.heinze@mailbox.org>
 */
#ifndef __DATA_VIEW_H
#define __DATA_VIEW_H

#include <stdint.h>

/** @brief Vertex data view. */
typedef struct data_view_s {
    void *pointer;      ///< Pointer to the vertex data.
    uint32_t stride;    ///< Vertex stride.
} data_view_t;

#endif
