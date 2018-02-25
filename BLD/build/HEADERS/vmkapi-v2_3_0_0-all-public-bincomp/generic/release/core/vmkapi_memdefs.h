/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 *****************************************************************************
 * Memory Types and Definitions                                         */ /**
 *
 * \addtogroup Core
 * @{
 * \defgroup MemDefs Memory Types and Definitions
 * @{
 *****************************************************************************
 */

#ifndef _VMKAPI_CORE_MEMDEFS_H_
#define _VMKAPI_CORE_MEMDEFS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

typedef vmk_uint64 vmk_PageAddress;
typedef vmk_uint64 vmk_PageNumber;

/**
 * \brief Machine Address
 */
typedef vmk_PageAddress vmk_MA;

/**
 * \brief Page number of a machine page.
 *
 * The machine page number is obtained by cutting the page offset from a page
 * address (machine page address >> page offset)
 */
typedef vmk_PageNumber vmk_MPN;

/**
 * \brief Guaranteed invalid machine page.
 */
#define VMK_INVALID_MPN ((vmk_MPN)-1)

/**
 * \brief Virtual Address
 */
typedef vmk_PageAddress vmk_VA;

/**
 * \brief Page Number of a virtual page.
 *
 * The virtual page number is obtained by cutting the page offset from a page
 * address (virtual page address >> page offset)
 */
typedef vmk_PageNumber vmk_VPN;

/**
 * \brief Guaranteed invalid virtual page.
 */
#define VMK_INVALID_VPN ((vmk_VPN)-1)

/**
 * \brief An address translated through an IOMMU for a device.
 *
 * This represents the address a device would use to access
 * main-memory using DMA where the address is translated through
 * an IOMMU. Systems without an IOMMU may still make use of this
 * address type to represent addresses presented to devices
 * for DMA.
 *
 * \note This is <b>not</b> the same as an IO-port address.
 */
typedef vmk_PageAddress vmk_IOA;

/**
 * \brief Page number of an IO page.
 */
typedef vmk_PageNumber vmk_IOPN;

/**
 * \brief Abstract address
 */
typedef union {
   vmk_VA addr;
   void *ptr;
} VMK_ATTRIBUTE_TRANSPARENT_UNION vmk_AddrCookie;

/**
 * \brief A collection of possible address types.
 */
typedef enum vmk_AddressType {
   VMK_ADDRESS_TYPE_NONE=0,
   VMK_ADDRESS_TYPE_UNKNOWN=1,
   VMK_ADDRESS_TYPE_VIRTUAL=2,
   VMK_ADDRESS_TYPE_MACHINE=3,
   VMK_ADDRESS_TYPE_IO=4,
} vmk_AddressType;

/*
 * Common address mask values
 */
#define VMK_ADDRESS_MASK_64BIT (~(VMK_CONST64U(0)))
#define VMK_ADDRESS_MASK_39BIT ((VMK_CONST64U(1) << 39) - VMK_CONST64U(1))
#define VMK_ADDRESS_MASK_32BIT ((VMK_CONST64U(1) << 32) - VMK_CONST64U(1))
#define VMK_ADDRESS_MASK_31BIT ((VMK_CONST64U(1) << 31) - VMK_CONST64U(1))

/**
 * \brief Physical contiguity property for memory allocators.
 */
typedef enum vmk_MemPhysContiguity {
   /** \brief All allocations will be physically contiguous. */
   VMK_MEM_PHYS_CONTIGUOUS,

   /** \brief Allocations may not be physically contiguous. */
   VMK_MEM_PHYS_ANY_CONTIGUITY,

   /** \brief Allocations will be physically discontiguous. */
   VMK_MEM_PHYS_DISCONTIGUOUS,
} vmk_MemPhysContiguity;

/**
 * \brief Address range restrictions for memory allocators.
 */
