/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 */

/*
 * Source file for tools routines
 */
#include <asm/types.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <net/sock.h>

#include "unm_nic.h"
#include "nic_phan_reg.h"

#include "unm_version.h"
#include "unm_nic_ioctl.h"
#include "nxhal_nic_interface.h"
#include "nxhal.h"

#include "nic_cmn.h"
#include "nx_license.h"

/* AEL1002 supports 3 device addresses: 1(PMA/PMD), 3(PCS), and 4(PHY XS) */
#define DEV_PMA_PMD 1
#define DEV_PCS     3
#define DEV_PHY_XS  4

/* Aeluros-specific registers use device address 1 */
#define AEL_POWERDOWN_REG   0xc011
#define AEL_TX_CONFIG_REG_1 0xc002
#define AEL_LOOPBACK_EN_REG 0xc017
#define AEL_MODE_SEL_REG    0xc001

#define PHY_XS_LANE_STATUS_REG 0x18

#define PMD_RESET          0
#define PMD_STATUS         1
#define PMD_IDENTIFIER     2
#define PCS_STATUS_REG     0x20

#define PMD_ID_QUAKE    0x43
#define PMD_ID_MYSTICOM 0x240
#define PCS_CONTROL 0

#define PEG_LOOP         1000   /* loop to pegstuck? */

static int nx_install_license(struct unm_adapter_s *adapter);
static int nx_get_capabilty_request(struct unm_adapter_s *adapter, 
		nx_license_capabilities_t *nx_lic_capabilities);
static int nx_install_license(struct unm_adapter_s *adapter);
static int nx_get_lic_finger_print(struct unm_adapter_s *adapter, 
		nx_finger_print_t *nx_lic_finger_print);
extern U32
issue_cmd(nx_dev_handle_t drv_handle,  U32 pci_fn, U32 version,
		U32 input_arg1, U32 input_arg2, U32 input_arg3, U32 cmd,
        U32 *output_arg1, U32 *output_arg2, U32 *output_arg3);
extern int 
nx_p3_set_vport_miss_mode(struct unm_adapter_s *adapter, int mode);
static int
unm_loopback_xmit_frame(struct sk_buff *skb, struct net_device *netdev);
static int
unm_nic_led_config(struct unm_adapter_s *adapter, nx_nic_led_config_t *param);
extern void nx_nic_halt_firmware(struct unm_adapter_s *adapter);
extern int nx_nic_minidump(struct unm_adapter_s *adapter);


unm_send_test_t tx_args;
/*
 * Test packets.
 */
unsigned char XMIT_DATA[] = { 0x50, 0xda, 0x2e, 0xfa, 0x77, 0x00, 0x02,
	0x2d, 0x8a, 0xa1, 0xde, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x32, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11,
	0xc2, 0x95, 0xc0, 0xa8, 0x7b, 0x64, 0xc0, 0xa8,
	0x7b, 0x70, 0x80, 0x09, 0x1c, 0x0d, 0x00, 0x1e,
	0xb1, 0x5a, 0x1f, 0x00, 0x00, 0x00, 0x58, 0x58,
	0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
	0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58
};


int
tx_test(struct net_device *netdev, unm_send_test_t * arg,
	unm_test_ctr_t * testCtx)
{
	unsigned int count = arg->count;
	struct sk_buff *skb;
	unsigned int len;
	unsigned char *pkt_data;
	unsigned int sent = 0;

	if (testCtx->tx_user_packet_data == NULL) {
		pkt_data = XMIT_DATA;
		len = sizeof(XMIT_DATA);
	} else {
		pkt_data = testCtx->tx_user_packet_data;
		len = testCtx->tx_user_packet_length;
	}

	while (testCtx->tx_stop == 0) {

		skb = alloc_skb(len, GFP_KERNEL);
		if (!skb) {
			return -ENOMEM;
		}
		*((uint16_t *) (pkt_data + 14)) = (uint16_t) sent;
		memcpy(skb->data, pkt_data, len);
		skb_put(skb, len);
		unm_nic_xmit_frame(skb, netdev);
		++sent;
		if (arg->count && --count == 0) {
			break;
		}

		schedule();
		if (arg->ifg) {
			if (arg->ifg < 1024) {
				udelay(arg->ifg);
			} else if (arg->ifg < 5000) {
				mdelay(arg->ifg >> 10);
			} else {
				mdelay(5);
			}
		}
		if (signal_pending(current)) {
			break;
		}
	}
	//printk("unm packet generator sent %d bytes %d packets %d bytes each\n",
	//                      sent*len, sent, len);
	return 0;
}

