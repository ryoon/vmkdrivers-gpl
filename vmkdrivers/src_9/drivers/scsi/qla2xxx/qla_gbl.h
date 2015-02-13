/*
 * QLogic Fibre Channel HBA Driver
 * Copyright (c)  2003-2008 QLogic Corporation
 *
 * See LICENSE.qla2xxx for copyright and licensing details.
 */
#ifndef __QLA_GBL_H
#define	__QLA_GBL_H

#include <linux/interrupt.h>
#include <linux/mutex.h>

/*
 * Global Function Prototypes in qla_init.c source file.
 */
extern int qla2x00_is_wwn_zero(uint8_t *wwn);
extern int qla2x00_initialize_adapter(scsi_qla_host_t *);

extern int qla2100_pci_config(struct scsi_qla_host *);
extern int qla2300_pci_config(struct scsi_qla_host *);
extern int qla24xx_pci_config(scsi_qla_host_t *);
extern int qla25xx_pci_config(scsi_qla_host_t *);
extern void qla2x00_reset_chip(struct scsi_qla_host *);
extern void qla24xx_reset_chip(struct scsi_qla_host *);
extern int qla2x00_chip_diag(struct scsi_qla_host *);
extern int qla24xx_chip_diag(struct scsi_qla_host *);
extern void qla2x00_config_rings(struct scsi_qla_host *);
extern void qla24xx_config_rings(struct scsi_qla_host *);
extern void qla2x00_reset_adapter(struct scsi_qla_host *);
extern void qla24xx_reset_adapter(struct scsi_qla_host *);
extern int qla2x00_nvram_config(struct scsi_qla_host *);
extern int qla24xx_nvram_config(struct scsi_qla_host *);
extern int qla81xx_nvram_config(struct scsi_qla_host *);
extern void qla2x00_update_fw_options(struct scsi_qla_host *);
extern void qla24xx_update_fw_options(scsi_qla_host_t *);
extern void qla81xx_update_fw_options(scsi_qla_host_t *);
extern int qla2x00_load_risc(struct scsi_qla_host *, uint32_t *);
extern int qla24xx_load_risc(scsi_qla_host_t *, uint32_t *);
extern int qla81xx_load_risc(scsi_qla_host_t *, uint32_t *);

extern int qla2x00_loop_resync(scsi_qla_host_t *);

extern int qla2x00_fabric_login(scsi_qla_host_t *, fc_port_t *, uint16_t *);
extern int qla2x00_local_device_login(scsi_qla_host_t *, fc_port_t *);

extern void qla2x00_restart_queues(scsi_qla_host_t *, uint8_t);

extern void qla2x00_update_fcports(scsi_qla_host_t *);

extern int qla2x00_abort_isp(scsi_qla_host_t *);

extern int qla2xxx_update_fcport_fcp_prio(scsi_qla_host_t *, fc_port_t *);
extern int qla2xxx_update_all_fcp_prio(scsi_qla_host_t *);
extern void qla2x00_update_fcport(scsi_qla_host_t *, fc_port_t *);

extern void qla2x00_alloc_fw_dump(scsi_qla_host_t *);
extern void qla2x00_try_to_stop_firmware(scsi_qla_host_t *);
extern void qla84xx_put_chip(struct scsi_qla_host *);

/*
 * Global Data in qla_os.c source file.
 */
extern char qla2x00_version_str[];

extern int ql2xlogintimeout;
extern int qlport_down_retry;
extern int ql2xplogiabsentdevice;
extern int ql2xloginretrycount;
extern int ql2xfdmienable;
extern int ql2xallocfwdump;
extern int ql2xextended_error_logging;
extern int ql2xqfullrampup;
extern int ql2xqfulltracking;
extern int ql2xiidmaenable;
extern int ql2xenablemsi;
extern int ql2xenablemsi24xx;
extern int ql2xenablemsi2422;
extern int ql2xcmdtimeout;
extern int ql2xexecution_throttle;
extern int ql2xusedefmaxrdreq;
extern int ql2xmqqos;
extern int ql2xmqcpuaffinity;
extern int ql2xfwloadbin;

