/*
 * Portions Copyright 2008 VMware, Inc.
 */
/*
 * Netqueue implementation for vmklinux
 * 
 *    Multi-queue NICs have multiple receive and/or transmit queues that 
 *    can be programmed individually. Netqueue API provides a mechanism 
 *    to discover and program these queues. 
 */

#ifndef _LINUX_NETQUEUE_H
#define _LINUX_NETQUEUE_H
#ifdef __VMKLNX__

#define __VMKNETDDI_QUEUEOPS__ 

/*
 *	IEEE 802.3 Ethernet magic constants.  The frame sizes omit the preamble
 *	and FCS/CRC (frame check sequence). 
 */

#define ETH_ALEN	6		/* Octets in one ethernet addr	 */

/*
 * WARNING: Binary compatibility tripping point.
 *
 * Version here must matchup with that on the kenel side for *a* 
 * given release.
 */
#define VMKNETDDI_QUEUEOPS_MAJOR_VER (2)
#define VMKNETDDI_QUEUEOPS_MINOR_VER (0)

#define VMKNETDDI_QUEUEOPS_OK  (0 )
#define VMKNETDDI_QUEUEOPS_ERR (-1)

#define VMKNETDDI_NETQUEUE_MAX_RSS_QUEUES 16
#define VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE 128
#define VMKNETDDI_NETQUEUE_MAX_RSS_KEY_SIZE 40

struct sk_buff;
struct net_device;
struct net_device_stats;

/*
 * WARNING: Be careful changing constants, binary comtiability may be broken.
 */

#define VMKNETDDI_QUEUEOPS_FEATURE_NONE     \
				((vmknetddi_queueops_features_t)0x0)
#define VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES \
				((vmknetddi_queueops_features_t)0x1)
#define VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES \
				((vmknetddi_queueops_features_t)0x2)

typedef enum vmknetddi_queueops_filter_class {
	VMKNETDDI_QUEUEOPS_FILTER_NONE = 0x0,       /* Invalid filter */
	VMKNETDDI_QUEUEOPS_FILTER_MACADDR = 0x1,    /* Mac address filter */
	VMKNETDDI_QUEUEOPS_FILTER_VLAN = 0x2,       /* Vlan tag filter */
	VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR = 0x4,/* Vlan tag + 
                                                       Mac addr filter */
	VMKNETDDI_QUEUEOPS_FILTER_VXLAN = 0x8,      /* VXLAN filter */
	VMKNETDDI_QUEUEOPS_FILTER_GENEVE = 0x10,    /* Geneve filter */
} vmknetddi_queueops_filter_class_t;

typedef enum vmknetddi_queueops_queue_type {
	VMKNETDDI_QUEUEOPS_QUEUE_TYPE_INVALID = 0x0, /* Invalid queue type */
	VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX = 0x1,      /* Rx queue */
	VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX = 0x2,      /* Tx queue */
} vmknetddi_queueops_queue_t;

typedef u32 vmknetddi_queueops_queueid_t;
typedef u32 vmknetddi_queueops_filterid_t;
typedef u8  vmknetddi_queueops_tx_priority_t;
typedef unsigned long long vmknetddi_queueops_features_t;

typedef enum {
        /* none */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_NONE     = 0x0,
        /* lro feature */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO      = 0x1,
        /* pair queue feature */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR     = 0x2,
        /* RSS queue feature */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS      = 0x4,
        /* RSS queue feature with run time indirection table modification */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN  = 0x8,
        /* Latency queue feature */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LATENCY      = 0x10,
        /* Dynamic pool queue feature */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_DYNAMIC      = 0x20,
        /** Pre-emptilbel queue feature */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PREEMPTIBLE  = 0x40,
        /* Geneve OAM RX queue */
        VMKNETDDI_QUEUEOPS_QUEUE_FEAT_GENEVE_OAM   = 0x80,
} vmknetddi_queueops_queue_features_t;

typedef struct vmknetddi_queueops_queueattr
{
   enum {
      VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR,           /* Priority attribute */
      VMKNETDDI_QUEUEOPS_QUEUE_ATTR_FEAT,            /* Features attribute */
      VMKNETDDI_QUEUEOPS_QUEUE_ATTR_NUM,             /* Number of attributes */
   } type;

   union {
      vmknetddi_queueops_tx_priority_t priority;     /* QUEUE_ATTR_PRIOR argument */
      vmknetddi_queueops_queue_features_t features;  /* QUEUE_ATTR_FEAT argument */
      void *p;                                       /* Generic attribute argument */
   } args;
} vmknetddi_queueops_queueattr_t;