static int
unm_send_test(struct net_device *netdev, void *ptr, unm_test_ctr_t * testCtx)
{
	unm_send_test_t args;

	if (copy_from_user(&args, ptr, sizeof(args))) {
		return -EFAULT;
	}
	switch (args.cmd) {
	case UNM_TX_START:
		if (tx_args.cmd == UNM_TX_SET_PARAM) {
			if (args.ifg == 0) {
				args.ifg = tx_args.ifg;
			}
			if (args.count == 0) {
				args.count = tx_args.count;
			}
		}
		testCtx->tx_stop = 0;
		return tx_test(netdev, &args, testCtx);
		break;

	case UNM_TX_STOP:
		testCtx->tx_stop = 1;
		break;

	case UNM_TX_SET_PARAM:
		tx_args = args;
		break;

	case UNM_TX_SET_PACKET:
		if (testCtx->tx_user_packet_data) {
			kfree(testCtx->tx_user_packet_data);
		}
		testCtx->tx_user_packet_data = kmalloc(args.count, GFP_KERNEL);
		if (testCtx->tx_user_packet_data == NULL) {
			return -ENOMEM;
		}
		if (copy_from_user(testCtx->tx_user_packet_data,
				   (char *)(uptr_t) args.ifg, args.count)) {
			kfree(testCtx->tx_user_packet_data);
			testCtx->tx_user_packet_data = NULL;
			return -EFAULT;
		}
		testCtx->tx_user_packet_length = args.count;
		break;

	case UNM_LOOPBACK_START:
		testCtx->loopback_start = 1;
		netdev->hard_start_xmit = &unm_loopback_xmit_frame;
		break;

	case UNM_LOOPBACK_STOP:
		testCtx->loopback_start = 0;
		netdev->hard_start_xmit = &unm_nic_xmit_frame;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int unm_irq_test(unm_adapter *adapter) 
{
	nx_dev_handle_t drv_handle;
	uint64_t pre_int_cnt, post_int_cnt;
	nx_rcode_t rcode = NX_RCODE_SUCCESS;
	drv_handle = adapter->nx_dev->nx_drv_handle;

	pre_int_cnt = adapter->stats.ints;
	rcode = issue_cmd(drv_handle,
			adapter->ahw.pci_func,
			NXHAL_VERSION,
			//adapter->nx_dev->rx_ctxs[0]->this_id,
			adapter->portnum,
			0,
			0,
			NX_CDRP_CMD_GEN_INT,
			NULL,
			NULL,
			NULL);
	
	if (rcode != NX_RCODE_SUCCESS) {
		return -1;
	}
	mdelay(50);
	post_int_cnt = adapter->stats.ints;
	
	return ((pre_int_cnt == post_int_cnt) ? (-1) : (0));
}

static int unm_set_fw_loopback(struct unm_adapter_s *adapter, int flag) 
{
	int			rv;
	nic_request_t		req;
	nx_nic_loopback_t	*pinfo;
	
	req.opcode = NX_NIC_HOST_REQUEST;
	pinfo = (nx_nic_loopback_t  *) &req.body;
	memset(pinfo, 0, sizeof (nx_nic_loopback_t));	
	
	pinfo->hdr.opcode = NX_NIC_H2C_OPCODE_CONFIG_LOOPBACK;
	pinfo->hdr.comp_id = 1;
	pinfo->hdr.ctxid = adapter->portnum;
	pinfo->hdr.need_completion = 0;
	
	if (flag) {
		pinfo->loopback_flag = 1;
	} else	{
		pinfo->loopback_flag = 0;
	}
	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);
	if (rv) {
		nx_nic_print4(adapter, "Changing fw Loopback mode failed \n");
	}
	return rv;

}

int
unm_loopback_test(struct net_device *netdev, int fint, void *ptr,
		  unm_test_ctr_t * testCtx)
{
	int ii, ret;
	unsigned char *data;
	unm_send_test_t args;
	struct unm_adapter_s *adapter;
	__uint32_t mode = VPORT_MISS_MODE_DROP;
	
	
	adapter = (struct unm_adapter_s *)netdev_priv(netdev);

	if(!fint)
		return (-LB_NOT_SUPPORTED);
	ret = 0;
	if (ptr) {
		if ((ret = copy_from_user(&args, ptr, sizeof(args)))) {
			return (-LB_UCOPY_PARAM_ERR);
		}
	} else
		memset(&args, 0, sizeof(args));

	if (testCtx->tx_user_packet_data != NULL) {
		kfree(testCtx->tx_user_packet_data);
		testCtx->tx_user_packet_data = NULL;
	}

	if (args.count != 0) {	/* user data */
		testCtx->tx_user_packet_data =
		    kmalloc(args.count + 16, GFP_KERNEL);
		if (testCtx->tx_user_packet_data == NULL)
			return (-LB_NOMEM_ERR);
		memset(testCtx->tx_user_packet_data, 0xFF, 16);
		if (ptr)
			if (copy_from_user(testCtx->tx_user_packet_data + 16,
					   (char *)(uptr_t) args.ifg,
					   args.count)) {
				kfree(testCtx->tx_user_packet_data);
				testCtx->tx_user_packet_data = NULL;
				return (-LB_UCOPY_DATA_ERR);
			}
		testCtx->tx_user_packet_length = args.count + 16;
		testCtx->rx_datalen =
		    (testCtx->tx_user_packet_length - 14) * 16;
	} else {		/* use local data */
		testCtx->tx_user_packet_data =
		    kmalloc(sizeof(XMIT_DATA) + 16, GFP_KERNEL);
		if (testCtx->tx_user_packet_data == NULL)
			return (-LB_NOMEM_ERR);
		memset(testCtx->tx_user_packet_data, 0xFF, 14);
		memcpy(testCtx->tx_user_packet_data + 16, XMIT_DATA,
		       sizeof(XMIT_DATA));
		testCtx->tx_user_packet_length = sizeof(XMIT_DATA) + 16;
		testCtx->rx_datalen = (sizeof(XMIT_DATA) + 2) * 16;
	}
	if ((testCtx->rx_user_packet_data =
	     kmalloc(testCtx->rx_datalen, GFP_KERNEL)) == NULL) {
		ret = -LB_NOMEM_ERR;
		goto end;
	}
	testCtx->rx_user_pos = 0;
	if (netif_running(netdev)) {
		netif_stop_queue(netdev);
	}

	if (fint == 1) {
		unm_set_fw_loopback(adapter, 1);
		testCtx->loopback_start = 1;
		if (netdev->flags & IFF_PROMISC)
			mode = VPORT_MISS_MODE_ACCEPT_ALL;
		if (netdev->flags & IFF_ALLMULTI)
			mode |= VPORT_MISS_MODE_ACCEPT_MULTI;
		nx_p3_set_vport_miss_mode(adapter, VPORT_MISS_MODE_ACCEPT_ALL);
	}
	mdelay(1000);
	testCtx->tx_stop = 0;
	testCtx->capture_input = 1;
	args.count = 16;
	if ((ret = tx_test(netdev, &args, testCtx)) != 0) {
		ret = -LB_TX_NOSKB_ERR;
		testCtx->capture_input = 0;
		goto end;
	}

	mdelay(1000);		/* wait for data to come back */
	testCtx->capture_input = 0;

	if (testCtx->rx_user_pos != testCtx->rx_datalen) {
		ret = -LB_SHORT_DATA_ERR;
		goto end;
	}

	data = testCtx->rx_user_packet_data;
	/* check received bytes against tx_user_packet_data */
	for (ii = 0; ii < 16; ++ii) {
		if (*((uint16_t *) data) != (uint16_t) ii) {
			ret = -LB_SEQUENCE_ERR;
			goto end;
		}
		if (memcmp(testCtx->tx_user_packet_data + 16, data + 2,
			   testCtx->tx_user_packet_length - 16)
		    != 0) {
			ret = -LB_DATA_ERR;
			goto end;
		}
		data += (testCtx->tx_user_packet_length - 14);
	}
	ret = LB_TEST_OK;

      end:
	testCtx->tx_stop = 1;
	if (fint == 1) {
		unm_set_fw_loopback(adapter, 0);
		nx_p3_set_vport_miss_mode(adapter, mode);
		testCtx->loopback_start = 0;
	}
	if (netif_running(netdev)) {
		netif_wake_queue(netdev);
	}

	if (testCtx->tx_user_packet_data != NULL) {
		kfree(testCtx->tx_user_packet_data);
		testCtx->tx_user_packet_data = NULL;
	}
	kfree(testCtx->rx_user_packet_data);
	testCtx->rx_user_packet_data = NULL;
	return ret;
}


int unm_link_test(unm_adapter * adapter)
{
	int rv;
	rv = adapter->ahw.linkup;
	return ((rv == 0) ? (-1) : (0));
}

int unm_led_blink_rate_set(unm_adapter *adapter, u32 testval)
{
	nx_nic_led_config_t param;
	int ret = 0;

	testval &= 0xff;
	if (testval <= 1) {
		param.ctx_id = adapter->portnum;
		param.blink_state = 0xf;
		param.blink_rate = testval;
		adapter->led_blink_rate = param.blink_rate;
		ret = unm_nic_led_config(adapter, &param);
	}
	return ret;
}

int unm_led_blink_state_set(unm_adapter *adapter, u32 testval)
{
	nx_nic_led_config_t param;
	int ret = 0;

	testval &= 0xff;
	if (testval <= 1) {
		param.ctx_id = adapter->portnum;
		param.blink_state = testval;
		param.blink_rate = 0xf;
		adapter->led_blink_state = param.blink_state;
		ret = unm_nic_led_config(adapter, &param);
	}
	return ret;
}


int unm_led_test(unm_adapter *adapter)
{
	adapter->ahw.LEDTestRet = LED_TEST_OK;
	return (-LED_TEST_NOT_SUPPORTED);
}

static int unmtest_pegstuck(unm_crbword_t addr, U64 reg, int loop,
		     struct unm_adapter_s *adapter)
{
	int i;
	unm_crbword_t temp;

	for (i = 0; i < loop; ++i) {
		temp = NXRD32(adapter,reg);
		if (temp != addr)
			return (0);
	}
	return (-1);
}

#define NIC_NUMPEGS                3

static int unm_hw_test(unm_adapter *adapter)
{
	unm_crbword_t temp;
	int ii, rc = HW_TEST_OK;
	uint64_t base, address;

	/* DMA Status */
	temp = NXRD32(adapter,UNM_DMA_COMMAND(0));
	if ((temp & 0x3) == 0x3) {
		rc = -HW_DMA_BZ_0;
		goto done;
	}
	temp = NXRD32(adapter,UNM_DMA_COMMAND(1));
	if ((temp & 0x3) == 0x3) {
		rc = -HW_DMA_BZ_1;
		goto done;
	}
	temp = NXRD32(adapter,UNM_DMA_COMMAND(2));
	if ((temp & 0x3) == 0x3) {
		rc = -HW_DMA_BZ_2;
		goto done;
	}
	temp = NXRD32(adapter,UNM_DMA_COMMAND(3));
	if ((temp & 0x3) == 0x3) {
		rc = -HW_DMA_BZ_3;
		goto done;
	}

	/* SRE Status */
	temp = NXRD32(adapter,UNM_SRE_PBI_ACTIVE_STATUS);
	if ((temp & 0x1) == 0) {
		rc = -HW_SRE_PBI_HALT;
		goto done;
	}
	temp = NXRD32(adapter,UNM_SRE_L1RE_CTL);
	if ((temp & 0x20000000) != 0) {
		rc = -HW_SRE_L1IPQ;
		goto done;
	}
	temp = NXRD32(adapter,UNM_SRE_L2RE_CTL);
	if ((temp & 0x20000000) != 0) {
		rc = -HW_SRE_L2IFQ;
		goto done;
	}

	temp = NXRD32(adapter,UNM_SRE_INT_STATUS);
	if ((temp & 0xc0ff) != 0) {
		if ((temp & 0x1) != 0) {
			rc = -HW_PQ_W_PAUSE;
			goto done;
		}
		if ((temp & 0x2) != 0) {
			rc = -HW_PQ_W_FULL;
			goto done;
		}
		if ((temp & 0x4) != 0) {
			rc = -HW_IFQ_W_PAUSE;
			goto done;
		}
		if ((temp & 0x8) != 0) {
			rc = -HW_IFQ_W_FULL;
			goto done;
		}
		if ((temp & 0x10) != 0) {
			rc = -HW_MEN_BP_TOUT;
			goto done;
		}
		if ((temp & 0x20) != 0) {
			rc = -HW_DOWN_BP_TOUT;
			goto done;
		}
		if ((temp & 0x40) != 0) {
			rc = -HW_FBUFF_POOL_WM;
			goto done;
		}
		if ((temp & 0x80) != 0) {
			rc = -HW_PBUF_ERR;
			goto done;
		}
		if ((temp & 0x4000) != 0) {
			rc = -HW_FM_MSG_HDR;
			goto done;
		}
		if ((temp & 0x8000) != 0) {
			rc = -HW_FM_MSG;
			goto done;
		}
	}

	temp = NXRD32(adapter,UNM_SRE_INT_STATUS);

	if ((temp & 0x3f00) != 0) {
		if ((temp & 0x100) != 0) {
			rc = -HW_EPG_MSG_BUF;
			goto done;
		}
		if ((temp & 0x200) != 0) {
			rc = -HW_EPG_QREAD_TOUT;
			goto done;
		}
		if ((temp & 0x400) != 0) {
			rc = -HW_EPG_QWRITE_TOUT;
			goto done;
		}
		if ((temp & 0x800) != 0) {
			rc = -HW_EPG_CQ_W_FULL;
			goto done;
		}
		if ((temp & 0x1000) != 0) {
			rc = -HW_EPG_MSG_CHKSM;
			goto done;
		}
		if ((temp & 0x2000) != 0) {
			rc = -HW_EPG_MTLQ_TOUT;
			goto done;
		}
	}

	/* Pegs */
	for (ii = 0; ii < NIC_NUMPEGS; ++ii) {
		base = PEG_NETWORK_BASE(ii);
		address = base | CRB_REG_EX_PC;
		temp = NXRD32(adapter,address);
		rc = unmtest_pegstuck(temp, address, PEG_LOOP, adapter);
		if (rc != 0) {
			rc = -(HW_PEG0 + ii);
			goto done;
		}
	}

      done:
	return (rc);
}

static int unm_cis_test(unm_adapter *adapter)
{
	return (CIS_TEST_OK);
}

static int unm_cr_test(unm_adapter *adapter)
{

	unm_crbword_t temp;
	int ret = CR_TEST_OK;

	temp = NXRD32(adapter,UNM_CRB_PCIE);

	/* at least one bit of bits 0-2 must be set */
	if ((temp & 0xFFFF) != PCI_VENDOR_ID_NX) {
		// Vendor ID is itself wrong. Report definite error
		ret = -CR_ERROR;
	}
	return (ret);
}
static int unm_nic_led_config(struct unm_adapter_s *adapter, nx_nic_led_config_t *param)
{
	int rv;
	nic_request_t   req;
	nx_nic_led_config_t *pinfo;

	req.opcode = NX_NIC_HOST_REQUEST;
	pinfo = (nx_nic_led_config_t  *) &req.body;
	memcpy(pinfo, param, sizeof (nx_nic_led_config_t));

	pinfo->hdr.opcode = NX_NIC_H2C_OPCODE_CONFIG_LED;
	pinfo->hdr.comp_id = 1;
	pinfo->hdr.ctxid = adapter->portnum;
	pinfo->hdr.need_completion = 0;

	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);
	if (rv) {
		nx_nic_print4(adapter, "Changing LED configuration failed \n");
	}
	return rv;
}