extern int num_hosts;
extern int apiHBAInstance;
/* Protects hostlist and host_instance_map */
extern struct mutex instance_lock; 
extern unsigned long host_instance_map[(MAX_HBAS / 8)/ sizeof(unsigned long)];
#if defined(__VMKLNX__)
extern int ql2xoperationmode;
extern int ql2xintrdelaytimer;
#endif

extern struct list_head qla_hostlist;

extern int ql2xioctltimeout;
extern int qla2x00_loop_reset(scsi_qla_host_t *);
extern void qla2x00_abort_all_cmds(scsi_qla_host_t *, int);
extern int qla2x00_post_hwe_work(struct scsi_qla_host *, uint16_t , uint16_t,
    uint16_t, uint16_t);
extern void qla2x00_cmd_timeout(srb_t *);
extern void qla2x00_extend_timeout(struct scsi_cmnd *, int);
extern void qla23xx_blink_led(scsi_qla_host_t *);
extern void qla2x00_abort_fcport_cmds(fc_port_t *);
extern void qla2xxx_vha_put(struct scsi_qla_host *);
extern void qla2xxx_vha_get(struct scsi_qla_host *);
extern struct scsi_qla_host *qla2x00_create_host(struct scsi_host_template *,
	struct qla_hw_data *);
extern void qla2x00_free_host(struct scsi_qla_host *);
extern void qla2x00_relogin(struct scsi_qla_host *);
extern int qla2x00_post_idc_ack_work(struct scsi_qla_host *, uint16_t *);
extern int qla2x00_eh_wait_for_pending_commands(struct scsi_qla_host *);
/*
 * Global Functions in qla_mid.c source file.
 */
extern struct scsi_host_template qla2x00_driver_template;
extern struct scsi_host_template qla24xx_driver_template;
extern struct scsi_transport_template *qla2xxx_transport_vport_template;
extern struct scsi_transport_template *qla2xxx_transport_template;
extern uint8_t qla2x00_mem_alloc(struct qla_hw_data *,
	uint16_t, uint16_t,  struct req_que **, struct rsp_que **);
extern void qla2x00_timer(scsi_qla_host_t *);
extern void qla2x00_start_timer(scsi_qla_host_t *, void *, unsigned long);
extern void qla2x00_stop_timer(scsi_qla_host_t *);
extern uint32_t qla24xx_allocate_vp_id(scsi_qla_host_t *);
extern void qla24xx_deallocate_vp_id(scsi_qla_host_t *);
extern int qla24xx_disable_vp(scsi_qla_host_t *);
extern int qla24xx_enable_vp(scsi_qla_host_t *);
extern void qla2x00_mem_free(struct qla_hw_data *);
extern int qla24xx_control_vp(scsi_qla_host_t *, int );
extern int qla24xx_modify_vp_config(scsi_qla_host_t *);
extern int qla2x00_send_change_request(scsi_qla_host_t *, uint16_t, uint16_t);
extern void qla2x00_vp_stop_timer(scsi_qla_host_t *);
extern int qla24xx_configure_vhba (scsi_qla_host_t *);
extern int qla24xx_get_vp_entry(scsi_qla_host_t *, uint16_t, int);
extern int qla24xx_get_vp_database(scsi_qla_host_t *, uint16_t);
extern int qla2x00_do_dpc_vp(scsi_qla_host_t *);
extern void qla24xx_report_id_acquisition(scsi_qla_host_t *,
    struct vp_rpt_id_entry_24xx *);
extern scsi_qla_host_t * qla24xx_find_vhost_by_name(struct qla_hw_data *,
    uint8_t *);
