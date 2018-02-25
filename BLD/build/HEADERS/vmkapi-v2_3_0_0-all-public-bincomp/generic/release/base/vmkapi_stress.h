/* **********************************************************
 * Copyright 2008 - 2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Stress                                                         */ /**
 * \defgroup Stress Stress Options
 *
 * The stress option interfaces allow access to special environment
 * variables that inform code whether or not certain stress code
 * should be run.
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_STRESS_H_
#define _VMKAPI_STRESS_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * Useful stress option names
 */
/** \cond nodoc */
#define VMK_STRESS_OPT_NET_GEN_TINY_ARP_RARP          "NetGenTinyArpRarp"
#define VMK_STRESS_OPT_NET_IF_CORRUPT_ETHERNET_HDR    "NetIfCorruptEthHdr"
#define VMK_STRESS_OPT_NET_IF_CORRUPT_RX_DATA         "NetIfCorruptRxData"
#define VMK_STRESS_OPT_NET_IF_CORRUPT_RX_TCP_UDP      "NetIfCorruptRxTcpUdp"
#define VMK_STRESS_OPT_NET_IF_CORRUPT_TX              "NetIfCorruptTx"
#define VMK_STRESS_OPT_NET_IF_FAIL_HARD_TX            "NetIfFailHardTx"
#define VMK_STRESS_OPT_NET_IF_FAIL_RX                 "NetIfFailRx"
#define VMK_STRESS_OPT_NET_IF_FAIL_TX_AND_STOP_QUEUE  "NetIfFailTxAndStopQueue"
#define VMK_STRESS_OPT_NET_IF_FORCE_HIGH_DMA_OVERFLOW "NetIfForceHighDMAOverflow"
#define VMK_STRESS_OPT_NET_IF_FORCE_RX_SW_CSUM        "NetIfForceRxSWCsum"
#define VMK_STRESS_OPT_NET_NAPI_FORCE_BACKUP_WORLDLET "NetNapiForceBackupWorldlet"
#define VMK_STRESS_OPT_NET_BLOCK_DEV_IS_SLUGGISH      "NetBlockDevIsSluggish"

#define VMK_STRESS_OPT_SCSI_ADAPTER_ISSUE_FAIL        "ScsiAdapterIssueFail"

#define VMK_STRESS_OPT_VMKLINUX_DROP_CMD_SCSI_DONE    "VmkLinuxDropCmdScsiDone"
#define VMK_STRESS_OPT_VMKLINUX_ABORT_CMD_FAILURE     "VmkLinuxAbortCmdFailure"

#define VMK_STRESS_OPT_USB_BULK_DELAY_PROCESS_URB        "USBBulkDelayProcessURB"
#define VMK_STRESS_OPT_USB_BULK_URB_FAKE_TRANSIENT_ERROR "USBBulkURBFakeTransientError"
#define VMK_STRESS_OPT_USB_DELAY_PROCESS_TD              "USBDelayProcessTD"
#define VMK_STRESS_OPT_USB_FAIL_GP_HEAP_ALLOC            "USBFailGPHeapAlloc"
#define VMK_STRESS_OPT_USB_STORAGE_DELAY_SCSI_DATA_PHASE "USBStorageDelaySCSIDataPhase"
#define VMK_STRESS_OPT_USB_STORAGE_DELAY_SCSI_TRANSFER "USBStorageDelaySCSITransfer"
/** \endcond nodoc */

/**
 * \brief Opaque stress option handle.
 */
typedef vmk_uint64 vmk_StressOptionHandle;

/*
 ***********************************************************************
 * vmk_StressOptionOpen --                                        */ /**
 *
 * \ingroup Stress
 * \brief Open a handle to stress option.
 *
 * \note  This function will not block.
 *
 * \param[in]  name            A stress option name.
 * \param[out] handle          Handle to the stress option.
 *
 * \retval     VMK_OK          Successful.
 * \retval     VMK_BAD_PARAM   The stress option id was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StressOptionOpen(
   const char *name,
   vmk_StressOptionHandle *handle);

/*
 ***********************************************************************
 * vmk_StressOptionClose --                                       */ /**
 *
 * \ingroup Stress
 * \brief Close a handle to stress option.
 *
 * \note  This function will not block.
 *
 * \param[in]  handle          Handle to the stress option
 *
 * \retval     VMK_OK          Successful.
 * \retval     VMK_BAD_PARAM   The stress option handle was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StressOptionClose(
   vmk_StressOptionHandle handle);

/*
 ***********************************************************************
 * vmk_StressOptionValue --                                       */ /**
 *
 * \ingroup Stress
 * \brief Get stress option value.
 *
 * \note  This function will not block.
 *
 * \param[in]  handle          Handle to the stress option.
 * \param[out] result          Stress option value.
 *
 * \retval     VMK_OK          Successful.
 * \retval     VMK_BAD_PARAM   The stress option handle was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StressOptionValue(
   vmk_StressOptionHandle handle,
   vmk_uint32 *result);

/*
 ***********************************************************************
 * vmk_StressOptionCounter --                                     */ /**
 *
 * \ingroup Stress
 * \brief Generate a "hit" once in a while.
 *
 * Returns VMK_TRUE with probability 1 / N, where N is the current
 * value of the stress option. If the stress option is disabled (N = 0),
 * always returns VMK_FALSE.
 *
 * \note  This function will not block.
 *
 * \param[in]  handle          Handle to the stress option.
 * \param[out] result          VMK_TRUE if a "hit" was generated,
 *                             VMK_FALSE otherwise.
 *
 * \retval     VMK_OK          Successful.
 * \retval     VMK_BAD_PARAM   The stress option handle was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StressOptionCounter(
   vmk_StressOptionHandle handle,
   vmk_Bool *result);

/*
 ***********************************************************************
 * vmk_StressOptionRandValue --                                   */ /**
 *
 * \ingroup Stress
 * \brief Generate a random integer bound by the stress option.
 *
 * Returns a random positive integer less than the current value of the
 * stress option (or 0 if the current value is 0).
 *
 * \note  This function will not block.
 *
 * \param[in]  handle          Handle to the stress option.
 * \param[out] result          The random value.
 *
 * \retval     VMK_OK          Successful.
 * \retval     VMK_BAD_PARAM   The stress option handle was invalid.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_StressOptionRandValue(
   vmk_StressOptionHandle handle,
   vmk_uint32 *result);

#endif /* _VMKAPI_STRESS_H_ */
/** @} */