#define VMKNETDDI_QUEUEOPS_INVALID_QUEUEID ((vmknetddi_queueops_queueid_t)0)
#define VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(n) ((vmknetddi_queueops_queueid_t) \
				((VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX<<16) | \
				 (n & 0xffff)))
#define VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(n) ((vmknetddi_queueops_queueid_t) \
				((VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX<<16) | \
				 (n & 0xffff)))

#define VMKNETDDI_QUEUEOPS_QUEUEID_VAL(cid) ((u16)((cid) & 0xffff))

#define VMKNETDDI_QUEUEOPS_MK_FILTERID(n) \
				((vmknetddi_queueops_filterid_t)(n))
#define VMKNETDDI_QUEUEOPS_FILTERID_VAL(fid)  ((u16)(fid))

#define VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(n) (((n) & 0x00ff0000) == \
				(VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX << 16))

#define VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(n) (((n) & 0x00ff0000) == \
				(VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX << 16))

#define VMKNETDDI_QUEUEOPS_TX_QUEUEID_SET_QIDX(qid, idx) (qid | (((idx) & 0xffff) << 24))
#define VMKNETDDI_QUEUEOPS_TX_QUEUEID_GET_QIDX(qid)      ((qid & 0xffff000000) >> 24)

typedef struct vmknetddi_queueops_vxlan_filter {
	u8 inner_macaddr[ETH_ALEN];   /* inner MAC address */
	u8 outer_macaddr[ETH_ALEN];   /* outer MAC address */
	u32 vxlan_id;
} vmknetddi_queueops_vxlan_filter_t;

typedef struct vmknetddi_queueops_geneve_filter {
	u8 inner_macaddr[ETH_ALEN];   /* inner MAC address */
	u8 outer_macaddr[ETH_ALEN];   /* outer MAC address */
	u32 vni;
} vmknetddi_queueops_geneve_filter_t;

typedef struct vmknetddi_queueops_filter {
	vmknetddi_queueops_filter_class_t class; /* Filter class */
	u8 active;                                /* Is active? */
	u8 pad1;

	union {
		u8 macaddr[ETH_ALEN];             /* MAC address */
		u16 vlan_id;                      /* VLAN Id */
		struct {
			u8 macaddr[ETH_ALEN];     /* MAC address */
			u16 vlan_id;              /* VLAN id */
		} vlanmac;
		vmknetddi_queueops_vxlan_filter_t *vxlan_filter;   /* VXLAN filter */
		vmknetddi_queueops_geneve_filter_t *geneve_filter; /* Geneve filter */
	} u;

	u8 pad2[2];
} __attribute__ ((packed)) vmknetddi_queueops_filter_t;

typedef struct vmknetddi_queueops_filter_9_2_1_x {
	vmknetddi_queueops_filter_class_t class; /* Filter class */
	u8 active;                                /* Is active? */

	union {
		u8 macaddr[ETH_ALEN];             /* MAC address */
		u16 vlan_id;                      /* VLAN Id */
		struct {
			u8 macaddr[ETH_ALEN];     /* MAC address */
			u16 vlan_id;              /* VLAN id */
		} vlanmac;
	} u;
} vmknetddi_queueops_filter_t_9_2_1_x;