static int unm_nic_do_ioctl(struct unm_adapter_s *adapter, void *u_data)
{
	unm_nic_ioctl_data_t data;
	unm_nic_ioctl_data_t *up_data = (unm_nic_ioctl_data_t *)u_data;
	int retval = 0;
	int ori_auto_fw_reset;
	long phy_id = 0 ;
	uint64_t efuse_chip_id = 0;
	char *ptr = NULL;
	nx_finger_print_t nx_lic_finger_print;
	nx_license_capabilities_t nx_lic_capabilities;
	nx_finger_print_ioctl_t *snt_ptr;
	nx_license_capabilities_ioctl_t *snt_ptr1;
	nx_install_license_ioctl_t *snt_ptr2;
	u8 *capture_buff;
	u32 dump_size;

	nx_nic_print7(adapter, "doing ioctl for %p\n", adapter);
	if (copy_from_user(&data, up_data, sizeof(data))) {
		/* evil user tried to crash the kernel */
		nx_nic_print6(adapter, "bad copy from userland: %d\n",
				(int)sizeof(data));
		retval = -EFAULT;
		goto error_out;
	}

	/* Shouldn't access beyond legal limits of  "char u[64];" member */
	if (!data.ptr && (data.size > sizeof(data.u))) {
		/* evil user tried to crash the kernel */
		nx_nic_print6(adapter, "bad size: %d\n", data.size);
		retval = -EFAULT;
		goto error_out;
	}

	switch (data.cmd) {
		case UNM_NIC_CMD_PCI_READ:
			if ((retval = adapter->unm_nic_hw_read_ioctl(adapter, data.off,
							&(data.u), data.size)))
				goto error_out;
			if (copy_to_user((void *)&(up_data->u), &(data.u), data.size)) {
				nx_nic_print6(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));
				retval = -EFAULT;
				goto error_out;
			}
			data.rv = 0;
			break;

		case UNM_NIC_CMD_PCI_WRITE:
			{
				int check = 1;
				int *val= (int *)&data.u;
				switch(data.off) {
					case UNM_CRB_PEG_NET_0+0x3c:
					case UNM_CRB_PEG_NET_1+0x3c:
					case UNM_CRB_PEG_NET_2+0x3c:
					case UNM_CRB_PEG_NET_3+0x3c:
					case UNM_CRB_PEG_NET_4+0x3c:
						if(*val == 1 && adapter->auto_fw_reset) {
							data.rv = 0;
							check = 0;
						}
						break;
				}
				if(check ) {
					data.rv = adapter->
						unm_nic_hw_write_ioctl(adapter, data.off, &(data.u),
							data.size);
				}
			}
			break;

		case UNM_NIC_CMD_PCI_MEM_READ:
			nx_nic_print7(adapter, "doing unm_nic_cmd_pci_mm_rd\n");
			if ((adapter->unm_nic_pci_mem_read(adapter, data.off, &(data.u),
							data.size) != 0) ||
					(copy_to_user((void *)&(up_data->u), &(data.u),
						      data.size) != 0)) {
				nx_nic_print6(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));
				retval = -EFAULT;
				goto error_out;
			}
			data.rv = 0;
			nx_nic_print7(adapter, "read %lx\n", (unsigned long)data.u);
			break;

		case UNM_NIC_CMD_PCI_MEM_WRITE:
			if ((data.rv =
						adapter->unm_nic_pci_mem_write(adapter, data.off, &(data.u),
							data.size)) != 0) {
				retval = -EFAULT;
				goto error_out;
			}
			break;

		case UNM_NIC_CMD_PCI_CONFIG_READ:
			switch (data.size) {
				case 1:
					data.rv = pci_read_config_byte(adapter->ahw.pdev,
							data.off,
							(char *)&(data.u));
					break;
				case 2:
					data.rv = pci_read_config_word(adapter->ahw.pdev,
							data.off,
							(short *)&(data.u));
					break;
				case 4:
					data.rv = pci_read_config_dword(adapter->ahw.pdev,
							data.off,
							(u32 *) & (data.u));
					break;
			}
			if (copy_to_user((void *)&up_data->u, &data.u, data.size)) {
				nx_nic_print6(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));
				retval = -EFAULT;
				goto error_out;
			}
			break;

		case UNM_NIC_CMD_PCI_CONFIG_WRITE:
			switch (data.size) {
				case 1:
					data.rv = pci_write_config_byte(adapter->ahw.pdev,
							data.off,
							*(char *)&(data.u));
					break;
				case 2:
					data.rv = pci_write_config_word(adapter->ahw.pdev,
							data.off,
							*(short *)&(data.u));
					break;
				case 4:
					data.rv = pci_write_config_dword(adapter->ahw.pdev,
							data.off,
							*(u32 *) & (data.u));
					break;
			}
			break;

		case UNM_NIC_CMD_GET_VERSION:
			if (copy_to_user((void *)&(up_data->u), UNM_NIC_LINUX_VERSIONID,
						sizeof(UNM_NIC_LINUX_VERSIONID))) {
				nx_nic_print6(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));
				retval = -EFAULT;
				goto error_out;
			}
			break;

		case UNM_NIC_CMD_SET_LED_CONFIG:
			{
				u32 testval = 0, rate = 1;
				led_params_t *snt_ptr3=NULL;
				retval = 0;
				memcpy(&snt_ptr3, &(data.ptr), sizeof(u32 *));
				if (copy_from_user((void *)&testval, (void*)&(snt_ptr3->state), sizeof(u32))) {
					nx_nic_print4(adapter, "bad copy from userland:\n ");
					retval = -EFAULT;
					goto error_out;
				}
				if (copy_from_user((void*)&rate, (void*)&(snt_ptr3->rate), sizeof(u32))) {
					nx_nic_print4(adapter, "bad copy from userland:\n ");
					retval = -EFAULT;
					goto error_out;
				}
				if(unm_led_blink_state_set(adapter, testval) &&
						unm_led_blink_rate_set(adapter, rate)) {
					nx_nic_print4(adapter, "unm_led_blink_state_set or unm_led_blink_rate_set failed\n");
					retval = LED_TEST_ERR;
					goto error_out;
				}
			}
			break;

		case UNM_NIC_CMD_GET_PHY_TYPE:
			phy_id = unm_xge_mdio_rd_port(adapter, 0, DEV_PMA_PMD,
					PMD_IDENTIFIER);
			if (adapter->portnum == 1 &&  phy_id == PMD_ID_MYSTICOM) {
				phy_id = unm_xge_mdio_rd_port(adapter, 3,
						DEV_PMA_PMD, PMD_IDENTIFIER);
			}
			if(copy_to_user((void *)&up_data->u, &phy_id, sizeof(phy_id))) {
				nx_nic_print4(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));
				retval = -EFAULT;
				goto error_out;
			}
			data.rv = 0;
			break;
		case UNM_NIC_CMD_EFUSE_CHIP_ID:

			efuse_chip_id = 
				NXRD32(adapter,
						UNM_EFUSE_CHIP_ID_HIGH);
			efuse_chip_id <<= 32;
			efuse_chip_id |= 
				NXRD32(adapter,
						UNM_EFUSE_CHIP_ID_LOW);
			if(copy_to_user((void *) &up_data->u, &efuse_chip_id,
						sizeof(uint64_t))) {
				nx_nic_print4(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));
				retval = -EFAULT;
				goto error_out;
			}
			data.rv = 0;
			break;
		case UNM_NIC_CMD_GET_LIC_FINGERPRINT:
			data.rv = nx_get_lic_finger_print(adapter, &nx_lic_finger_print);
			memcpy(&snt_ptr, &(data.ptr), sizeof(u32 *));


			if(copy_to_user(&snt_ptr->req_len, &(nx_lic_finger_print.len),
						sizeof(nx_lic_finger_print.len))) {
				nx_nic_print4(adapter, "bad copy to userland:\n ");
				retval = -EFAULT;
				goto error_out;
			}

			if(copy_to_user((void *) snt_ptr->req_finger_print, &(nx_lic_finger_print.data),
						sizeof(nx_lic_finger_print.data))) {
				nx_nic_print4(adapter, "bad copy to userland:\n ");
				retval = -EFAULT;
				goto error_out;
			}

			if(copy_to_user((void *) &up_data->u, &(nx_lic_finger_print.len),
						sizeof(nx_lic_finger_print.len))) {
				nx_nic_print4(adapter, "bad copy to userland:\n");
				retval = -EFAULT;
				goto error_out;
			}

			break;
		case UNM_NIC_CMD_LIC_INSTALL:
			memset(adapter->nx_lic_dma.addr, 0, sizeof(nx_install_license_t));
                        memcpy(&snt_ptr2, &(data.ptr), sizeof(u32 *));

			if (copy_from_user((adapter->nx_lic_dma.addr), (void*)snt_ptr2->data, snt_ptr2->data_len)) {
				nx_nic_print4(adapter, "bad copy from userland:\n ");
				retval = -EFAULT;
				goto error_out;
			}
			data.rv = nx_install_license(adapter);
			break;
		case UNM_NIC_CMD_GET_LIC_FEATURES:
			data.rv = nx_get_capabilty_request(adapter, &nx_lic_capabilities);
                        memcpy(&snt_ptr1, &(data.ptr), sizeof(u32 *));

			if(copy_to_user((void *) &(snt_ptr1->req_len), &(nx_lic_capabilities.len),
						sizeof(nx_lic_capabilities.len))) {
				nx_nic_print4(adapter, "bad copy to userland:\n ");
				retval = -EFAULT;
				goto error_out;
			}

			if(copy_to_user((void *) snt_ptr1->req_license_capabilities, &(nx_lic_capabilities.arr),
						sizeof(nx_lic_capabilities.arr))) {
				nx_nic_print4(adapter, "bad copy to userland:\n ");
				retval = -EFAULT;
				goto error_out;
			}
			if(copy_to_user((void *) &up_data->u, &(nx_lic_capabilities.len),
						sizeof(nx_lic_capabilities.len))) {
				nx_nic_print4(adapter, "bad copy to userland:\n");
				retval = -EFAULT;
				goto error_out;
			}
			break;

