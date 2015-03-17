/***************************************************************************
 * Copyright 2007 - 2012  VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Devices                                                        */ /**
 * \defgroup Device Device interface
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_DEVICE_TYPES_H_
#define _VMKAPI_DEVICE_TYPES_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Opaque device type 
 */
typedef struct vmkDevice* vmk_Device;

/** \brief A null device handle. */
#define VMK_DEVICE_NONE ((vmk_Device )0)

/**
 * \brief Opaque driver type.
 */
typedef struct vmkDriver* vmk_Driver; 

/** \brief Invalid driver handle. */
#define VMK_DRIVER_NONE ((vmk_Driver)0)

#endif /* _VMKAPI_DEVICE_TYPES_H_ */
/** @} */
