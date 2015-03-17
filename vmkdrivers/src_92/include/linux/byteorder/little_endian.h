#ifndef _LINUX_BYTEORDER_LITTLE_ENDIAN_H
#define _LINUX_BYTEORDER_LITTLE_ENDIAN_H

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif
#ifndef __LITTLE_ENDIAN_BITFIELD
#define __LITTLE_ENDIAN_BITFIELD
#endif

#include <linux/types.h>
#include <linux/byteorder/swab.h>

/**                                          
 *  __constant_htonl - Convert constant long int from host to network byte
 *  @x: 32-bit constant that is to be converted 
 *                                           
 *  Converts given constant 32-bit value from host to network byte order
 *
 *  SYNOPSIS:
 *     __constant_htonl(x)
 *                                           
 *  RETURN VALUE:                     
 *  32-bit value in network byte order
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __constant_htonl  */
#define __constant_htonl(x) ((__force __be32)___constant_swab32((x)))
#define __constant_ntohl(x) ___constant_swab32((__force __be32)(x))
/**                                          
 *  __constant_htons - Convert constant short int from host to network byte
 *  @x: 16-bit constant that is to be converted 
 *                                           
 *  Converts given constant 16-bit value from host to network byte order
 *
 *  SYNOPSIS:
 *     __constant_htons(x)
 *                                           
 *  RETURN VALUE:                     
 *  16-bit value in network byte order
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __constant_htons  */
#define __constant_htons(x) ((__force __be16)___constant_swab16((x)))
#define __constant_ntohs(x) ___constant_swab16((__force __be16)(x))
#define __constant_cpu_to_le64(x) ((__force __le64)(__u64)(x))
#define __constant_le64_to_cpu(x) ((__force __u64)(__le64)(x))
/**                                          
 *  __constant_cpu_to_le32 - Convert constant from cpu to 32-bit little endian
 *  @x: 32-bit constant that is to be converted 
 *                                           
 *  Converts given constant 32-bit value from native cpu to little endian
 *
 *  SYNOPSIS:
 *     __constant_cpu_to_le32(x)
 *                                           
 *  RETURN VALUE:                     
 *  little endian 32-bit value 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __constant_cpu_to_le32  */
#define __constant_cpu_to_le32(x) ((__force __le32)(__u32)(x))
#define __constant_le32_to_cpu(x) ((__force __u32)(__le32)(x))
/**                                          
 *  __constant_cpu_to_le16 - Convert constant from cpu to 16-bit little endian
 *  @x: 16-bit constant that is to be converted 
 *                                           
 *  Converts given constant 16-bit value from native cpu to little endian
 *
 *  SYNOPSIS:
 *     __constant_cpu_to_le16(x)
 *                                           
 *  RETURN VALUE:                     
 *  little endian 16-bit value 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __constant_cpu_to_le16  */
#define __constant_cpu_to_le16(x) ((__force __le16)(__u16)(x))
#define __constant_le16_to_cpu(x) ((__force __u16)(__le16)(x))
#define __constant_cpu_to_be64(x) ((__force __be64)___constant_swab64((x)))
#define __constant_be64_to_cpu(x) ___constant_swab64((__force __u64)(__be64)(x))
/**                                          
 *  __constant_cpu_to_be32 - Convert constant from cpu to 32-bit big endian
 *  @x: 32-bit constant that is to be converted 
 *                                           
 *  Converts given constant 32-bit value from native cpu to big endian
 *
 *  SYNOPSIS:
 *     __constant_cpu_to_be32(x)
 *                                           
 *  RETURN VALUE:                     
 *  big endian 32-bit value 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __constant_cpu_to_be32  */
#define __constant_cpu_to_be32(x) ((__force __be32)___constant_swab32((x)))
#define __constant_be32_to_cpu(x) ___constant_swab32((__force __u32)(__be32)(x))
/**                                          
 *  __constant_cpu_to_be16 - Convert constant from cpu to 16-bit big endian
 *  @x: 16-bit constant that is to be converted 
 *                                           
 *  Converts given constant 16-bit value from native cpu to big endian
 *
 *  SYNOPSIS:
 *     __constant_cpu_to_be16(x)
 *                                           
 *  RETURN VALUE:                     
 *  big endian 16-bit value 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __constant_cpu_to_be16  */
#define __constant_cpu_to_be16(x) ((__force __be16)___constant_swab16((x)))
#define __constant_be16_to_cpu(x) ___constant_swab16((__force __u16)(__be16)(x))

#define __cpu_to_le64(x) ((__force __le64)(__u64)(x))
#define __le64_to_cpu(x) ((__force __u64)(__le64)(x))
/**                                          
 *  __cpu_to_le32 - Convert from native cpu to 32-bit little endian
 *  @x: 32-bit value that is to be converted 
 *                                           
 *  Converts given unsigned 32-bit value from native cpu to little endian
 *
 *  SYNOPSIS:
 *     __cpu_to_le32(x)
 *                                           
 *  RETURN VALUE:                     
 *  little endian 32-bit value 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __cpu_to_le32 */