#if 0				// wait for the unmflash changes
		case UNM_NIC_CMD_FLASH_READ:
			/* do_rom_fast_read() called instead of rom_fast_read() because
			   rom_lock/rom_unlock is done by separate ioctl */
			if ((retval = do_rom_fast_read(adapter, data.off,
							(int *)&(data.u))))
				goto error_out;
			if (copy_to_user((void *)&(up_data->u), &(data.u),
						UNM_FLASH_READ_SIZE)) {
				nx_nic_print6(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));
				retval = -EFAULT;
				goto error_out;
			}
			data.rv = 0;
			break;

		case UNM_NIC_CMD_FLASH_WRITE:
			/*
			 * do_rom_fast_write() called instead of rom_fast_write()
			 * because rom_lock/rom_unlock is done by separate ioctl
			 */
			data.rv = do_rom_fast_write(adapter, data.off,
					*(u32 *)&data.u);
			break;

		case UNM_NIC_CMD_FLASH_SE:
			data.rv = rom_se(adapter, data.off);
			break;
#endif
		case UNM_NIC_CMD_TRCBUF_READ:
			
			if(data.off > NX_TRACE_ARR_SIZE) {
				retval = -EINVAL;
				goto error_out;
			}
			if (copy_to_user((void *)&(up_data->u),
						&(adapter->trc_buf[data.off]), data.size)) {
				nx_nic_print6(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));

				retval = -EFAULT;
				goto error_out;
			}
			data.rv = 0;
			break;

		case UNM_NIC_CMD_MINIDUMP_SIZE:
		    if (adapter->mdump.has_valid_dump) {
				dump_size = adapter->mdump.md_dump_size;
		    } else {
				dump_size = 0;
		    }

		    if (copy_to_user((void *)&(up_data->size), &dump_size, sizeof(up_data->size)))
				return -EFAULT;

		    data.rv = 0;
		    break;

		case UNM_NIC_CMD_MINIDUMP_READ:
		    if (!adapter->mdump.has_valid_dump) {
				return -EINVAL;
		    }

		    if (data.size > NX_NIC_MINIDUMP_IOCTL_COPY_SIZE ||
			    (data.off + data.size) > adapter->mdump.md_dump_size || !data.ptr) {
				return -EINVAL;
		    }

		    capture_buff = adapter->mdump.md_capture_buff;

		    if (copy_to_user((void *)(data.ptr), capture_buff + data.off, data.size))
				return -EFAULT;

		    data.rv = 0;
		    break;

		case UNM_NIC_CMD_FORCE_MINIDUMP:
		    if (!adapter->mdump.fw_supports_md || adapter->portnum != 0) {
				return -EINVAL;
		    }
			ori_auto_fw_reset = adapter->auto_fw_reset;
			adapter->auto_fw_reset = 0;
			nx_nic_halt_firmware(adapter);
			data.rv = nx_nic_minidump(adapter);
			if (adapter->mdump.has_valid_dump == 1) {
				adapter->mdump.disable_overwrite = 1;
			}
			adapter->auto_fw_reset = ori_auto_fw_reset;
		    break;

		case UNM_NIC_CMD_FWLOG_GET_SIZE:

			switch (data.off) {
				case 0:
					ptr = adapter->fw_dmp.pcon[0];
					break;
				case 1:
					ptr = adapter->fw_dmp.pcon[1];
					break;
				case 2:
					ptr = adapter->fw_dmp.pcon[2];
					break;
				case 3:
					ptr = adapter->fw_dmp.pcon[3];
					break;
				case 4:
					ptr = adapter->fw_dmp.pcon[4];
					break;
				case 5:
					ptr = adapter->fw_dmp.phanstat;
					break;
				case 6:
					ptr = adapter->fw_dmp.srestat;
					break;
				case 7:
					ptr = adapter->fw_dmp.epgstat;
					break;
				case 8:
					ptr = adapter->fw_dmp.nicregs;
					break;
				case 9:
					ptr = adapter->fw_dmp.macstat[0];
					break;
				case 10:
					ptr = adapter->fw_dmp.macstat[1];
					break;
				default:
					retval = -EOPNOTSUPP;
					goto error_out;
			}

			if(ptr == NULL) {
				dump_size = 0;
			} else {
				dump_size = strlen(ptr);
			}

			if (copy_to_user((void *)&(up_data->size), &dump_size, sizeof(up_data->size)))
				return -EFAULT;

			data.rv = 0;

			break;

		case UNM_NIC_CMD_FWLOG_GET:

			switch (data.off) {
				case 0:
					ptr = adapter->fw_dmp.pcon[0];
					break;
				case 1:
					ptr = adapter->fw_dmp.pcon[1];
					break;
				case 2:
					ptr = adapter->fw_dmp.pcon[2];
					break;
				case 3:
					ptr = adapter->fw_dmp.pcon[3];
					break;
				case 4:
					ptr = adapter->fw_dmp.pcon[4];
					break;
				case 5:
					ptr = adapter->fw_dmp.phanstat;
					break;
				case 6:
					ptr = adapter->fw_dmp.srestat;
					break;
				case 7:
					ptr = adapter->fw_dmp.epgstat;
					break;
				case 8:
					ptr = adapter->fw_dmp.nicregs;
					break;
				case 9:
					ptr = adapter->fw_dmp.macstat[0];
					break;
				case 10:
					ptr = adapter->fw_dmp.macstat[1];
					break;
				default:
					retval = -EOPNOTSUPP;
					goto error_out;
			}

			if(ptr == NULL) {
				retval = -ENODATA;
				goto error_out;
			}

			if(data.size < strlen(ptr)) {
				retval = -EINVAL;
				goto error_out;
			}

			if (copy_to_user((void *)data.ptr, ptr, strlen(ptr))) {
				nx_nic_print6(adapter, "bad copy to userland: %d\n",
						(int)sizeof(data));

				retval = -EFAULT;
				goto error_out;
			}
			data.rv = 0;
			break;

		default:
			nx_nic_print4(adapter, "bad command %d\n", data.cmd);
			retval = -EOPNOTSUPP;
			goto error_out;
	}
	put_user(data.rv, &(up_data->rv));
	nx_nic_print7(adapter, "done ioctl.\n");

