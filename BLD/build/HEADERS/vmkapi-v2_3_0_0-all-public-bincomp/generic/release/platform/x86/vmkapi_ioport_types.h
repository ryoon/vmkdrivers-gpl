/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

#ifndef _VMKAPI_IOPORT_TYPES_H_
#define _VMKAPI_IOPORT_TYPES_H_

/**
 * IOPort 
 * \defgroup IOPort IOPort 
 *
 * Constants and definitions for IO ports 
 * @{ 
 */

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/**
 * \brief An IO port address.
 */
typedef vmk_uint16 	   vmk_IOPortAddr;

#endif
/** @} */