/* WARNING: Order below should not be changed  */
typedef enum _vmknetddi_queueops_op {
	/* Get supported netqueue api version */	
	VMKNETDDI_QUEUEOPS_OP_GET_VERSION           = 0x1,
	/* Get features supported by implementation */
	VMKNETDDI_QUEUEOPS_OP_GET_FEATURES          = 0x2, 
	/* Get supported (tx or rx) queue count */
	VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT       = 0x3, 
	/* Get supported (tx or rx) filters count */
	VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT      = 0x4,
	/* Allocate a queue */
	VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE           = 0x5,
	/* Free allocated queue */
	VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE            = 0x6,
	/* Get vector assigned to the queue */
	VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR      = 0x7,
	/* Get default queue */
	VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE     = 0x8,
	/* Apply rx filter on a queue */
	VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER       = 0x9,
	/* Remove appled rx filter */
	VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER      = 0xa,
	/* Get queue stats */
	VMKNETDDI_QUEUEOPS_OP_GET_STATS             = 0xb,
   /* Set tx queue priority */
   VMKNETDDI_QUEUEOPS_OP_SET_TX_PRIORITY       = 0xc,
   /* Allocate a queue with attributes */
   VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR = 0xd,
   /* Enable queue's features */
   VMKNETDDI_QUEUEOPS_OP_ENABLE_FEAT           = 0xe,
   /* Disable queue's features */
   VMKNETDDI_QUEUEOPS_OP_DISABLE_FEAT          = 0xf,
   /* Get supported queue's features */
   VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT    = 0x10,
   /* Get supported queue's filter class */
   VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FILTER_CLASS = 0x11,
   /* Re-Allocate a queue with attributes (free + alloc) */
   VMKNETDDI_QUEUEOPS_OP_REALLOC_QUEUE_WITH_ATTR    = 0x12,
   /* Perform an RSS specific operation */
   VMKNETDDI_QUEUEOPS_OP_CONFIG_RSS            = 0x14,
   /* Get supported rx filters count of deive */
   VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT_OF_DEVICE = 0x15,
} vmknetddi_queueops_op_t;

typedef enum _vmknetddi_queue_rss_op_t {
   /* Get the devices RSS params */
   VMKNETDDI_QUEUEOPS_RSS_OP_GET_PARAMS        = 0x1,
   /* Initialize the RSS state (key + table) */
   VMKNETDDI_QUEUEOPS_RSS_OP_INIT_STATE        = 0x2,
   /* Update RSS redirection table */
   VMKNETDDI_QUEUEOPS_RSS_OP_UPDATE_IND_TABLE  = 0x3,
   /* Get RSS redirection table */
   VMKNETDDI_QUEUEOPS_RSS_OP_GET_IND_TABLE     = 0x4,
} vmknetddi_queue_rss_op_t;

typedef struct _vmknetddi_queueop_get_version_args_t {
	u16                     major;       /* OUT: Major version */
	u16                     minor;       /* OUT: Minor version */
} vmknetddi_queueop_get_version_args_t ;

typedef struct _vmknetddi_queueop_get_features_args_t {
	struct net_device		*netdev;   /* IN:  Netdev */
	vmknetddi_queueops_features_t	features;  /* OUT: Features supported */
} vmknetddi_queueop_get_features_args_t ;

typedef struct _vmknetddi_queueop_get_queue_count_args_t {
	struct net_device		*netdev; /* IN:  Netdev */
	vmknetddi_queueops_queue_t	type;    /* IN:  Type of queue */
	u16				count;   /* OUT: Queues supported */
} vmknetddi_queueop_get_queue_count_args_t ;

typedef struct _vmknetddi_queueop_get_filter_count_args_t {
	struct net_device		*netdev; /* IN:  Netdev */
	vmknetddi_queueops_queue_t	type;    /* IN:  Type of queue */
	u16				count;   /* OUT: Filters supported */
} vmknetddi_queueop_get_filter_count_args_t ;

typedef struct _vmknetddi_queueop_get_filter_count_of_device_args_t {
	struct net_device		*netdev; /* IN:  Netdev */
	vmknetddi_queueops_queue_t	type;    /* IN:  Type of queue */
	u16				filters_of_device_count;   /* OUT: Filters supported per device */
	u16				filters_per_queue_count;   /* OUT: Filters supported per queue */
} vmknetddi_queueop_get_filter_count_of_device_args_t ;

typedef struct _vmknetddi_queueop_alloc_queue_args_t {
	struct net_device       	*netdev;  /* IN:  Netdev */
	vmknetddi_queueops_queue_t      type;     /* IN:  Type of queue */
        struct napi_struct              *napi;    /* OUT: Napi struct for this queue */
        u16                             queue_mapping; /* OUT: Linux tx queue mapping */
	vmknetddi_queueops_queueid_t    queueid;  /* OUT: New queue id */
} vmknetddi_queueop_alloc_queue_args_t ;

typedef struct _vmknetddi_queueop_free_queue_args_t {
	struct net_device       	*netdev; /* IN:  Netdev */
	vmknetddi_queueops_queueid_t    queueid; /* IN: Queue id */
} vmknetddi_queueop_free_queue_args_t ;

