/* bnx2x_esx.h: Broadcom Everest network driver.
 *
 * Copyright 2008-2011 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Written by: Benjamin Li
 * PASSTHRU code by: Shmulik Ravid
 *
 */
#ifndef BNX2X_ESX_H
#define BNX2X_ESX_H

#ifdef BNX2X_VMWARE_BMAPILNX /* ! BNX2X_UPSTREAM */
int bnx2x_ioctl_cim(struct net_device *dev, struct ifreq *ifr);
#endif

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
int bnx2x_netqueue_ops(vmknetddi_queueops_op_t op, void *args);
void bnx2x_reserve_netq_feature(struct bnx2x *bp);
int bnx2x_netq_sp_event(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			int cid, int command);
void bnx2x_netq_clear_rx_queues(struct bnx2x *bp);
#endif

#ifdef BCM_IOV

/* forward */
struct bnx2x_virtf;

#ifdef BNX2X_PASSTHRU
VMK_ReturnStatus
bnx2x_pt_passthru_ops(void *client_data, vmk_NetPTOP op, void *args);
#endif

int
bnx2x_vmk_pci_dev(struct bnx2x *bp, vmk_PCIDevice *vmkPfDev);

int
bnx2x_vmk_vf_pci_dev(struct bnx2x *bp, u16 abs_vfid, vmk_PCIDevice *vmkVfDev);

int
bnx2x_vmk_pci_read_config_byte(vmk_PCIDevice dev, vmk_uint16 offset, vmk_uint8 *val);

int
bnx2x_vmk_pci_read_config_word(vmk_PCIDevice dev, vmk_uint16 offset, vmk_uint16 *val);

int
bnx2x_vmk_pci_read_config_dword(vmk_PCIDevice dev, vmk_uint16 offset, vmk_uint32 *val);

/* Must be called only after VF-Enable*/
int
bnx2x_vmk_vf_bus(struct bnx2x *bp, int vfid);

/* Must be called only after VF-Enable*/
int
bnx2x_vmk_vf_devfn(struct bnx2x *bp, int vfid);

void
bnx2x_vmk_vf_set_bars(struct bnx2x *bp, struct bnx2x_virtf *vf);

void
bnx2x_vmk_get_sriov_cap_pos(struct bnx2x *bp, vmk_uint16 *pos);

#endif

#endif  /* BNX2X_ESX_H */
