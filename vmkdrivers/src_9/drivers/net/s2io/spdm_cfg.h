/* A SPDM config structure to pass info the driver by user */
struct spdm_cfg {
	unsigned int	sip; /* Src IP addr */
	unsigned int	dip; /* Dst IP addr */
	unsigned short	sprt; /* Src TCP port */
	unsigned short	dprt; /* Dst TCP port */
	unsigned int	t_queue; /*Target Rx Queue for the packet */
	unsigned int	hash; /* the hash as per jenkin's hash algorithm. */
#define SPDM_NO_DATA			0x1
#define SPDM_XENA_IF			0x2
#define SPDM_HW_UNINITIALIZED		0x3
#define SPDM_INCOMPLETE_SOCKET		0x4
#define SPDM_TABLE_ACCESS_FAILED	0x5
#define SPDM_TABLE_FULL			0x6
#define SPDM_TABLE_UNKNOWN_BAR		0x7
#define SPDM_TABLE_MALLOC_FAIL		0x8
#define	SPDM_INVALID_DEVICE		0x9
#define SPDM_CONF_SUCCESS		0x0
#define SPDM_CLR_SUCCESS		0xA
#define SPDM_CLR_FAIL			0xB
#define	SPDM_GET_CFG_DATA		0xAA55
#define	SPDM_GET_CLR_DATA		0xAAA555
	int		ret;
#define MAX_SPDM_ENTRIES_SIZE	(0x100 * 0x40)
	unsigned char	data[MAX_SPDM_ENTRIES_SIZE];
	int		data_len; /* Number of entries retrieved */
	char		dev_name[20];	/* Device name, e.g. eth0, eth1... */
};

