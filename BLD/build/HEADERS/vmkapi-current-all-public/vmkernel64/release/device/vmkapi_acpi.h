/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * ACPI                                                           */ /**
 * \addtogroup Device
 * @{
 * \defgroup ACPI ACPI Device Information Interfaces
 * @{ 
 *
 * These interfaces deal with information about various device-associated
 * resources. The resources could be memory ranges, IO ranges, IRQs, etc.
 *
 * The interfaces deal only with device information for devices which have
 * a populated ACPI entry.
 *
 ***********************************************************************
 */

#ifndef _VMKAPI_ACPI_H_
#define _VMKAPI_ACPI_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#define VMK_ACPI_BUS_NAME "acpi"

/** \brief The Fixed IO range resource. */
typedef struct vmk_AcpiFixedIORange {
   vmk_uint64  start;
   vmk_uint64  end;
} vmk_AcpiFixedIORange;

/** \brief The Fixed Memory range resource. */
typedef struct vmk_AcpiFixedMemRange {
   vmk_Bool    writeProtect;
   vmk_uint64  start;
   vmk_uint64  end;
} vmk_AcpiFixedMemRange;

/** \brief The ACPI decode width. */
typedef enum {
   VMK_ACPI_DECODE_WIDTH_10 = 0,
   VMK_ACPI_DECODE_WIDTH_16 = 1   
} vmk_AcpiDecodeWidth;

/** \brief The dynamic IO range resource. */
typedef struct vmk_AcpiIORange {
   vmk_AcpiDecodeWidth decodeWidth;
   vmk_uint64  start;
   vmk_uint64  end;
   vmk_uint64  alignment;
} vmk_AcpiIORange;

/** \brief The dynamic memory range resource. */
typedef struct vmk_AcpiMemRange {
   vmk_Bool    writeProtect;
   vmk_uint64  start;
   vmk_uint64  end;
   vmk_uint64  alignment;
} vmk_AcpiMemRange;

/** \brief Max number of ACPI resources for a TPM device. */
#define VMK_ACPI_TPM_MAX_RESOURCES  10

/** \brief The device info capturing all the resources used. */
typedef struct vmk_AcpiTPMDevice {
   unsigned fixedIORangeLen;
   unsigned ioRangeLen;
   unsigned fixedMemRangeLen;
   unsigned memRangeLen;
   vmk_AcpiFixedIORange  fixedIO[VMK_ACPI_TPM_MAX_RESOURCES];
   vmk_AcpiIORange       io[VMK_ACPI_TPM_MAX_RESOURCES];
   vmk_AcpiFixedMemRange fixedMem[VMK_ACPI_TPM_MAX_RESOURCES];
   vmk_AcpiMemRange      mem[VMK_ACPI_TPM_MAX_RESOURCES];
} vmk_AcpiTPMDevice;

/** \brief What type of device info to query. */
typedef enum {
   VMK_ACPI_INFO_TPM = 0
} vmk_AcpiDeviceType;

/** \brief Device information. */
typedef struct vmk_AcpiDeviceInfo {
   vmk_AcpiDeviceType type;
   union {
      vmk_AcpiTPMDevice tpmInfo;
   } info;
} vmk_AcpiDeviceInfo;

/*
 ***********************************************************************
 * vmk_AcpiGetInfo --                                             */ /**
 *
 * \ingroup ACPI
 * \brief Use ACPI to get the information about resources used by some
 *        device. 
 *
 * \param[in] type         Type of the device to query.
 * \param[out] deviceInfo  ACPI information about the device.
 *
 * \retval VMK_BAD_PARAM If the deviceType passed is unsupported.
 * \retval VMK_FAILURE   If device is not found or any other error
 *                       because of which resource information could
 *                       not be obtained.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_AcpiGetInfo(vmk_AcpiDeviceType type,
                                 vmk_AcpiDeviceInfo *deviceInfo);

#endif /* _VMKAPI_ACPI_H_ */
/** @} */
/** @} */