error_out:
	return retval;

}
/*
 * unm_nic_ioctl ()    We provide the tcl/phanmon support through these
 * ioctls.
 */
int unm_nic_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	int err = 0;
	unsigned long nr_bytes = 0;
	struct unm_adapter_s *adapter = (struct unm_adapter_s*) netdev_priv(netdev);
	char dev_name[UNM_NIC_NAME_LEN];
	int count;

        nx_nic_print7(adapter, "doing ioctl\n");

	if ((cmd != UNM_NIC_NAME) && !capable(CAP_NET_ADMIN))
		return -EPERM;

        switch (cmd) {
        case UNM_NIC_CMD:
                err = unm_nic_do_ioctl(adapter, (void *) ifr->ifr_data);
                break;

        case UNM_NIC_NAME:
                nx_nic_print7(adapter, "ioctl cmd for UNM\n");
                if (ifr->ifr_data) {
                        sprintf(dev_name, "%s-%d", UNM_NIC_NAME_RSP,
                                adapter->portnum);

			count = -1;
			if (adapter->interface_id != -1)
				count = adapter->interface_id & PORT_MASK;

			if (count != -1)
				sprintf(dev_name + strlen(dev_name) + 1, "%d",
				    count);

                        nr_bytes = copy_to_user((char *)ifr->ifr_data,
						dev_name, UNM_NIC_NAME_LEN);
                        if (nr_bytes)
				err = -EIO;
                }
                break;

        case UNM_NIC_SEND_TEST:
                err = unm_send_test(netdev, (void *)ifr->ifr_data,
				    &adapter->testCtx);
                break;

        case UNM_NIC_IRQ_TEST:
                err = unm_irq_test(adapter);
                break;

        case UNM_NIC_ILB_TEST:
                if (adapter->is_up == ADAPTER_UP_MAGIC) {
			if (!(ifr->ifr_data))
				err = -LB_UCOPY_PARAM_ERR;
			else 
				err = unm_loopback_test(netdev, 1,
							(void *)ifr->ifr_data,
							&adapter->testCtx);
		} else {
			nx_nic_print5(adapter, "Adapter resources not "
				      "initialized\n");
			err = -ENOSYS;
                }
                break;

	case UNM_NIC_ELB_TEST:
		if (adapter->is_up == ADAPTER_UP_MAGIC) {
			if (!(ifr->ifr_data))
				err = -LB_UCOPY_PARAM_ERR;
			else
				err = unm_loopback_test(netdev, 0,
							(void *)ifr->ifr_data,
							&adapter->testCtx);
		} else {
			nx_nic_print3(adapter,
				      "Adapter resources not initialized\n");
			err = -ENOSYS;
		}
		break;

	case UNM_NIC_LINK_TEST:
		err = unm_link_test(adapter);
		break;

	case UNM_NIC_HW_TEST:
		err = unm_hw_test(adapter);
		break;

	case UNM_NIC_CIS_TEST:
		err = unm_cis_test(adapter);
		break;

	case UNM_NIC_CR_TEST:
		err = unm_cr_test(adapter);
		break;

	case UNM_NIC_LED_TEST:
		(adapter->ahw).LEDTestLast = 0;
		for (count = 0; count < 8; count++) {
			//Need to restore LED on last test
			if (count == 7)
				(adapter->ahw).LEDTestLast = 1;
			err = unm_led_test(adapter);
			mdelay(100);
		}
		break;

	default:
                nx_nic_print7(adapter, "ioctl cmd %x not supported\n", cmd);
		err = -EOPNOTSUPP;
		break;
	}

	return err;
}
int unm_read_blink_state(char *buf, char **start, off_t offset, int count,
		                       int *eof, void *data) {
	struct net_device *netdev = (struct net_device *)data;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	int len = 0;
	len = sprintf(buf,"%d\n",adapter->led_blink_state);
	*eof = 1;
	return len;
}
int unm_write_blink_state(struct file *file, const char *buffer,
		unsigned long count, void *data) {
	nx_nic_led_config_t param;
	struct net_device *netdev = (struct net_device *)data;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	unsigned int testval;
	int ret = 0;

	if (!capable(CAP_NET_ADMIN)) {
		return -EACCES;
	}
	if (adapter->state != PORT_UP) {
		return -EINVAL;
	}
	if (copy_from_user(&testval, buffer, 1)){

		return -EFAULT;
	}
	testval &= 0xff;
	testval -= 48;
	if (testval <= 1) {
		param.ctx_id = adapter->portnum;
		param.blink_state = testval;
		param.blink_rate = 0xf;
		adapter->led_blink_state = param.blink_state;
		ret = unm_nic_led_config(adapter, &param);
	}
	if (ret) {
		return ret;
	} else {
		return 1;
	}
}
int unm_read_blink_rate(char *buf, char **start, off_t offset, int count,
				int *eof, void *data) {

	int len = 0;
	struct net_device *netdev = (struct net_device *)data;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	len = sprintf(buf,"%d\n", adapter->led_blink_rate);
	*eof = 1;
	return len ;
}
#define MAX_SUPPORTED_BLINK_RATES               3
int unm_write_blink_rate(struct file *file, const char *buffer,
		unsigned long count, void *data) {
	nx_nic_led_config_t param;
	struct net_device *netdev = (struct net_device *)data;
	struct unm_adapter_s *adapter = (struct unm_adapter_s *)netdev_priv(netdev);
	unsigned int testval;
	int ret = 0;
	if (!capable(CAP_NET_ADMIN)) {
		return -EACCES;
	}
	if (adapter->state != PORT_UP) {
		return -EINVAL;
	}
	if(copy_from_user(&testval, buffer, 1)) {
		return -EFAULT;
	}
	testval &= 0xff;
	testval -= 48;
	if(testval <= MAX_SUPPORTED_BLINK_RATES) {
		param.ctx_id = adapter->portnum;
		param.blink_rate = testval;
		param.blink_state = 0xf;
		adapter->led_blink_rate = param.blink_rate;
		ret = unm_nic_led_config(adapter, &param);
	}
	if (ret) {
		return ret;
	} else {
		return 1;
	}
}
static int
unm_loopback_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	unsigned long flags;
	int rv;

	local_irq_save(flags);
	rv = unm_nic_xmit_frame(skb, netdev);
	local_irq_restore(flags);
	return rv;
}