typedef struct _vmknetddi_queueop_get_queue_vector_args_t {
	struct net_device       	*netdev; /* IN:  Netdev */
	vmknetddi_queueops_queueid_t	queueid; /* IN: Queue id */
	u16                     	vector;  /* OUT: Assigned interrupt vector */
} vmknetddi_queueop_get_queue_vector_args_t ;

typedef struct _vmknetddi_queueop_get_default_queue_args_t {
	struct net_device       	*netdev;  /* IN:  Netdev */
	vmknetddi_queueops_queue_t	type;     /* IN:  Type of queue */
        struct napi_struct              *napi;    /* OUT: Napi struct for this queue */
        u16                             queue_mapping; /* OUT: Linux tx queue mapping */
	vmknetddi_queueops_queueid_t	queueid;  /* OUT: Default queue id */
} vmknetddi_queueop_get_default_queue_args_t ;

typedef struct _vmknetddi_queueop_apply_rx_filter_args_t {
	struct net_device       	*netdev;   /* IN: Netdev */
	vmknetddi_queueops_queueid_t	queueid;   /* IN: Queue id */
	vmknetddi_queueops_filter_t	filter;    /* IN: Filter */
	vmknetddi_queueops_filterid_t	filterid;  /* OUT: Filter id */
	u16                             pairtxqid;  /* OUT: Paired TX Queue id */
} vmknetddi_queueop_apply_rx_filter_args_t ;

typedef struct _vmknetddi_queueop_remove_rx_filter_args_t {
	struct net_device       	*netdev;   /* IN: Netdev */
	vmknetddi_queueops_queueid_t	queueid;   /* IN: Queue id */
	vmknetddi_queueops_filterid_t	filterid;  /* IN: Filter id */
} vmknetddi_queueop_remove_rx_filter_args_t ;

typedef struct _vmknetddi_queueop_get_stats_args_t {
	struct net_device        	*netdev;   /* IN: Netdev */
	vmknetddi_queueops_queueid_t    queueid;   /* IN: Queue id */
	struct net_device_stats  	*stats;    /* OUT: Queue statistics */
} vmknetddi_queueop_get_stats_args_t ;

typedef struct _vmknetddi_queueop_set_tx_priority_args_t {
        struct net_device               *netdev;   /* IN: Netdev */
	vmknetddi_queueops_queueid_t     queueid;  /* IN: Queue id */
	vmknetddi_queueops_tx_priority_t priority; /* IN: Queue priority */
} vmknetddi_queueop_set_tx_priority_args_t;

typedef struct _vmknetddi_queueop_alloc_queue_with_attr_args_t {
	struct net_device       	*netdev;  /* IN:  Netdev */
	vmknetddi_queueops_queue_t      type;     /* IN:  Type of queue */
        u16                             nattr;    /* IN:  Number of attributes */
        vmknetddi_queueops_queueattr_t  *attr;    /* IN:  Queue attributes */
        struct napi_struct              *napi;    /* OUT: Napi struct for this queue */
        u16                             queue_mapping; /* OUT: Linux tx queue mapping */
	vmknetddi_queueops_queueid_t    queueid;  /* OUT: New queue id */
} vmknetddi_queueop_alloc_queue_with_attr_args_t ;

typedef struct _vmknetddi_queueop_enable_feat_args_t {
        struct net_device                  *netdev;   /* IN: Netdev */
	vmknetddi_queueops_queueid_t        queueid;  /* IN: Queue id */
        vmknetddi_queueops_queue_features_t features; /* IN: Features */
} vmknetddi_queueop_enable_feat_args_t;

typedef struct _vmknetddi_queueop_disable_feat_args_t {
        struct net_device                   *netdev;  /* IN: Netdev */
	vmknetddi_queueops_queueid_t        queueid;  /* IN: Queue id */
        vmknetddi_queueops_queue_features_t features; /* IN: Features */
} vmknetddi_queueop_disable_feat_args_t;

typedef struct _vmknetddi_queueop_get_sup_feat_args_t {
        struct net_device                   *netdev;  /* IN: Netdev */
	vmknetddi_queueops_queue_t     	    type;     /* IN: Type of queue */        
        vmknetddi_queueops_queue_features_t features; /* OUT: Features */
} vmknetddi_queueop_get_sup_feat_args_t;