#define __cpu_to_le32(x) ((__force __le32)(__u32)(x))
#define __le32_to_cpu(x) ((__force __u32)(__le32)(x))

#define __cpu_to_le16(x) ((__force __le16)(__u16)(x))
#define __le16_to_cpu(x) ((__force __u16)(__le16)(x))

#define __cpu_to_be64(x) ((__force __be64)__swab64((x)))
#define __be64_to_cpu(x) __swab64((__force __u64)(__be64)(x))
#define __cpu_to_be32(x) ((__force __be32)__swab32((x)))
#define __be32_to_cpu(x) __swab32((__force __u32)(__be32)(x))

#define __cpu_to_be16(x) ((__force __be16)__swab16((x)))
#define __be16_to_cpu(x) __swab16((__force __u16)(__be16)(x))

static inline __le64 __cpu_to_le64p(const __u64 *p)
{
	return (__force __le64)*p;
}
static inline __u64 __le64_to_cpup(const __le64 *p)
{
	return (__force __u64)*p;
}
/**                                          
 *  __cpu_to_le32p - Convert from native cpu to 32-bit little endian
 *  @p: Pointer to value that is to be converted 
 *                                           
 *  Converts given 32-bit value from native cpu to little endian
 *
 *  RETURN VALUE:                     
 *  little endian 32-bit value
 *                                           
 */
/* _VMKLNX_CODECHECK_:   __cpu_to_le32p */
static inline __le32 __cpu_to_le32p(const __u32 *p)
{
	return (__force __le32)*p;
}
static inline __u32 __le32_to_cpup(const __le32 *p)
{
	return (__force __u32)*p;
}
/**                                          
 *  __cpu_to_le16p - Convert from cpu to little endian
 *  @p: Pointer to value that is to be converted 
 *                                           
 *  Converts given unsigned 16 bit value to little endian                       
 *
 *  SYNOPSIS:
 *     __cpu_to_le16p(const __u16 *p)
 *                                           
 *  RETURN VALUE:                     
 *  little endian 16-bit value 
 *                                           
 */                                          
/* _VMKLNX_CODECHECK_: __cpu_to_le16p */
static inline __le16 __cpu_to_le16p(const __u16 *p)
{
	return (__force __le16)*p;
}
static inline __u16 __le16_to_cpup(const __le16 *p)
{
	return (__force __u16)*p;
}
static inline __be64 __cpu_to_be64p(const __u64 *p)
{
	return (__force __be64)__swab64p(p);
}
static inline __u64 __be64_to_cpup(const __be64 *p)
{
	return __swab64p((__u64 *)p);
}

/**                                          
 *  __cpu_to_be32p - Convert from native cpu to 32-bit big endian 
 *  @p: Pointer to value that is to be converted 
 *                                           
 *  Converts given 32-bit value from native cpu to big endian
 *
 *  RETURN VALUE:                     
 *  big endian 32-bit value
 *                                           
 */
/* _VMKLNX_CODECHECK_:   __cpu_to_be32p */
static inline __be32 __cpu_to_be32p(const __u32 *p)
{
	return (__force __be32)__swab32p(p);
}
static inline __u32 __be32_to_cpup(const __be32 *p)
{
	return __swab32p((__u32 *)p);
}

/**                                          
 *  __cpu_to_be16p - Convert from native cpu to 16-bit big endian 
 *  @p: Pointer to value that is to be converted 
 *                                           
 *  Converts given 16-bit value from native cpu to big endian
 *
 *  RETURN VALUE:                     
 *  big endian 16-bit value
 *                                           
 */
/* _VMKLNX_CODECHECK_:   __cpu_to_be16p */
static inline __be16 __cpu_to_be16p(const __u16 *p)
{
	return (__force __be16)__swab16p(p);
}
static inline __u16 __be16_to_cpup(const __be16 *p)
{
	return __swab16p((__u16 *)p);
}
#define __cpu_to_le64s(x) do {} while (0)
#define __le64_to_cpus(x) do {} while (0)
#define __cpu_to_le32s(x) do {} while (0)
#define __le32_to_cpus(x) do {} while (0)
#define __cpu_to_le16s(x) do {} while (0)
#define __le16_to_cpus(x) do {} while (0)
#define __cpu_to_be64s(x) __swab64s((x))
#define __be64_to_cpus(x) __swab64s((x))
#define __cpu_to_be32s(x) __swab32s((x))
#define __be32_to_cpus(x) __swab32s((x))
#define __cpu_to_be16s(x) __swab16s((x))
#define __be16_to_cpus(x) __swab16s((x))

#include <linux/byteorder/generic.h>

#endif /* _LINUX_BYTEORDER_LITTLE_ENDIAN_H */