#if defined(XGB_DEBUG)
static void dump_skb(struct sk_buff *skb)
{
	printk("%s: SKB at %p\n", unm_nic_driver_name, skb);
	printk("    ->next: %p\n", skb->next);
	printk("    ->prev: %p\n", skb->prev);
	printk("    ->list: %p\n", skb->list);
	printk("    ->sk: %p\n", skb->sk);
	printk("    ->stamp: %lx\n", skb->stamp);
	printk("    ->dev: %p\n", skb->dev);
	//printk("    ->input_dev: %p\n", skb->input_dev);
	printk("    ->real_dev: %p\n", skb->real_dev);
	printk("    ->h.raw: %p\n", skb->h.raw);

	printk("    ->len: %lx\n", skb->len);
	printk("    ->data_len:%lx\n", skb->data_len);
	printk("    ->mac_len:%lx\n", skb->mac_len);
	printk("    ->csum:%lx\n", skb->csum);

	printk("    ->head:%p\n", skb->head);
	printk("    ->data:%p\n", skb->data);
	printk("    ->tail:%p\n", skb->tail);
	printk("    ->end:%p\n", skb->end);

	return;
}

static int skb_is_sane(struct sk_buff *skb)
{
	int ret = 1;

	if (skb_is_nonlinear(skb)) {
		nx_nic_print3(adapter, "Got a non-linear SKB @%p data_len:"
			      "%d back\n", skb, skb->data_len);
		return 0;
	}

#if 0
	if (skb->list) {
		return 0;
	}
#endif
	if (skb->dev != NULL || skb->next != NULL || skb->prev != NULL)
		return 0;
	if (skb->data == NULL || skb->data > skb->tail)
		return 0;
	if (*(unsigned long *)skb->head != 0xc0debabe) {
                nx_nic_print5(adapter, "signature not found\n");
		return 0;
	}

	return ret;
}
#endif

