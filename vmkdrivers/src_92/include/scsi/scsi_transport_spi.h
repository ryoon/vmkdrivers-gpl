/*
 * Portions Copyright 2008 VMware, Inc.
 */
/* 
 *  Parallel SCSI (SPI) transport specific attributes exported to sysfs.
 *
 *  Copyright (c) 2003 Silicon Graphics, Inc.  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef SCSI_TRANSPORT_SPI_H
#define SCSI_TRANSPORT_SPI_H

#include <linux/transport_class.h>
#include <linux/mutex.h>

struct scsi_transport_template;
struct scsi_target;
struct scsi_device;
struct Scsi_Host;

struct spi_transport_attrs {
	int period;		/* value in the PPR/SDTR command */
	int min_period;
	int offset;
	int max_offset;
	unsigned int width:1;	/* 0 - narrow, 1 - wide */
	unsigned int max_width:1;
	unsigned int iu:1;	/* Information Units enabled */
	unsigned int dt:1;	/* DT clocking enabled */
	unsigned int qas:1;	/* Quick Arbitration and Selection enabled */
	unsigned int wr_flow:1;	/* Write Flow control enabled */
	unsigned int rd_strm:1;	/* Read streaming enabled */
	unsigned int rti:1;	/* Retain Training Information */
	unsigned int pcomp_en:1;/* Precompensation enabled */
	unsigned int hold_mcs:1;/* Hold Margin Control Settings */
	unsigned int initial_dv:1; /* DV done to this target yet  */
	unsigned long flags;	/* flags field for drivers to use */
	/* Device Properties fields */
	unsigned int support_sync:1; /* synchronous support */
	unsigned int support_wide:1; /* wide support */
	unsigned int support_dt:1; /* allows DT phases */
	unsigned int support_dt_only; /* disallows ST phases */
	unsigned int support_ius; /* support Information Units */
	unsigned int support_qas; /* supports quick arbitration and selection */
	/* Private Fields */
	unsigned int dv_pending:1; /* Internal flag */
	unsigned int dv_in_progress:1; /* DV started  - Moved from 2.6.21 */
#if defined(__VMKLNX__)
	unsigned int attr_initialized:1; /* We initialize this only once */
#endif
	struct mutex dv_mutex; /* semaphore to serialise dv */
};

enum spi_signal_type {
	SPI_SIGNAL_UNKNOWN = 1,
	SPI_SIGNAL_SE,
	SPI_SIGNAL_LVD,
	SPI_SIGNAL_HVD,
};

struct spi_host_attrs {
	enum spi_signal_type signalling;
};

/* accessor functions */

/**
 *  spi_period - Access the SCSI target's request period
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's SDTR or PPR period
 *
 *  SYNOPSIS:
 *     #define spi_period(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_period */
#define spi_period(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->period)

/**
 *  spi_min_period - Access the SCSI target's minimum synchronization period
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's minimum synchronization
 *  period
 *
 *  SYNOPSIS:
 *     #define spi_min_period(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_min_period */
#define spi_min_period(x) (((struct spi_transport_attrs *)&(x)->starget_data)->min_period)

/**
 *  spi_offset - Access the SCSI target's current synchronization offset
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's current synchronization
 *  offset
 *
 *  SYNOPSIS:
 *     #define spi_offset(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_offset */
#define spi_offset(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->offset)

/**
 *  spi_max_offset - Access the SCSI target's maximum synchronization offset
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's maximum synchronization
 *  offset
 *
 *  SYNOPSIS:
 *     #define spi_max_offset(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_max_offset */
#define spi_max_offset(x) (((struct spi_transport_attrs *)&(x)->starget_data)->max_offset)

/**
 *  spi_width - Access the SCSI target's width flag
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's width flag, indicating
 *  whether the target can perform wide-SCSI transfers
 *
 *  SYNOPSIS:
 *     #define spi_width(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_width */
#define spi_width(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->width)

/**
 *  spi_max_width - Access the SCSI target's max_width flag
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's max_width flag.  Usually
 *  this is used to indicate whether the target can perform wide-SCSI transfers.
 *
 *  SYNOPSIS:
 *     #define spi_max_width(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_max_width */
#define spi_max_width(x) (((struct spi_transport_attrs *)&(x)->starget_data)->max_width)

/**
 *  spi_iu - Access the SCSI target's setting for supporting information units
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting
 *  for the target's support of information units
 *
 *  SYNOPSIS:
 *     #define spi_iu(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_iu */
#define spi_iu(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->iu)

/**
 *  spi_dt - Access the SCSI target's setting for double-transition clocking
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting for
 *  the target's support of double-transition clocking
 *
 *  SYNOPSIS:
 *     #define spi_dt(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_dt */
#define spi_dt(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->dt)

/**
 *  spi_qas - Access the SCSI target's setting for quick-arbitration-and-selection
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting for
 *  quick arbitration and selection
 *
 *  SYNOPSIS:
 *     #define spi_qas(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_qas */
#define spi_qas(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->qas)

/**
 *  spi_wr_flow - Access the SCSI target's setting for write flow control
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting for
 *  write flow control
 *
 *  SYNOPSIS:
 *     #define spi_wr_flow(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_wr_flow */
#define spi_wr_flow(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->wr_flow)

/**
 *  spi_rd_strm - Access the SCSI target's setting for read streaming
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting for
 *  read streaming
 *
 *  SYNOPSIS:
 *     #define spi_rd_strm(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_rd_strm */
#define spi_rd_strm(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->rd_strm)

/**
 *  spi_rti - Access the SCSI target's setting for retaining training information
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting for
 *  retaining training information
 *
 *  SYNOPSIS:
 *     #define spi_rti(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_rti */
#define spi_rti(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->rti)