extern void qla2x00_do_dpc_all_vps(scsi_qla_host_t *);
extern int qla24xx_vport_create_req_sanity_check(struct fc_vport *);
extern scsi_qla_host_t * qla24xx_create_vhost(struct fc_vport *, uint8_t);
extern int qla2xxx_add_boot_host(scsi_qla_host_t *);
extern void qla2xxx_initialize_boothost(scsi_qla_host_t *);
extern void qla2xxx_delete_boothost(scsi_qla_host_t *, uint8_t);

extern void qla2x00_sp_compl(struct qla_hw_data *, srb_t *);

extern char *qla2x00_get_fw_version_str(struct scsi_qla_host *, char *);

extern void qla2x00_mark_device_lost(scsi_qla_host_t *, fc_port_t *, int, int);
extern void qla2x00_mark_all_devices_lost(scsi_qla_host_t *, int);

extern int qla2x00_down_timeout(struct semaphore *, unsigned long);

extern struct fw_blob *qla2x00_request_firmware(scsi_qla_host_t *);

extern int qla2x00_wait_for_hba_online(scsi_qla_host_t *);
extern int qla2x00_wait_for_chip_reset(scsi_qla_host_t *);

extern void qla2xxx_wake_dpc(scsi_qla_host_t *);
extern void qla2x00_alert_all_vps(struct rsp_que *, uint16_t *);
extern void qla2x00_async_event(scsi_qla_host_t *, struct rsp_que *,
	uint16_t *);
extern int qla2x00_vp_abort_isp(scsi_qla_host_t *);
extern int qla24xx_vport_delete(struct fc_vport *);
extern int qla2x00_eh_wait_on_command(struct scsi_cmnd *);
extern int qla2x00_eh_wait_for_vp_pending_commands(struct scsi_qla_host *);
/*
 * Global Function Prototypes in qla_iocb.c source file.
 */
extern void qla2x00_isp_cmd(scsi_qla_host_t *, struct req_que *);
extern uint16_t qla2x00_calc_iocbs_32(uint16_t);
extern uint16_t qla2x00_calc_iocbs_64(uint16_t);
extern void qla2x00_build_scsi_iocbs_32(srb_t *, cmd_entry_t *, uint16_t);
extern void qla2x00_build_scsi_iocbs_64(srb_t *, cmd_entry_t *, uint16_t);
extern int qla2x00_start_scsi(srb_t *sp);
extern int qla24xx_start_scsi(srb_t *sp);
int qla2x00_marker(scsi_qla_host_t *, struct req_que *, struct rsp_que *,
	uint16_t, uint16_t, uint8_t);
int __qla2x00_marker(scsi_qla_host_t *, struct req_que *, struct rsp_que *,
	uint16_t, uint16_t, uint8_t);
extern request_t *qla2x00_ms_req_pkt(scsi_qla_host_t *, struct req_que *,
	struct rsp_que *, srb_t *);

/*
 * Global Function Prototypes in qla_mbx.c source file.
 */
#include "exioct.h"
#include "inioct.h"

extern int
qla2x00_send_rnid_mbx(scsi_qla_host_t *, uint16_t, uint8_t,
dma_addr_t , size_t , uint16_t *);

extern int
qla2x00_set_rnid_params_mbx(scsi_qla_host_t *, dma_addr_t, size_t, uint16_t *);

extern int
qla2x00_echo_test(scsi_qla_host_t *, INT_LOOPBACK_REQ *, uint16_t *);

extern int
qla2x00_loopback_test(scsi_qla_host_t *, INT_LOOPBACK_REQ *, uint16_t *);

extern int
qla2x00_get_rnid_params_mbx(scsi_qla_host_t *, dma_addr_t , size_t, uint16_t *);

extern int
qla2x00_load_ram(scsi_qla_host_t *, dma_addr_t, uint32_t, uint32_t);

extern int
qla2x00_dump_ram(scsi_qla_host_t *, dma_addr_t, uint32_t, uint32_t);

