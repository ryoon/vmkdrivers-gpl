/***************************************************************************
 * Copyright 2010,2012 VMware, Inc.  All rights reserved.
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

#ifndef _VMKAPI_BUS_H_
#define _VMKAPI_BUS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

typedef struct vmkBusType* vmk_BusType;

/** \brief A null bustype handle. */
#define VMK_BUSTYPE_NONE ((vmk_BusType)0)

/*
 ***********************************************************************
 * vmk_BusTypeFind --                                          */ /**
 *
 * \brief Return a reference to the bus type object registered with 
 *        given name. 
 *
 * \note  Reference must be freed using vmk_BusTypeRelease. 
 *        
 * \note This function will not block.
 *
 * \param[in]   name       Name of bus type 
 * \param[out]  busHandle  Handle to bus type object. 
 *
 * \retval VMK_OK          Success 
 * \retval VMK_NOT_FOUND   No bus type object matching given name. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_BusTypeFind(vmk_Name *name,
                vmk_BusType *busHandle);


/*
 ***********************************************************************
 * vmk_BusTypeRelease --                                          */ /**
 *
 * \brief Release a reference to a bus type object obtained using one 
 *        of the search functions.        
 *       
 * \note If last reference was released bus type object may be freed.
 *
 * \note This function will not block.
 *
 * \param[in]  busHandle   Bus type handle to release.
 *
 * \retval VMK_OK          Success 
 * \retval VMK_BAD_PARAM   No bus type object matching given handle. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_BusTypeRelease(vmk_BusType busHandle);


/*
 ***********************************************************************
 * vmk_BusTypeGetName --                                          */ /**
 *
 * \brief   Find the name of the bus type for given bus type handle. 
 *        
 * \note This function will not block.
 *
 *
 * \param[in]  busHandle   Bus type handle to find name for.
 * \param[out] name        Name of bus type.    
 *
 * \retval VMK_OK             Success 
 * \retval VMK_BAD_PARAM      No bus type object matching given handle. 
 *
 ***********************************************************************
 */
VMK_ReturnStatus
vmk_BusTypeGetName(vmk_BusType busHandle,
                   vmk_Name *name);

#endif /* _VMKAPI_BUS_H_ */
/** @} */
/** @} */
