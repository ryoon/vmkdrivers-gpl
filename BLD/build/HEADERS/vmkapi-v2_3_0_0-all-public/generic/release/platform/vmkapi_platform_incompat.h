/* **********************************************************
 * Copyright 2013 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

#ifndef _VMKAPI_PLATFORM_INCOMPAT_H
#define _VMKAPI_PLATFORM_INCOMPAT_H

/*
 ***********************************************************************
 * vmk_EFIIsPresent --                                            */ /**
 *
 * \ingroup Platform
 * \brief Determines if EFI is present in the system.
 *
 * \retval VMK_TRUE   EFI is present in the system.
 * \retval VMK_FALSE  EFI is not present in the system.
 *
 ***********************************************************************
 */
vmk_Bool vmk_EFIIsPresent(void);

/*
 ***********************************************************************
 * vmk_EFIGetSMBIOSEntryPoint --                                  */ /**
 *
 * \ingroup Platform
 * \brief Returns a pointer to the SMBIOS entry point structure.
 *
 * \param[out] smbios     Pointer to SMBIOS entry point structure.
 *
 * \retval VMK_OK         Success.
 * \retval VMK_NOT_FOUND  SMBIOS entry point structure not found.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_EFIGetSMBIOSEntryPoint(void **smbios);

#endif
