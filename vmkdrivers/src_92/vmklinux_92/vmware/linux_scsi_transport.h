/* ****************************************************************
 * Portions Copyright 1998 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/******************************************************************
 *
 * linux_scsi_transport.h
 *
 *      Linux Transport Emulation.
 *
 * From linux-2.6.18-8/drivers/scsi/scsi_transport_fc.c:
 *
 * Copyright (c) 2003 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (C) 2004-2005   James Smart, Emulex Corporation
 *
 * From linux-2.6.18-8/drivers/scsi/scsi_transport_spi.c:
 *
 * Copyright (c) 2003 Silicon Graphics, Inc.  All rights reserved.
 * Copyright (c) 2004, 2005 James Bottomley <James.Bottomley@SteelEye.com>
 *
 ******************************************************************/

#ifndef _LINUX_SCSI_TRANSPORT_H_
#define _LINUX_SCSI_TRANSPORT_H_

#define SETUP_ATTRIBUTE(field)						\
	i->private_attrs[count] = class_device_attr_##field;		\
	if (!i->f->set_##field) {					\
		i->private_attrs[count].attr.mode = S_IRUGO;		\
		i->private_attrs[count].store = NULL;			\
	}								\
	i->attrs[count] = &i->private_attrs[count];			\
	if (i->f->show_##field)						\
		count++

#define SETUP_RELATED_ATTRIBUTE(field, rel_field)			\
	i->private_attrs[count] = class_device_attr_##field;		\
	if (!i->f->set_##rel_field) {					\
		i->private_attrs[count].attr.mode = S_IRUGO;		\
		i->private_attrs[count].store = NULL;			\
	}								\
	i->attrs[count] = &i->private_attrs[count];			\
	if (i->f->show_##rel_field)					\
		count++

#define SETUP_HOST_ATTRIBUTE(field)					\
	i->private_host_attrs[count] = class_device_attr_##field;	\
	if (!i->f->set_##field) {					\
		i->private_host_attrs[count].attr.mode = S_IRUGO;	\
		i->private_host_attrs[count].store = NULL;		\
	}								\
	i->host_attrs[count] = &i->private_host_attrs[count];		\
	count++

#define to_spi_internal(ft)  ((struct spi_internal *)((struct vmklnx_ScsiModule *)ft->module)->transportData)
#define to_xsan_internal(ft)  ((struct xsan_internal *)((struct vmklnx_ScsiModule *)ft->module)->transportData)
#define to_fc_internal(ft)  ((struct fc_internal *)((struct vmklnx_ScsiModule *)ft->module)->transportData)

#define SPI_NUM_ATTRS 14	/* increase this if you add attributes */
#define SPI_OTHER_ATTRS 1	/* Increase this if you add "always
				 * on" attributes */
#define SPI_HOST_ATTRS	1

#define SPI_MAX_ECHO_BUFFER_SIZE	4096

#define DV_LOOPS	3
#define DV_TIMEOUT	(10*HZ)
#define DV_RETRIES	3	/* should only need at most 
				 * two cc/ua clears */
#define spi_dv_pending(x) (((struct spi_transport_attrs *)&(x)->starget_data)->dv_pending)
#define spi_dv_in_progress(x) (((struct spi_transport_attrs *)&(x)->starget_data)->dv_in_progress)
#define spi_attr_initialized(x) (((struct spi_transport_attrs *)&(x)->starget_data)->attr_initialized)
#define spi_dv_mutex(x) (((struct spi_transport_attrs *)&(x)->starget_data)->dv_mutex)

#define DV_SET(x, y)                    \
        if(i->f->set_##x)               \
		VMKAPI_MODULE_CALL_VOID(SCSI_GET_MODULE_ID(shost), i->f->set_##x, sdev->sdev_target, y);

enum spi_compare_returns {
	SPI_COMPARE_SUCCESS,
	SPI_COMPARE_FAILURE,
	SPI_COMPARE_SKIP_TEST,
};

struct spi_internal {
	struct scsi_transport_template t;
	struct spi_function_template *f;
	/* The actual attributes */
	struct class_device_attribute private_attrs[SPI_NUM_ATTRS];
	/* The array of null terminated pointers to attributes 
	 * needed by scsi_sysfs.c */
	struct class_device_attribute *attrs[SPI_NUM_ATTRS + SPI_OTHER_ATTRS + 1];
	struct class_device_attribute private_host_attrs[SPI_HOST_ATTRS];
	struct class_device_attribute *host_attrs[SPI_HOST_ATTRS + 1];
};

struct xsan_internal {
   struct scsi_transport_template t;
   struct xsan_function_template *f;
};

/*
 * Attribute counts pre object type...
 * Increase these values if you add attributes
 */
#define FC_STARGET_NUM_ATTRS 	3
#define FC_RPORT_NUM_ATTRS	10
#define FC_VPORT_NUM_ATTRS	9
#define FC_HOST_NUM_ATTRS	21

#define FC_MAX_WORK_QUEUE_NAME  64

struct fc_internal {
	struct scsi_transport_template t;
	struct fc_function_template *f;

	/*
	 * For attributes : each object has :
	 *   An array of the actual attributes structures
	 *   An array of null-terminated pointers to the attribute
	 *     structures - used for mid-layer interaction.
	 *
	 * The attribute containers for the starget and host are are
	 * part of the midlayer. As the remote port is specific to the
	 * fc transport, we must provide the attribute container.
	 */
	struct class_device_attribute private_starget_attrs[
							FC_STARGET_NUM_ATTRS];
	struct class_device_attribute *starget_attrs[FC_STARGET_NUM_ATTRS + 1];

	struct class_device_attribute private_host_attrs[FC_HOST_NUM_ATTRS];
	struct class_device_attribute *host_attrs[FC_HOST_NUM_ATTRS + 1];

	struct transport_container rport_attr_cont;
	struct class_device_attribute private_rport_attrs[FC_RPORT_NUM_ATTRS];
	struct class_device_attribute *rport_attrs[FC_RPORT_NUM_ATTRS + 1];

	struct transport_container vport_attr_cont;
	struct class_device_attribute private_vport_attrs[FC_VPORT_NUM_ATTRS];
	struct class_device_attribute *vport_attrs[FC_VPORT_NUM_ATTRS + 1];
};


/*
 * SAS Internal Section
 */
                                                                                
#define SAS_HOST_ATTRS          0
#define SAS_PHY_ATTRS           17
#define SAS_PORT_ATTRS          1
#define SAS_RPORT_ATTRS         7
#define SAS_END_DEV_ATTRS       3
#define SAS_EXPANDER_ATTRS      7

struct sas_host_attrs {
        struct list_head rphy_list;
        struct mutex lock;
        u32 next_target_id;
	struct list_head freed_rphy_list; // rphy's that were freed
        u32 next_expander_id;
        int next_port_id;
};
#define to_sas_host_attrs(host) ((struct sas_host_attrs *)(host)->shost_data)
                                                                                
struct sas_internal {
        struct scsi_transport_template t;
        struct sas_function_template *f;
        struct sas_domain_function_template *dft;
                                                                                
        struct class_device_attribute private_host_attrs[SAS_HOST_ATTRS];
        struct class_device_attribute private_phy_attrs[SAS_PHY_ATTRS];
        struct class_device_attribute private_port_attrs[SAS_PORT_ATTRS];
        struct class_device_attribute private_rphy_attrs[SAS_RPORT_ATTRS];
        struct class_device_attribute private_end_dev_attrs[SAS_END_DEV_ATTRS];
        struct class_device_attribute private_expander_attrs[SAS_EXPANDER_ATTRS];
                                                                                
        struct transport_container phy_attr_cont;
        struct transport_container port_attr_cont;
        struct transport_container rphy_attr_cont;
        struct transport_container end_dev_attr_cont;
        struct transport_container expander_attr_cont;
                                                                                
        /*
         * The array of null terminated pointers to attributes
         * needed by scsi_sysfs.c
         */
        struct class_device_attribute *host_attrs[SAS_HOST_ATTRS + 1];
        struct class_device_attribute *phy_attrs[SAS_PHY_ATTRS + 1];
        struct class_device_attribute *port_attrs[SAS_PORT_ATTRS + 1];
        struct class_device_attribute *rphy_attrs[SAS_RPORT_ATTRS + 1];
        struct class_device_attribute *end_dev_attrs[SAS_END_DEV_ATTRS + 1];
        struct class_device_attribute *expander_attrs[SAS_EXPANDER_ATTRS + 1];
};
                                                                                
#define to_sas_internal(tmpl)   container_of(tmpl, struct sas_internal, t)

int vmk_fc_vport_create(struct Scsi_Host *shost,
      struct device *pdev,
      void *data,
      void **vport_shost);

int vmk_fc_vport_delete(struct Scsi_Host *vhost);
int vmk_fc_vport_getinfo(struct Scsi_Host *shp, void *info);
int vmk_fc_vport_suspend(struct Scsi_Host *shost, int suspend);
int spi_setup_transport_attrs(struct scsi_target *starget);
int xsan_setup_transport_attrs(struct Scsi_Host *sh, struct scsi_target *starget);
int vmklnx_xsan_host_setup(struct Scsi_Host *shost);

#endif /* _LINUX_SCSI_TRANSPORT_H */