extern int
qla2x00_execute_fw(scsi_qla_host_t *, uint32_t);

extern int
qla2x00_get_fw_version(scsi_qla_host_t *, uint16_t *, uint16_t *, uint16_t *,
		uint16_t *, uint32_t *, uint8_t *, uint32_t *, uint8_t *);

extern int
qla2x00_get_fw_options(scsi_qla_host_t *, uint16_t *);

extern int
qla2x00_set_fw_options(scsi_qla_host_t *, uint16_t *);

extern int
qla2x00_mbx_reg_test(scsi_qla_host_t *);

extern int
qla2x00_verify_checksum(scsi_qla_host_t *, uint32_t);

extern int
qla2x00_issue_iocb(scsi_qla_host_t *, void *, dma_addr_t, size_t);

extern int
qla2x00_issue_iocb_timeout(scsi_qla_host_t *, void *, dma_addr_t, size_t,
    uint32_t);

extern int
qla2x00_abort_command(srb_t *);

extern int
qla2x00_abort_target(fc_port_t *, int);

extern int
qla2x00_get_adapter_id(scsi_qla_host_t *, uint16_t *, uint8_t *, uint8_t *,
    uint8_t *, uint16_t *, uint16_t *);

extern int
qla2x00_get_retry_cnt(scsi_qla_host_t *, uint8_t *, uint8_t *, uint16_t *);

extern int
qla2x00_init_firmware(scsi_qla_host_t *, uint16_t);

extern int
qla2x00_get_port_database(scsi_qla_host_t *, fc_port_t *, uint8_t);

extern int
qla2x00_get_firmware_state(scsi_qla_host_t *, uint16_t *);

extern int
qla2x00_get_port_name(scsi_qla_host_t *, uint16_t, uint8_t *, uint8_t);

extern int
qla2x00_lip_reset(scsi_qla_host_t *);

extern int
qla2x00_send_sns(scsi_qla_host_t *, dma_addr_t, uint16_t, size_t);

extern int
qla2x00_login_fabric(scsi_qla_host_t *, uint16_t, uint8_t, uint8_t, uint8_t,
    uint16_t *, uint8_t);
extern int
qla24xx_login_fabric(scsi_qla_host_t *, uint16_t, uint8_t, uint8_t, uint8_t,
    uint16_t *, uint8_t);

extern int
qla2x00_login_local_device(scsi_qla_host_t *, fc_port_t *, uint16_t *,
    uint8_t);

extern int
qla2x00_fabric_logout(scsi_qla_host_t *, uint16_t, uint8_t, uint8_t, uint8_t);

extern int
qla24xx_fabric_logout(scsi_qla_host_t *, uint16_t, uint8_t, uint8_t, uint8_t);

extern int
qla2x00_full_login_lip(scsi_qla_host_t *ha);

extern int
qla2x00_get_id_list(scsi_qla_host_t *, void *, dma_addr_t, uint16_t *);

extern int
qla2x00_get_resource_cnts(scsi_qla_host_t *, uint16_t *, uint16_t *, 
    uint16_t *, uint16_t *, uint16_t *);

extern int
qla2x00_get_fcal_position_map(scsi_qla_host_t *ha, char *pos_map);

extern int
qla2x00_get_link_status(scsi_qla_host_t *, uint16_t, struct link_statistics *,
    dma_addr_t);

extern int
qla24xx_get_isp_stats(scsi_qla_host_t *, struct link_statistics *,
    dma_addr_t);

extern int qla24xx_abort_command(srb_t *);
extern int qla24xx_abort_target(fc_port_t *, int);

extern int qla2x00_system_error(scsi_qla_host_t *);

extern int
qla2x00_get_serdes_params(scsi_qla_host_t *, uint16_t *, uint16_t *,
    uint16_t *);

extern int
qla2x00_set_serdes_params(scsi_qla_host_t *, uint16_t, uint16_t, uint16_t);

