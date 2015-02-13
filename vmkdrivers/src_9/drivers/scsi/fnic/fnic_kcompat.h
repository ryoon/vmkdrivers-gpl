#ifndef _FNIC_KCOMPAT_H_
#define _FNIC_KCOMPAT_H_

#include <scsi/scsi_host.h>

#if BITS_PER_LONG == 64

#define div_u64(divisor, dividend) \
        (divisor) / (dividend); \

#else

/*
 * This is not a generic macro. It is specific to a dividend of 10^6
 */

#define div_u64(divisor, dividend) \
        /* \
         * To convert to Megabytes, instead of dividing by 10^6, replace \
         * by multiplying by (2 ^32 / 10^6 which is approx. 4295) \
         * and then divide by 2^32. \
         * \
         * This avoids link break for 32 bit systems where the 64 bit \
         * divide is not defined and keeps division error low, about 0.00076% \
         */ \
        ((divisor) * 4295) >> 32; \

#endif

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

#ifndef DID_TRANSPORT_DISRUPTED
#define DID_TRANSPORT_DISRUPTED DID_BUS_BUSY
#endif

#ifndef fc_block_scsi_eh
#define fc_block_scsi_eh(sc) fnic_block_error_handler(sc)
#endif

void fc_block_scsi_eh(struct scsi_cmnd *);
void scsi_dma_unmap(struct scsi_cmnd *);
int scsi_dma_map(struct scsi_cmnd *);

#endif /* _FNIC_KCOMPAT_H_ */
