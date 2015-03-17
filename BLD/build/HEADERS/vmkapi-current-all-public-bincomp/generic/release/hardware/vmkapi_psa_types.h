/* **********************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */


/*
 ***********************************************************************
 * PSA                                                            */ /**
 * \addtogroup Device
 * @{
 * \defgroup PSA PSA Driver Interfaces
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PSA_H_
#define _VMKAPI_PSA_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */


/** \brief PSA Device Identifier */
#define VMK_SCSI_PSA_DRIVER_BUS_ID "com.vmware.HBAPort"
/** \brief PSA VPORT Device Identifier */
#define VMK_SCSI_PSA_DRIVER_VPORT_BUS_ID "com.vmware.Virtual_HBAPort"

#endif /* _VMKAPI_PSA_H_ */
/** @} */
/** @} */
