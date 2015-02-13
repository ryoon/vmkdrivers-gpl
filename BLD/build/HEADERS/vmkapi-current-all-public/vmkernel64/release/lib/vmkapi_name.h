/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Name                                                           */ /**
 * \addtogroup Lib
 * @{
 * \defgroup Name Names for kernel objects.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_NAME_H_
#define _VMKAPI_NAME_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief Maximum length for the name of varous kernel objects.
 */
#define VMK_MISC_NAME_MAX 32

/**
 * \brief Name of a kernel object.
 *
 * The name must be NUL-terminated.
 */
typedef struct vmk_Name {
   char string[VMK_MISC_NAME_MAX];
} vmk_Name;

/*
 ***********************************************************************
 * vmk_NameInitialize --                                          */ /**
 *
 * \brief Initialize a vmk_Name from a string literal.
 *
 * \note  This function will not block.
 *
 * \param[out] name     Name to be used for a kernel object.
 * \param[in]  string   String to initialize name to.
 *
 * \retval   VMK_OK             The name was successfully initialized.
 * \retval   VMK_LIMIT_EXCEEDED string was too long to fit in name.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_NameInitialize(vmk_Name *name, const char *string);

/*
 ***********************************************************************
 * vmk_NameFormat --                                              */ /**
 *
 * \brief Creates a name from a format string and arguments.
 *
 * \note  This function will not block.
 *
 * \printformatstringdoc
 *
 * \param[out] name    Name to receive output.
 * \param[in]  format  Format string
 *
 * \retval VMK_OK              The name was successfully formatted.
 * \retval VMK_LIMIT_EXCEEDED  The resulting name was too long.
 *
 ***********************************************************************
 */

VMK_ReturnStatus vmk_NameFormat(vmk_Name *name, const char *format, ...)
   VMK_ATTRIBUTE_PRINTF(2, 3);

/*
 ***********************************************************************
 * vmk_NameToString --                                            */ /**
 *
 * \brief Returns a pointer to the string for a name.
 *
 * \note  This function will not block.
 *
 * \param[in]  name     Name to convert.
 *
 * \retval   A string for the name.
 *
 ***********************************************************************
 */
const char * vmk_NameToString(const vmk_Name *name);

/*
 ***********************************************************************
 * vmk_NameCopy --                                                */ /**
 *
 * \brief Copies a source name to a destination name.
 *
 * \note  This function will not block.
 *
 * src is copied to dst.  No error checking is performed, so if
 * src is an invalid name, dst will also be an invalid name.
 *
 * \param[out] dst     Copy destination.
 * \param[in]  src     Name to copy.
 *
 ***********************************************************************
 */

void vmk_NameCopy(vmk_Name *dst, vmk_Name *src);

#endif /* _VMKAPI_TYPES_H_ */
/** @} */
/** @} */