extern int
qla2x00_stop_firmware(scsi_qla_host_t *);

extern int
qla2x00_enable_eft_trace(scsi_qla_host_t *, dma_addr_t, uint16_t);
extern int
qla2x00_disable_eft_trace(scsi_qla_host_t *);

extern int
qla2x00_read_sfp(scsi_qla_host_t *, dma_addr_t, uint16_t, uint16_t, uint16_t);

extern int
qla2x00_read_edc(scsi_qla_host_t *, uint16_t, uint16_t, dma_addr_t,
	uint8_t *, uint16_t, uint16_t);
 
extern int
qla2x00_write_edc(scsi_qla_host_t *, uint16_t, uint16_t, dma_addr_t,
	uint8_t *, uint16_t, uint16_t);

extern int
qla2x00_get_idma_speed(scsi_qla_host_t *, uint16_t, uint16_t *, uint16_t *);

extern int
qla2x00_set_idma_speed(scsi_qla_host_t *, uint16_t, uint16_t, uint16_t *);

extern int
qla2xxx_set_fcp_prio(scsi_qla_host_t *, uint16_t, uint16_t, uint16_t *);

#if defined(__VMKLNX__)
extern int 
qla2x00_lun_reset(scsi_qla_host_t *, fc_port_t *, uint16_t, int);

extern int 
qla24xx_lun_reset(scsi_qla_host_t *, fc_port_t *, uint16_t, int);
#endif

extern int
qla84xx_reset_chip(scsi_qla_host_t *, uint16_t, uint16_t *);
extern int
qla2x00_get_data_rate(scsi_qla_host_t *);
extern int
qla2x00_get_thermal_temp(scsi_qla_host_t *, uint16_t *, uint16_t *);


extern int qla81xx_restart_mpi_firmware(scsi_qla_host_t *, uint16_t *);

/*
 * Global Function Prototypes in qla_isr.c source file.
 */
extern irqreturn_t qla24xx_msix_poll(int, void *);
extern irqreturn_t qla2100_intr_handler(int, void *);
extern irqreturn_t qla2300_intr_handler(int, void *);
extern irqreturn_t qla24xx_intr_handler(int, void *);
extern void qla2x00_process_response_queue(struct rsp_que *);
extern void qla24xx_process_response_queue(struct scsi_qla_host *, struct rsp_que *);

extern int qla84xx_verify_chip(struct scsi_qla_host *, uint16_t *);

extern int qla2x00_request_irqs(struct qla_hw_data *, struct rsp_que *);
extern void qla2x00_free_irqs(scsi_qla_host_t *);
extern int qla81xx_idc_ack(scsi_qla_host_t *, uint16_t *);
extern int
qla81xx_fac_get_sector_size(scsi_qla_host_t *, uint32_t *);
extern int
qla81xx_fac_do_write_enable(scsi_qla_host_t *, int);
extern int
qla81xx_fac_erase_sector(scsi_qla_host_t *, uint32_t, uint32_t);
extern int
qla81xx_get_xgmac_stats(scsi_qla_host_t *, dma_addr_t, uint16_t);
extern int
qla81xx_get_dcbx_params(scsi_qla_host_t *, dma_addr_t, uint16_t);

extern int
qla2x00_read_ram_word(scsi_qla_host_t *, uint32_t, uint32_t *);

extern int
qla2x00_write_ram_word(scsi_qla_host_t *, uint32_t, uint32_t);

/*
 * Global Function Prototypes in qla_sup.c source file.
 */
extern void qla2x00_lock_nvram_access(struct qla_hw_data *);
extern void qla2x00_unlock_nvram_access(struct qla_hw_data *);
extern void qla2x00_release_nvram_protection(scsi_qla_host_t *);
extern uint16_t qla2x00_get_nvram_word(struct qla_hw_data *, uint32_t);
extern void qla2x00_write_nvram_word(struct qla_hw_data *, uint32_t, uint16_t);
extern uint32_t *qla24xx_read_flash_data(scsi_qla_host_t *, uint32_t *,
    uint32_t, uint32_t);
