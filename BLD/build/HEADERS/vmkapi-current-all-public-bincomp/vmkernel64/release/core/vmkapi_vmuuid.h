/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * VM UUID                                                        */ /**
 * \addtogroup Core
 * @{
 * \defgroup VMUUID VM UUID
 *
 * The VM UUID is an unique identifier for a VM.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_VM_UUID_H_
#define _VMKAPI_VM_UUID_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief VM UUID
 */
typedef char* vmk_UUID;


/*
 ***********************************************************************
 * vmk_VMUUIDGetMaxLength --                                      */ /**
 *
 * \brief Query the maximum length of a null-terminated VM UUID string.
 *
 * \note The length of the null-terminated VM UUID does not change
 *       during system runtime.
 * \note This function will not block.
 *
 * \retval vmk_ByteCount   Maximum length of a null-terminated VM UUID
 *                         string in bytes.
 *
 ***********************************************************************
 */
vmk_ByteCount vmk_VMUUIDGetMaxLength(void);

#endif /* _VMKAPI_VM_UUID_H_ */
/** @} */
/** @} */
