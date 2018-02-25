
/***************************************************************************
 * Copyright 2012 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Bus                                                            */ /**
 * \addtogroup Device 
 * @{
 * \defgroup Bus Bus interface
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_BUS_INCOMPAT_H_
#define _VMKAPI_BUS_INCOMPAT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Properties of a bus type needed for registration. */
typedef struct {
   /** Module registering the bustype */
   vmk_ModuleID moduleID;
   /** Name of bustype */
   vmk_Name name;
} vmk_BusTypeProps;

/*
 ***********************************************************************
 * vmk_BusTypeRegister --                                          */ /**
 *
 * \brief Register a bus type with the device subsystem.
 *
 * \note This function will not block.
 *
 * \param[in]   busProps   Bus type description data.
 * \param[out]  busHandle  Handle to registered bus type.
 *
 * \retval VMK_OK         Success.
 * \retval VMK_EXISTS     Bus type is already registered.
 * \retval VMK_NO_MEMORY  Unable to allocate memory for bus type handle.
 * \retval VMK_BAD_PARAM  Input parameter is invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_BusTypeRegister(vmk_BusTypeProps *busProps,
                    vmk_BusType *busHandle);


/*
 ***********************************************************************
 * vmk_BusTypeUnregister --                                        */ /**
 *
 * \brief Unregister a bus type.
 *        
 * \note This function will not block.
 *
 * \param[in]   busHandle Handle to registered bus type. 
 *
 * \retval VMK_OK          Success 
 * \retval VMK_BAD_PARAM   No bus type object matching given handle. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_BusTypeUnregister(vmk_BusType busHandle);

#endif /* _VMKAPI_BUS_INCOMPAT_H_ */
/** @} */
/** @} */