extern uint8_t *qla2x00_read_nvram_data(scsi_qla_host_t *, uint8_t *, uint32_t,
    uint32_t);
extern uint8_t *qla24xx_read_nvram_data(scsi_qla_host_t *, uint8_t *, uint32_t,
    uint32_t);
extern int qla2x00_write_nvram_data(scsi_qla_host_t *, uint8_t *, uint32_t,
    uint32_t);
extern int qla24xx_write_nvram_data(scsi_qla_host_t *, uint8_t *, uint32_t,
    uint32_t);
extern uint8_t *qla25xx_read_nvram_data(scsi_qla_host_t *, uint8_t *, uint32_t,
    uint32_t);
extern int qla25xx_write_nvram_data(scsi_qla_host_t *, uint8_t *, uint32_t,
    uint32_t);

extern int qla2x00_beacon_on(struct scsi_qla_host *);
extern int qla2x00_beacon_off(struct scsi_qla_host *);
extern void qla2x00_beacon_blink(struct scsi_qla_host *);
extern int qla24xx_beacon_on(struct scsi_qla_host *);
extern int qla24xx_beacon_off(struct scsi_qla_host *);
extern void qla24xx_beacon_blink(struct scsi_qla_host *);

extern uint8_t *qla2x00_read_optrom_data(struct scsi_qla_host *, uint8_t *,
    uint32_t, uint32_t);
extern int qla2x00_write_optrom_data(struct scsi_qla_host *, uint8_t *,
    uint32_t, uint32_t);
extern uint8_t *qla24xx_read_optrom_data(struct scsi_qla_host *, uint8_t *,
    uint32_t, uint32_t);
extern int qla24xx_write_optrom_data(struct scsi_qla_host *, uint8_t *,
    uint32_t, uint32_t);
extern uint8_t *qla25xx_read_optrom_data(struct scsi_qla_host *, uint8_t *,
    uint32_t, uint32_t);

extern int qla2x00_get_flash_version(scsi_qla_host_t *, void *);
extern int qla24xx_get_flash_version(scsi_qla_host_t *, void *);

extern int qla2xxx_get_flash_info(scsi_qla_host_t *);
extern int qla2xxx_get_vpd_field(scsi_qla_host_t *, char *, char *, size_t);

extern int qla2xxx_read_fcp_prio_cfg(scsi_qla_host_t *);

extern void qla2xxx_flash_npiv_conf(scsi_qla_host_t *, uint8_t);
extern void qla2xxx_npiv_cleanup(scsi_qla_host_t *);
/*
 * Global Function Prototypes in qla_dbg.c source file.
 */
extern void qla2100_fw_dump(scsi_qla_host_t *, int);
extern void qla2300_fw_dump(scsi_qla_host_t *, int);
extern void qla24xx_fw_dump(scsi_qla_host_t *, int);
extern void qla25xx_fw_dump(scsi_qla_host_t *, int);
extern void qla81xx_fw_dump(scsi_qla_host_t *, int);
extern void qla2x00_dump_regs(scsi_qla_host_t *);
extern void qla2x00_dump_buffer(uint8_t *, uint32_t);
extern void qla2x00_print_scsi_cmd(struct scsi_cmnd *);
extern void qla2x00_dump_pkt(void *);

/*
 * Global Function Prototypes in qla_gs.c source file.
 */
