/*
 * errno.h - kernel-internal error numbers (M4).
 *
 * Values are the Linux x86_64 ABI numbers so that the Ring 3 header
 * include/abi/errno.h can adopt the same values in M5 without renumbering any
 * existing caller. Kernel functions return 0 on success or a negated value
 * from this header.
 *
 * This is deliberately a kernel header: user code must never include it. The
 * ABI copy is introduced with the first syscall milestone.
 */
#ifndef ATAOS_KERNEL_ERRNO_H
#define ATAOS_KERNEL_ERRNO_H

#define ENOENT 2  /* No such file or directory */
#define EIO    5  /* I/O error */
#define ENOMEM 12 /* Out of memory */
#define EFAULT 14 /* Bad address */
#define EBUSY  16 /* Device or resource busy */
#define EEXIST 17 /* File exists */
#define EINVAL 22 /* Invalid argument */

#endif /* ATAOS_KERNEL_ERRNO_H */
