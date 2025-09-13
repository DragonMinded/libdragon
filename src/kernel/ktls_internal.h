/**
 * @file ktls_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_KERNEL_TLS_INTERNAL_H
#define LIBDRAGON_KERNEL_TLS_INTERNAL_H

/* TLS Linker symbols */
/** @brief TLS Base Linker Symbol */
extern char __tls_base[];
/** @brief TLS End Linker Symbol */
extern char __tls_end[];
/** @brief TLS data start (linker symbol) */
extern char __tdata_start[];
/** @brief TLS data end (linker symbol) */
extern char __tdata_end[];
/** @brief TLS BSS start (linker symbol) */
extern char __tbss_start[];
/** @brief TLS BSS end (linker symbol) */
extern char __tbss_end[];

/** @brief Size of .tdata section */
#define TDATA_SIZE      ((uint32_t)(__tdata_end) - (uint32_t)(__tdata_start))

/** @brief Size of .tdata and .tbss sections combined  */
#define TLS_SIZE        ((uint32_t)(__tls_end) - (uint32_t)(__tls_base))

/** @brief Offset of Pointer for TLS accesses */
#define TP_OFFSET       0x7000 

/** 
 * @brief Invalid thread pointer value 
 * 
 * This is used to indicate that the current thread pointer is invalid, which
 * happens during exception processing. If the code happens to access TLS,
 * it will dereference this, and the exception code in __get_exception_name will
 * be able to detect this and print an useful error message.
 */
#define KERNEL_TP_INVALID ((void *)0x5FFF8001)

extern void *__th_cur_tp;

#endif
