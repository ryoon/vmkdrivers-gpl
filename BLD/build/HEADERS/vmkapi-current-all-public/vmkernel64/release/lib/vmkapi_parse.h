/* **********************************************************
 * Copyright 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Parse                                                          */ /**
 * \addtogroup Lib
 * @{
 * \defgroup Parse Parsing Utilities
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_PARSE_H_
#define _VMKAPI_PARSE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_ParseArgs--                                                */ /**
 *
 * \brief Parse "buf" as a vector of arguments. 
 * 
 *        Parse "buf" of length "buflen" as a vector of up to "argc" 
 *        arguments delimited by whitespace. Updates "buf" in-place,
 *        replacing whitespace with NULs, and sets elements of "argv" to
 *        the start of each parsed argument. 
 *
 * \note  This function will not block.
 *
 * \param[in]  buf      Source string 
 * \param[in]  buflen   Length of source string
 * \param[out] argv     Array of parsed tokens
 * \param[in]  argc     Count of tokens to be parsed 
 *
 * \return Returns the number of parsed arguments.
 *
 ***********************************************************************
 */
vmk_uint32 vmk_ParseArgs(
   char *buf,
   vmk_ByteCount buflen,
   char *argv[],
   vmk_uint32 argc);

#endif /* _VMKAPI_PARSE_H_ */
/** @} */
/** @} */