typedef enum vmk_MemPhysAddrConstraint {
   /** \brief Allocate memory without any address restrictions.
    *
    * This memory range should be used for all allocations that do not
    * have device-specific constraints.
    */
   VMK_PHYS_ADDR_ANY,

   /**
     * \brief Allocate memory below 2GB.
     *
     * Should be used sparingly, only when absolutely necessary for
     * handling a device (never for general purpose use - even in a
     * device driver).
     */
   VMK_PHYS_ADDR_BELOW_2GB,

   /**
     * \brief Allocate memory below 4GB.
     *
     * Should be used sparingly, only when absolutely necessary for
     * handling a device (never for general purpose use - even in a
     * device driver).
     */
   VMK_PHYS_ADDR_BELOW_4GB,

   /**
     * \brief Allocate memory below 512GB.
     *
     * Should be used sparingly, only when absolutely necessary for
     * handling a device (never for general purpose use - even in a
     * device driver).
     */
   VMK_PHYS_ADDR_BELOW_512GB,

} vmk_MemPhysAddrConstraint;

/*
 ***********************************************************************
 * vmk_MA2MPN --                                                  */ /**
 *
 * \brief Convert a machine address into a machine page number.
 * 
 * \param[in] ma   Machine address to convert.
 *
 * \return Machine page number for the given machine address.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_MPN
vmk_MA2MPN(vmk_MA ma)
{
   return (ma >> VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_MPN2MA --                                                  */ /**
 *
 * \brief Convert a machine page number into a machine address.
 * 
 * \param[in] mpn   Machine page number to convert.
 *
 * \return Machine page address for the given machine page number.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_MA
vmk_MPN2MA(vmk_MPN mpn)
{
   return (mpn << VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_VA2VPN --                                                  */ /**
 *
 * \brief Convert a virtual address into a virtual page number.
 * 
 * \param[in] va   Virtual address to convert.
 *
 * \return Virtual page number for the given virtual address.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_VPN
vmk_VA2VPN(vmk_VA va)
{
   return (va >> VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_VPN2VA --                                                  */ /**
 *
 * \brief Convert a virtual page number into a virtual address.
 * 
 * \param[in] vpn   Virtual page number to convert.
 *
 * \return Virtual page address for the given virtual page number.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_VA
vmk_VPN2VA(vmk_VPN vpn)
{
   return (vpn << VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_IOA2IOPN --                                                */ /**
 *
 * \brief Convert a IO address into a IO page number.
 * 
 * \param[in] ioa   IO address to convert.
 *
 * \return IO page number for the given IO address.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_IOPN
vmk_IOA2IOPN(vmk_IOA ioa)
{
   return (ioa >> VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_IOPN2IOA --                                                */ /**
 *
 * \brief Convert a IO page number into a IO address.
 * 
 * \param[in] iopn   IO page number to convert.
 *
 * \return IO page address for the given IO page number.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_IOA
vmk_IOPN2IOA(vmk_IOPN iopn)
{
   return (iopn << VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_Bytes2Pages --                                             */ /**
 *
 * \brief Convert a byte number into a page number.
 * 
 * \param[in] nBytes   Number of bytes to convert.
 *
 * \return page number for the given byte number.
 *
 * \note    This function will not block.
 * \note    This function returns the page value this byte exists
 *          in, not the total number of pages required to hold this
 *          many bytes.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageNumber
vmk_Bytes2Pages(vmk_PageAddress nBytes)
{
   return nBytes >> VMK_PAGE_SHIFT;
}


/*
 ***********************************************************************
 * vmk_Pages2Bytes --                                             */ /**
 *
 * \brief Convert a page number into a byte number.
 * 
 * \param[in] nPages   Number of pages to convert.
 *
 * \return byte number for the given page number.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageAddress
vmk_Pages2Bytes(vmk_PageNumber nPages)
{
   return nPages << VMK_PAGE_SHIFT;
}


/*
 ***********************************************************************
 * vmk_MBytes2Pages --                                            */ /**
 *
 * \brief Convert a megabyte number into a page number.
 * 
 * \param[in] nMBytes   Number of megabytes to convert.
 *
 * \return page number for the given megabyte number.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageNumber
vmk_MBytes2Pages(vmk_PageAddress nMBytes)
{
   return nMBytes << (20 - VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_Pages2MBytes --                                            */ /**
 *
 * \brief Convert a page number into a megabyte number.
 * 
 * \param[in] nPages   Number of pages to convert.
 *
 * \return megabyte number for the given page number.
 *
 * \note    This function will not block.
 * \note    This function returns the megabyte value this page exists
 *          in, not the total number of megabytes required to hold this
 *          many pages.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageAddress
vmk_Pages2MBytes(vmk_PageNumber nPages)
{
   return nPages >> (20 - VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_GBytes2Pages --                                            */ /**
 *
 * \brief Convert a gigabyte number into a page number.
 * 
 * \param[in] nGBytes   Number of gigabytes to convert.
 *
 * \return page number for the given gigabyte number.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageNumber
vmk_GBytes2Pages(vmk_PageAddress nGBytes)
{
   return nGBytes << (30 - VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_Pages2GBytes --                                            */ /**
 *
 * \brief Convert a page number into a gigabyte number.
 * 
 * \param[in] nPages   Number of pages to convert.
 *
 * \return gigabyte number for the given page number.
 *
 * \note    This function will not block.
 * \note    This function returns the gigabyte value this page exists
 *          in, not the total number of gigabytes required to hold this
 *          many pages.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageAddress
vmk_Pages2GBytes(vmk_PageNumber nPages)
{
   return nPages >> (30 - VMK_PAGE_SHIFT);
}


/*
 ***********************************************************************
 * vmk_Bytes2MBytes --                                            */ /**
 *
 * \brief Convert a byte number into a megabyte number.
 * 
 * \param[in] nBytes   Number of bytes to convert.
 *
 * \return megabyte number for the given byte number.
 *
 * \note    This function will not block.
 * \note    This function returns the megabyte value this byte exists
 *          in, not the total number of megabytes required to hold this
 *          many bytes.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageAddress
vmk_Bytes2MBytes(vmk_PageAddress nBytes)
{
   return nBytes >> 20;
}


/*
 ***********************************************************************
 * vmk_MBytes2Bytes --                                            */ /**
 *
 * \brief Convert a megabyte number into a byte number.
 * 
 * \param[in] nMBytes   Number of megabytes to convert.
 *
 * \return byte number for the given megabyte number.
 *
 * \note    This function will not block.
 * \note    This function returns the byte value this megabyte exists
 *          in, not the total number of bytes required to hold this
 *          many pages.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageAddress
vmk_MBytes2Bytes(vmk_PageNumber nMBytes)
{
   return nMBytes << 20;
}


/*
 ***********************************************************************
 * vmk_Bytes2GBytes --                                            */ /**
 *
 * \brief Convert a byte number into a gigabyte number.
 * 
 * \param[in] nBytes   Number of bytes to convert.
 *
 * \return gigabyte number for the given byte number.
 *
 * \note    This function will not block.
 * \note    This function returns the gigabyte value this byte exists
 *          in, not the total number of gigabytes required to hold this
 *          many bytes.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageAddress
vmk_Bytes2GBytes(vmk_PageAddress nBytes)
{
   return nBytes >> 30;
}


/*
 ***********************************************************************
 * vmk_GBytes2Bytes --                                            */ /**
 *
 * \brief Convert a gigabyte number into a byte number.
 * 
 * \param[in] nGBytes   Number of gigabytes to convert.
 *
 * \return byte number for the given gigabyte number.
 *
 * \note    This function will not block.
 *
 ***********************************************************************
 */

static VMK_ALWAYS_INLINE vmk_PageAddress
vmk_GBytes2Bytes(vmk_PageAddress nGBytes)
{
   return nGBytes << 30;
}


#endif
/** @} */
/** @} */