extern int qla2x00_mgmt_svr_login(scsi_qla_host_t *);
extern void *qla2x00_prep_ms_iocb(scsi_qla_host_t *, uint32_t, uint32_t);
extern void *qla24xx_prep_ms_iocb(scsi_qla_host_t *, uint32_t, uint32_t);
extern int qla2x00_ga_nxt(scsi_qla_host_t *, fc_port_t *);
extern int qla2x00_gid_pt(scsi_qla_host_t *, sw_info_t *);
extern int qla2x00_gpn_id(scsi_qla_host_t *, sw_info_t *);
extern int qla2x00_gnn_id(scsi_qla_host_t *, sw_info_t *);
extern int qla2x00_rft_id(scsi_qla_host_t *);
extern int qla2x00_rff_id(scsi_qla_host_t *);
extern int qla2x00_rnn_id(scsi_qla_host_t *);
extern int qla2x00_rsnn_nn(scsi_qla_host_t *);
extern void *qla2x00_prep_ms_fdmi_iocb(scsi_qla_host_t *, uint32_t, uint32_t);
extern void *qla24xx_prep_ms_fdmi_iocb(scsi_qla_host_t *, uint32_t, uint32_t);
extern int qla2x00_fdmi_register(scsi_qla_host_t *);
extern int qla2x00_gfpn_id(scsi_qla_host_t *, sw_info_t *);
extern int qla2x00_gpsc(scsi_qla_host_t *, sw_info_t *);
extern void qla2x00_get_sym_node_name(scsi_qla_host_t *, uint8_t *);

/*
 * Global Function Prototypes in qla_xioctl.c source file.
 */
extern void * Q64BIT_TO_PTR(uint64_t, uint16_t);

extern int qla2x00_ioctl(struct scsi_device *, int , void *);
extern int qla2x00_ioctl_init(void);
extern int qla2x00_ioctl_exit(void);
extern void qla2x00_aen_flush(scsi_qla_host_t *);
extern void qla2x00_enqueue_aen(scsi_qla_host_t *, uint16_t, void *);
extern int qla2x00_alloc_ioctl_mem(scsi_qla_host_t *);
extern void qla2x00_free_ioctl_mem(scsi_qla_host_t *);
extern int qla2x00_get_ioctl_scrap_mem(scsi_qla_host_t *, void **, uint32_t);
extern void qla2x00_free_ioctl_scrap_mem(scsi_qla_host_t *);
extern int qla2x00_get_new_ioctl_dma_mem(scsi_qla_host_t *, uint32_t);

extern int qla2xxx_fcp_prio_cfg_valid(struct qla_fcp_prio_cfg *, uint8_t);

/*
 * Global Function Prototypes in qla_attr.c source file.
 */
struct class_device_attribute;
extern struct class_device_attribute *qla2x00_host_attrs[];
struct fc_function_template;
extern struct fc_function_template qla2xxx_transport_functions;
extern struct fc_function_template qla2xxx_transport_vport_functions;
extern void qla2x00_init_host_attr(scsi_qla_host_t *);

/* Global function prototypes for multi-q */
extern void qla25xx_config_rings(struct scsi_qla_host *);
extern int qla25xx_request_irq(struct rsp_que *);
extern int qla25xx_init_req_que(struct scsi_qla_host *, struct req_que *);
extern int qla25xx_init_rsp_que(struct scsi_qla_host *, struct rsp_que *);
extern int qla25xx_create_req_que(struct qla_hw_data *, uint16_t, uint8_t,
      uint16_t, int, uint8_t);
extern int qla25xx_create_rsp_que(struct qla_hw_data *, uint16_t, uint8_t,
      uint16_t, int);
extern int qla25xx_update_req_que(struct scsi_qla_host *, uint8_t, uint8_t);
extern void qla2x00_init_response_q_entries(struct rsp_que *);
extern int qla25xx_delete_req_que(struct scsi_qla_host *, struct req_que *);
extern int qla25xx_delete_rsp_que(struct scsi_qla_host *, struct rsp_que *);
extern int qla25xx_create_queues(struct scsi_qla_host *, uint8_t);
extern int qla25xx_delete_queues(struct scsi_qla_host *);
extern void qla2x00_relogin(struct scsi_qla_host *);

#endif /* _QLA_GBL_H */