typedef struct _vmknetddi_queueop_get_sup_filter_class_args_t {
        struct net_device                   *netdev;  /* IN: Netdev */
	vmknetddi_queueops_queue_t     	    type;     /* IN: Type of queue */        
        vmknetddi_queueops_filter_class_t   class;    /* OUT: Class */
} vmknetddi_queueop_get_sup_filter_class_args_t;

/* Type for RSS indirection table */
typedef struct _vmknetddi_queue_rssop_ind_table_t {
   u16 table_size;
   u8  table[0];
} vmknetddi_queue_rssop_ind_table_t;

/* Type for RSS hash key */
typedef struct _vmknetddi_queue_rssop_hash_key_t {
   u16 key_size;
   u8  key[0];
} vmknetddi_queue_rssop_hash_key_t;

/* Argument for setting/getting RSS indirection table */
typedef struct _vmknetddi_queue_rssop_ind_table_args_t {
   struct net_device *netdev;                       // IN
   vmknetddi_queue_rssop_ind_table_t *rss_ind_table; // IN (Set), OUT (Get)
} vmknetddi_queue_rssop_ind_table_args_t;

/* Arguments for getting RSS params */
typedef struct _vmknetddi_queue_rssop_get_params_args_t {
   struct net_device *netdev;
   u16 num_rss_pools;
   u16 num_rss_queues_per_pool;
   u16 rss_hash_key_size;
   u16 rss_ind_table_size;
} vmknetddi_queue_rssop_get_params_args_t;

/* Argument for init RSS state operation */
typedef struct _vmknetddi_queue_rssop_init_state_args_t {
   struct net_device *netdev;                     // IN
   vmknetddi_queue_rssop_hash_key_t *rss_key;        // IN
   vmknetddi_queue_rssop_ind_table_t *rss_ind_table; // IN
} vmknetddi_queue_rssop_init_state_args_t;

typedef struct _vmknetddi_queueop_realloc_queue_with_attr_args_t {
	vmknetddi_queueop_alloc_queue_with_attr_args_t *alloc_args;  /* IN:  Netdev */
	u16				   rm_filter_count;   /* IN: Count of filters to be removed */
	vmknetddi_queueop_remove_rx_filter_args_t *rm_filters_args;  /* IN:  list of rm_filter args */
        vmknetddi_queueop_apply_rx_filter_args_t  *apply_rx_filter_args; /* IN: apply_rx_filter args */ 
} vmknetddi_queueop_realloc_queue_with_attr_args_t ;

/* Arguments for config RSS netqueue op */
typedef struct _vmknetddi_queueop_config_rss_args_t {
   vmknetddi_queue_rss_op_t   op_type;
   void                      *op_args;
} vmknetddi_queueop_config_rss_args_t;

typedef int (*vmknetddi_queueops_f)(vmknetddi_queueops_op_t op, 
				     void *args);

static inline int
vmknetddi_queueops_version(vmknetddi_queueop_get_version_args_t *args)
{
	args->major = VMKNETDDI_QUEUEOPS_MAJOR_VER;
	args->minor = VMKNETDDI_QUEUEOPS_MINOR_VER;
	return 0;
}

static inline void
vmknetddi_queueops_set_filter_active(vmknetddi_queueops_filter_t *f)
{
	f->active = 1;
}

static inline void
vmknetddi_queueops_set_filter_inactive(vmknetddi_queueops_filter_t *f)
{
	f->active = 0;
}

static inline int
vmknetddi_queueops_is_filter_active(vmknetddi_queueops_filter_t *f)
{
	return f->active;
}

/**                                          
 *  vmknetddi_queueops_filter_class - get filter class 
 *  @f: a given vmknetddi_queueops_filter_t    
 *                                           
 *  Get the class of a given filter. 
 *                                           
 *  RETURN VALUE:                     
 *  0x0 (Invalid filter class)
 *  0x1 (Mac address filter class)
 *  0x2 (Vlan tag filter class)
 *  0x3 (Vlan tag + Mac address filter class) 
 *                                           
 */
/* _VMKLNX_CODECHECK_: vmknetddi_queueops_get_filter_class */
static inline vmknetddi_queueops_filter_class_t
vmknetddi_queueops_get_filter_class(vmknetddi_queueops_filter_t *f)
{
	return f->class;
}

