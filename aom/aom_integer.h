/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_INTEGER_H_
#define AOM_AOM_INTEGER_H_

/* get ptrdiff_t, size_t, wchar_t, NULL */
#include <stddef.h>

#if defined(_MSC_VER)
#define AOM_FORCE_INLINE __forceinline
#define AOM_INLINE __inline
#else
#define AOM_FORCE_INLINE __inline__ __attribute__((always_inline))
// TODO(jbb): Allow a way to force inline off for older compilers.
#define AOM_INLINE inline
#endif

#if defined(AOM_EMULATE_INTTYPES)
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#ifndef _UINTPTR_T_DEFINED
typedef size_t uintptr_t;
#endif

#else

/* Most platforms have the C99 standard integer types. */

#if defined(__cplusplus)
#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif
#if !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS
#endif
#endif  // __cplusplus

#include <stdint.h>

#endif

/* VS2010 defines stdint.h, but not inttypes.h */
#if defined(_MSC_VER) && _MSC_VER < 1800
#define PRId64 "I64d"
#else
#include <inttypes.h>
#endif

#if !defined(INT8_MAX)
#define INT8_MAX 127
#endif

#if !defined(INT32_MAX)
#define INT32_MAX 2147483647
#endif

#if !defined(INT32_MIN)
#define INT32_MIN (-2147483647 - 1)
#endif

#define NELEMENTS(x) (int)(sizeof(x) / sizeof(x[0]))

#if defined(__cplusplus)
extern "C" {
#endif  // __cplusplus

// Returns size of uint32_t when encoded using LEB128.
size_t aom_uleb_size_in_bytes(uint32_t value);

// Returns decoded LEB128 value.
void aom_uleb_decode(const uint8_t *buffer, size_t available, uint32_t *value);

// Encodes LEB128 integer. Returns 0 when successful, and -1 upon failure.
int aom_uleb_encode(uint32_t value, size_t available, uint8_t *coded_value,
                    size_t *coded_size);

// Encodes LEB128 integer to size specified. Returns 0 when successful, and -1
// upon failure.
int aom_uleb_encode_fixed_size(uint32_t value, size_t available,
                               size_t pad_to_size, uint8_t *coded_value,
                               size_t *coded_size);

#if defined(__cplusplus)
}  // extern "C"
#endif  // __cplusplus

#endif  // AOM_AOM_INTEGER_H_
