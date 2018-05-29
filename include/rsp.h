/**
 * @file rsp.h
 * @brief Hardware Vector Interface
 * @ingroup rsp
 */
#ifndef __LIBDRAGON_RSP_H
#define __LIBDRAGON_RSP_H

#ifdef __cplusplus
extern "C" {
#endif

void rsp_init();
void load_ucode(void * start, unsigned long size);
void read_ucode(void* start, unsigned long size);
void run_ucode();

#ifdef __cplusplus
}
#endif

#endif