/**
 *  vmknetddi_queueops_set_filter_macaddr - set class and MAC address for the filter
 *  @f: a given vmknetddi_queueops_filter_t
 *  @macaddr: a given MAC address
 *
 *  Set the filter class to VMKNETDDI_QUEUEOPS_FILTER_MACADDR and
 *  the MAC address of the filter.
 *
 *  RETURN VALUE:
 *  This function does not return a value.
 *
 */
/* _VMKLNX_CODECHECK_: vmknetddi_queueops_set_filter_macaddr */
static inline void
vmknetddi_queueops_set_filter_macaddr(vmknetddi_queueops_filter_t *f,
			      	       u8 *macaddr)
{
	f->class = VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
	memcpy(f->u.macaddr, macaddr, ETH_ALEN);
}

/**                                          
 *  vmknetddi_queueops_get_filter_macaddr - get the MAC address for a given filter 
 *  @f: a pointer to the given filter
 *                                           
 *  Get the Mac address of the filter if the filter class is Mac filter or
 *  Vlan tag + Mac filter. 
 *                                           
 *  RETURN VALUE:                     
 *  the pointer to the Mac address if the filter class is Mac filter or
 *                     Vlan tag + Mac address filter
 *  NULL otherwise
 *                                           
 */
/* _VMKLNX_CODECHECK_: vmknetddi_queueops_get_filter_macaddr */
static inline u8 *
vmknetddi_queueops_get_filter_macaddr(vmknetddi_queueops_filter_t *f)
{
	if (f->class == VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
		return f->u.macaddr;
	}
	else if (f->class == VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR) {
		return f->u.vlanmac.macaddr;
	}
	else {
		return NULL;
	}
}

static inline void
vmknetddi_queueops_set_filter_vlan(vmknetddi_queueops_filter_t *f,
			   	    u16 vlanid)
{
	f->class = VMKNETDDI_QUEUEOPS_FILTER_VLAN;
	f->u.vlan_id = vlanid;
}

static inline void
vmknetddi_queueops_set_filter_vlanmacaddr(vmknetddi_queueops_filter_t *f,
					   u8 *macaddr,
					   u16 vlanid)
{
	f->class = VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR;
	memcpy(f->u.vlanmac.macaddr, macaddr, ETH_ALEN);
	f->u.vlanmac.vlan_id = vlanid;
}

static inline void
vmknetddi_queueops_set_filter_vxlan(vmknetddi_queueops_filter_t *f,
                                    u8 *imacaddr, u8 *omacaddr, u16 vxlanid)
{
   f->class = VMKNETDDI_QUEUEOPS_FILTER_VXLAN;
   memcpy(f->u.vxlan_filter->inner_macaddr, imacaddr, ETH_ALEN);
   memcpy(f->u.vxlan_filter->outer_macaddr, omacaddr, ETH_ALEN);
   f->u.vxlan_filter->vxlan_id = vxlanid;
}

static inline void
vmknetddi_queueops_set_filter_geneve(vmknetddi_queueops_filter_t *f,
                                     u8 *imacaddr, u8 *omacaddr, u16 vni)
{
   f->class = VMKNETDDI_QUEUEOPS_FILTER_GENEVE;
   memcpy(f->u.geneve_filter->inner_macaddr, imacaddr, ETH_ALEN);
   memcpy(f->u.geneve_filter->outer_macaddr, omacaddr, ETH_ALEN);
   f->u.geneve_filter->vni = vni;
}

/**
 *  vmknetddi_queueops_get_filter_vlanid - <1 Line Description>
 *  @f: vmknetddi queue operation filter
 *
 *  Return the vlanid associated with the vmknetddi queue operation filter.
 *
 *  RETURN VALUE:
 *  Returns vlanid if the filter class its either a vlan tag filter or a 
 *  vlan tag and mac address filter. 0 otherwise.
 *
 */
 /* _VMKLNX_CODECHECK_: vmknetddi_queueops_get_filter_vlanid */
static inline u16 
vmknetddi_queueops_get_filter_vlanid(vmknetddi_queueops_filter_t *f)
{
	if (f->class == VMKNETDDI_QUEUEOPS_FILTER_VLAN) {
		return f->u.vlan_id;
	}
	else if (f->class == VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR) {
		return f->u.vlanmac.vlan_id;
	}
	else {
		return 0;
	}
}

#endif	/* _LINUX_NETQUEUE_H */
#endif	/* __VMKLNX__ */
