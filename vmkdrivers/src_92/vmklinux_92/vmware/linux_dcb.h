/* ****************************************************************
 * Copyright 2010-2013 VMware, Inc.  All rights reserved.
 * ****************************************************************/

#ifndef _LINUX_DCB_H_
#define _LINUX_DCB_H_

extern VMK_ReturnStatus
NICDCBIsEnabled(void *device, vmk_Bool *state, vmk_DCBVersion *version);

extern VMK_ReturnStatus
NICDCBEnable(void *device);

extern VMK_ReturnStatus
NICDCBDisable(void *device);

extern VMK_ReturnStatus
NICDCBGetNumTCs(void *device, vmk_DCBNumTCs *tcsInfo);

extern VMK_ReturnStatus
NICDCBGetPriorityGroup(void *device, vmk_DCBPriorityGroup *vmkpg);

extern VMK_ReturnStatus
NICDCBSetPriorityGroup(void *device, vmk_DCBPriorityGroup *vmkpg);

extern VMK_ReturnStatus
NICDCBGetPFCCfg(void *device, vmk_DCBPriorityFlowControlCfg *cfg);

extern VMK_ReturnStatus
NICDCBSetPFCCfg(void *device, vmk_DCBPriorityFlowControlCfg *cfg);

extern VMK_ReturnStatus
NICDCBIsPFCEnabled(void *device, vmk_Bool *state);

extern VMK_ReturnStatus
NICDCBEnablePFC(void *device);

extern VMK_ReturnStatus
NICDCBDisablePFC(void *device);

extern VMK_ReturnStatus
NICDCBGetApplications(void *device, vmk_DCBApplications *apps);

extern VMK_ReturnStatus
NICDCBSetApplication(void *device, vmk_DCBApplication *app);

extern VMK_ReturnStatus
NICDCBGetCapabilities(void *device, vmk_DCBCapabilities *caps);

extern VMK_ReturnStatus
NICDCBApplySettings(void *device);

extern VMK_ReturnStatus
NICDCBGetSettings(void *device, vmk_DCBConfig *dcb);

extern int
vmklnx_cna_update_fcoe_priority(char *vmnic_name, vmk_uint8 priority);

extern VMK_ReturnStatus
NICDCBSetIEEEEtsCfg(void *device, vmk_DCBIEEEEtsCfg *etsCfg);

extern VMK_ReturnStatus
NICDCBGetIEEEEtsCfg(void *device, vmk_DCBIEEEEtsCfg *etsCfg);

extern VMK_ReturnStatus
NICDCBGetIEEEPfcCfg(void *device, vmk_DCBIEEEPfcCfg *pfccfg);

extern VMK_ReturnStatus
NICDCBSetIEEEPfcCfg(void *device, vmk_DCBIEEEPfcCfg *pfccfg);

extern VMK_ReturnStatus
NICDCBGetIEEEAppCfg(void *device, vmk_DCBApplication *appcfg);

extern VMK_ReturnStatus
NICDCBSetIEEEAppCfg(void *device, vmk_DCBApplication *appcfg);

extern VMK_ReturnStatus
NICDCBDelIEEEAppCfg(void *device, vmk_DCBApplication *appcfg);

extern VMK_ReturnStatus
NICDCBGetDcbxMode(void *device, vmk_DCBSubType *dcbx_mode);

extern VMK_ReturnStatus
NICDCBSetDcbxMode(void *device, vmk_DCBSubType dcbx_mode);

#endif /*_LINUX_DCB_H_ */