int nx_get_lic_finger_print(struct unm_adapter_s *adapter, nx_finger_print_t *nx_lic_finger_print) {

	nic_request_t   req;
	nx_get_finger_print_request_t *lic_req;
	int          rv = 0;
	struct timeval tv;
	nx_os_wait_event_t swait;

	memset(&req, 0, sizeof(req));
	memset(adapter->nx_lic_dma.addr, 0, sizeof(nx_finger_print_t));
	req.opcode = NX_NIC_HOST_REQUEST;
	req.qmsg_type = UNM_MSGTYPE_NIC_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_GET_FINGER_PRINT_REQUEST;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;
	lic_req = (nx_get_finger_print_request_t *)&req.body;
	lic_req->ring_ctx = adapter->portnum;

	lic_req->dma_to_addr = (__uint64_t)(adapter->nx_lic_dma.phys_addr);
	lic_req->dma_size = (__uint32_t)(sizeof(nx_finger_print_t));
	do_gettimeofday(&tv); // Get Latest Time
	lic_req->nx_time = tv.tv_sec;

	/* Here req.body.cmn.req_hdr.comp_i will be set */
	rv = nx_os_event_wait_setup(adapter, &req, NULL, &swait);
	if(rv) {
		nx_nic_print3(adapter, "os event setup failed: \n");
		return rv;
	}

	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);
	if(rv) {
		nx_nic_print3(adapter, "Sending finger_print request to FW failed: %d\n", rv);
		return rv;
	}
	rv = nx_os_event_wait(adapter, &swait, 5000);
	if(rv) {
		nx_nic_print3(adapter, "nx os event wait failed\n");
		return rv;	
	}
	memcpy(nx_lic_finger_print, adapter->nx_lic_dma.addr,
			sizeof(nx_finger_print_t));
	return (rv);
}

