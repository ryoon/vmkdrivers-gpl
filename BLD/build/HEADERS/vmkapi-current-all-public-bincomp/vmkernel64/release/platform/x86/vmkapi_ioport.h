/***************************************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved.
 ***************************************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * IOPort                                                         */ /**
 * \addtogroup IOResource 
 * @{
 * \defgroup IOPort IO port interface.
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_IOPORT_H_
#define _VMKAPI_IOPORT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 ***********************************************************************
 * vmk_IOPortRead8 --                                             */ /**
 *
 * \brief       Read one byte from given port address.
 *
 * \note This function will not block.
 *
 * \param[in]   reservation     IOResource reservation handle.
 * \param[in]   port            Port address.
 * \param[out]  outValue        Result of read.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_ADDR_RANGE   Request doesn't match reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOPortRead8(vmk_IOReservation reservation,
                vmk_IOPortAddr port,
                vmk_uint8 *outValue);

/*
 ***********************************************************************
 * vmk_IOPortWrite8 --                                            */ /**
 *
 * \brief       Write one byte to given port address.
 *
 * \note This function will not block.
 *
 * \param[in]   reservation     IOResource reservation handle.
 * \param[in]   port            Port address.
 * \param[in]   inValue         Value to write.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_ADDR_RANGE   Request doesn't match reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOPortWrite8(vmk_IOReservation reservation,
                 vmk_IOPortAddr port,
                 vmk_uint8 inValue);
/*
 ***********************************************************************
 * vmk_IOPortRead16 --                                            */ /**
 *
 * \brief       Read two bytes from given port address.
 *
 * \note This function will not block.
 *
 * \param[in]   reservation     IOResource reservation handle.
 * \param[in]   port            Port address.
 * \param[out]  outValue        Result of read.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_ADDR_RANGE   Request doesn't match reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOPortRead16(vmk_IOReservation reservation,
                 vmk_IOPortAddr port,
                 vmk_uint16 *outValue);
/*
 ***********************************************************************
 * vmk_IOPortWrite16 --                                           */ /**
 *
 * \brief       Write two bytes to given port address.
 *
 * \note This function will not block.
 *
 * \param[in]   reservation     IOResource reservation handle.
 * \param[in]   port            Port address.
 * \param[in]   inValue         Value to write.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_ADDR_RANGE   Request doesn't match reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOPortWrite16(vmk_IOReservation reservation,
                  vmk_IOPortAddr port,
                  vmk_uint16 inValue);
/*
 ***********************************************************************
 * vmk_IOPortRead32 --                                            */ /**
 *
 * \brief       Read four bytes from given port address.
 *
 * \note This function will not block.
 *
 * \param[in]   reservation     IOResource reservation handle.
 * \param[in]   port            Port address.
 * \param[out]  outValue        Result of read.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_ADDR_RANGE   Request doesn't match reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOPortRead32(vmk_IOReservation reservation,
                 vmk_IOPortAddr port,
                 vmk_uint32 *outValue);
/*
 ***********************************************************************
 * vmk_IOPortRead32 --                                            */ /**
 *
 * \brief       Write four bytes to given port address.
 *
 * \note This function will not block.
 *
 * \param[in]   reservation     IOResource reservation handle.
 * \param[in]   port            Port address.
 * \param[in]   inValue         Value to write.
 *
 * \retval VMK_OK               Success.
 * \retval VMK_BAD_ADDR_RANGE   Request doesn't match reservation.
 *
 ***********************************************************************
 */
VMK_ReturnStatus 
vmk_IOPortWrite32(vmk_IOReservation reservation,
                  vmk_IOPortAddr port,
                  vmk_uint32 inValue);
#endif
/** @} */
/** @} */