/**
 *  spi_pcomp_en - Access the SCSI target's setting for enabling precompensation
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting for
 *  enabling precompensation
 *
 *  SYNOPSIS:
 *     #define spi_pcomp_en(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_pcomp_en */
#define spi_pcomp_en(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->pcomp_en)

/**
 *  spi_hold_mcs - Access the SCSI target's setting for hold margin control
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's configuration setting for
 *  hold margin control
 *
 *  SYNOPSIS:
 *     #define spi_hold_mcs(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_hold_mcs */
#define spi_hold_mcs(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->hold_mcs)

/**
 *  spi_initial_dv - Access the SCSI target's indicator for initial domain validation
 *  @x: SCSI target to access
 *
 *  Use this macro to read or write the SCSI target's flag indicating whether or
 *  not initial domain validation has happened yet
 *
 *  SYNOPSIS:
 *     #define spi_initial_dv(x)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_initial_dv */
#define spi_initial_dv(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->initial_dv)

#define spi_support_sync(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->support_sync)
#define spi_support_wide(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->support_wide)
#define spi_support_dt(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->support_dt)
#define spi_support_dt_only(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->support_dt_only)
#define spi_support_ius(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->support_ius)
#define spi_support_qas(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->support_qas)

#define spi_flags(x)	(((struct spi_transport_attrs *)&(x)->starget_data)->flags)

/**
 *  spi_signalling - Access the SCSI host's current signal flags
 *  @h: SCSI target to access
 *
 *  Use this macro to read or write the SCSI host's signal flags
 *
 *  SYNOPSIS:
 *     #define spi_signalling(h)
 *
 *  RETURN VALUE:
 *  None
 *
 */
/* _VMKLNX_CODECHECK_: spi_signalling */
#define spi_signalling(h)	(((struct spi_host_attrs *)(h)->shost_data)->signalling)



/* The functions by which the transport class and the driver communicate */
struct spi_function_template {
	void	(*get_period)(struct scsi_target *);
	void	(*set_period)(struct scsi_target *, int);
	void	(*get_offset)(struct scsi_target *);
	void	(*set_offset)(struct scsi_target *, int);
	void	(*get_width)(struct scsi_target *);
	void	(*set_width)(struct scsi_target *, int);
	void	(*get_iu)(struct scsi_target *);
	void	(*set_iu)(struct scsi_target *, int);
	void	(*get_dt)(struct scsi_target *);
	void	(*set_dt)(struct scsi_target *, int);
	void	(*get_qas)(struct scsi_target *);
	void	(*set_qas)(struct scsi_target *, int);
	void	(*get_wr_flow)(struct scsi_target *);
	void	(*set_wr_flow)(struct scsi_target *, int);
	void	(*get_rd_strm)(struct scsi_target *);
	void	(*set_rd_strm)(struct scsi_target *, int);
	void	(*get_rti)(struct scsi_target *);
	void	(*set_rti)(struct scsi_target *, int);
	void	(*get_pcomp_en)(struct scsi_target *);
	void	(*set_pcomp_en)(struct scsi_target *, int);
	void	(*get_hold_mcs)(struct scsi_target *);
	void	(*set_hold_mcs)(struct scsi_target *, int);
	void	(*get_signalling)(struct Scsi_Host *);
	void	(*set_signalling)(struct Scsi_Host *, enum spi_signal_type);
	int	(*deny_binding)(struct scsi_target *);
	/* The driver sets these to tell the transport class it
	 * wants the attributes displayed in sysfs.  If the show_ flag
	 * is not set, the attribute will be private to the transport
	 * class */
	unsigned long	show_period:1;
	unsigned long	show_offset:1;
	unsigned long	show_width:1;
	unsigned long	show_iu:1;
	unsigned long	show_dt:1;
	unsigned long	show_qas:1;
	unsigned long	show_wr_flow:1;
	unsigned long	show_rd_strm:1;
	unsigned long	show_rti:1;
	unsigned long	show_pcomp_en:1;
	unsigned long	show_hold_mcs:1;
};

struct scsi_transport_template *spi_attach_transport(struct spi_function_template *);
void spi_release_transport(struct scsi_transport_template *);
void spi_schedule_dv_device(struct scsi_device *);
void spi_dv_device(struct scsi_device *);
void spi_display_xfer_agreement(struct scsi_target *);
int spi_print_msg(const unsigned char *);

/**
 *  spi_populate_width_msg - Populate a width message's fields
 *  @msg: Pointer to message
 *  @width: Width to set
 *
 *  Use this function to populate a width message's fields
 *
 *  RETURN VALUE:
 *  4
 *
 */
/* _VMKLNX_CODECHECK_: spi_populate_width_msg */
int spi_populate_width_msg(unsigned char *msg, int width);

/**
 *  spi_populate_sync_msg - Populate a sync message's fields
 *  @msg: Pointer to message
 *  @period: Period to set
 *  @offset: Offset to set
 *
 *  Use this function to populate a sync message's fields
 *
 *  RETURN VALUE:
 *  5
 *
 */
/* _VMKLNX_CODECHECK_: spi_populate_sync_msg */
int spi_populate_sync_msg(unsigned char *msg, int period, int offset);

/**
 *  spi_populate_ppr_msg - Populate a ppr message's fields
 *  @msg: Pointer to message
 *  @period: Period to set
 *  @offset: Offset to set
 *  @width: Width to set
 *  @options: Options to set
 *
 *  Use this function to populate a sync message's fields
 *
 *  RETURN VALUE:
 *  8
 *
 */
/* _VMKLNX_CODECHECK_: spi_populate_ppr_msg */
int spi_populate_ppr_msg(unsigned char *msg, int period, int offset, int width,
		int options);

#endif /* SCSI_TRANSPORT_SPI_H */