int nx_install_license(struct unm_adapter_s *adapter) {

	nic_request_t   req;
	nx_install_license_request_t *lic_req;
	int          rv = 0;

	memset(&req, 0, sizeof(req));
	req.opcode = NX_NIC_HOST_REQUEST;
	req.qmsg_type = UNM_MSGTYPE_NIC_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_INSTALL_LICENSE_REQUEST;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;
	lic_req = (nx_install_license_request_t *)&req.body;
	lic_req->ring_ctx = adapter->portnum;
	lic_req->dma_to_addr = (__uint64_t)(adapter->nx_lic_dma.phys_addr);
	lic_req->dma_size = (__uint32_t)(sizeof(nx_install_license_t));
	
	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);

	if(rv) {
		nx_nic_print3(adapter, "Sending Install License request to FW failed: %d\n", rv);
		return rv;
	}
	return (rv);
}

int nx_get_capabilty_request(struct unm_adapter_s *adapter, nx_license_capabilities_t *nx_lic_capabilities) {

	nic_request_t   req;
	nx_get_license_capability_request_t *lic_req;
	int          rv = 0;
	nx_os_wait_event_t swait;

	memset(&req, 0, sizeof(req));
	memset(adapter->nx_lic_dma.addr, 0, sizeof(nx_license_capabilities_t));

	req.opcode = NX_NIC_HOST_REQUEST;
	req.qmsg_type = UNM_MSGTYPE_NIC_REQUEST;
	req.body.cmn.req_hdr.opcode = NX_NIC_H2C_OPCODE_GET_LICENSE_CAPABILITY_REQUEST;
	req.body.cmn.req_hdr.ctxid = adapter->portnum;
	lic_req = (nx_get_license_capability_request_t *)&req.body;
	lic_req->ring_ctx = adapter->portnum;

	lic_req->dma_to_addr = (__uint64_t)(adapter->nx_lic_dma.phys_addr);
	lic_req->dma_size = (__uint32_t)(sizeof(nx_license_capabilities_t));

	/* Here req.body.cmn.req_hdr.comp_i will be set */
	rv = nx_os_event_wait_setup(adapter, &req, NULL, &swait);
	if(rv) {
		nx_nic_print3(adapter,"os event setup failed: \n");
		return rv;
	}

	rv = nx_nic_send_cmd_descs(adapter->netdev, (cmdDescType0_t *)&req, 1);
	if(rv) {
		nx_nic_print3(adapter, "Sending get_capabilty request to FW failed: %d\n", rv);
		return rv;
	}
	rv = nx_os_event_wait(adapter, &swait, 5000);
	if(rv) {
		nx_nic_print3(adapter, "nx os event wait failed\n");
		return rv;
	}
	memcpy(nx_lic_capabilities, adapter->nx_lic_dma.addr, sizeof(nx_license_capabilities_t));
	return (rv);
}

