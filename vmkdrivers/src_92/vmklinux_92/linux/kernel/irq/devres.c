#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/gfp.h>

#if defined(__VMKLNX__)
#include <linux/pci.h>
#include "vmkapi.h"
#include "linux_irq.h"
#include "linux_stubs.h"
#include "vmklinux_log.h"
#endif /* defined(__VMKLNX__) */

/*
 * Device resource management aware IRQ request/free implementation.
 */
struct irq_devres {
	unsigned int irq;
	void *dev_id;
#if defined(__VMKLNX__)
	irq_handler_t handler;
	vmk_ModuleID moduleID;
#endif /* defined(__VMKLNX__) */
};

#if defined(__VMKLNX__)
void devm_irq_release(struct device *dev, void *res)
#else /* !defined(__VMKLNX__) */
static void devm_irq_release(struct device *dev, void *res)
#endif /* defined(__VMKLNX__) */
{
	struct irq_devres *this = res;

	free_irq(this->irq, this->dev_id);
}

#if defined(__VMKLNX__)
int devm_irq_match(struct device *dev, void *res, void *data)
#else /* !defined(__VMKLNX__) */
static int devm_irq_match(struct device *dev, void *res, void *data)
#endif /* defined(__VMKLNX__) */
{
	struct irq_devres *this = res, *match = data;

	return this->irq == match->irq && this->dev_id == match->dev_id;
}

/**
 *	devm_request_irq - allocate an interrupt line for a managed device
 *	@dev: device to request interrupt for
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs
 *	@irqflags: Interrupt type flags
 *	@devname: An ascii name for the claiming device
 *	@dev_id: A cookie passed back to the handler function
 *
 *	Except for the extra @dev argument, this function takes the
 *	same arguments and performs the same function as
 *	request_irq().  IRQs requested with this function will be
 *	automatically freed on driver detach.
 *
 *	If an IRQ allocated with this function needs to be freed
 *	separately, dev_free_irq() must be used.
 *
 *	RETURN VALUE:
 *	0 for success, error code for failure.
 */
/* _VMKLNX_CODECHECK_: devm_request_irq */
int devm_request_irq(struct device *dev, unsigned int irq,
		     irq_handler_t handler, unsigned long irqflags,
		     const char *devname, void *dev_id)
{
	struct irq_devres *dr;
	int rc;

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	dr = devres_alloc(devm_irq_release, sizeof(struct irq_devres),
			  GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	rc = request_irq(irq, handler, irqflags, devname, dev_id);
	if (rc) {
		devres_free(dr);
		return rc;
	}

	dr->irq = irq;
	dr->dev_id = dev_id;
	devres_add(dev, dr);

	return 0;
}
EXPORT_SYMBOL(devm_request_irq);

/**
 *	devm_free_irq - free an interrupt
 *	@dev: device to free interrupt for
 *	@irq: Interrupt line to free
 *	@dev_id: Device identity to free
 *
 *	Except for the extra @dev argument, this function takes the
 *	same arguments and performs the same function as free_irq().
 *	This function instead of free_irq() should be used to manually
 *	free IRQs allocated with dev_request_irq().
 *
 *	RETURN VALUE:
 *	None.
 */
/* _VMKLNX_CODECHECK_: devm_free_irq */
void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id)
{
	struct irq_devres match_data = { irq, dev_id };

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	free_irq(irq, dev_id);
	WARN_ON(devres_destroy(dev, devm_irq_release, devm_irq_match,
			       &match_data));
}
EXPORT_SYMBOL(devm_free_irq);

#if defined(__VMKLNX__)
void *irqdevres_register(struct device *dev, unsigned int irq,
                         irq_handler_t handler, void *dev_id)
{
   struct irq_devres *idr;

   /*
    * create devres for this irq and register it
    */
   idr = devres_alloc(devm_irq_release, sizeof(struct irq_devres), GFP_KERNEL);

   if (!idr) {
      VMKLNX_WARN("No memory for irq resource data");
      VMK_ASSERT(0);
      return NULL;
   }

   idr->irq = irq;
   idr->dev_id = dev_id;
   devres_add(dev, idr);

   idr->handler = handler;
   idr->moduleID = vmk_ModuleStackTop();
   VMKLNX_DEBUG(2, "Registered - irq: %d dev_id: %p", irq, dev_id);

   return idr;
}

void irqdevres_unregister(struct device *dev, unsigned int irq, void *dev_id)
{
   struct irq_devres match_idr = { irq, dev_id };

   devres_destroy(dev, devm_irq_release, devm_irq_match, &match_idr);
}

void *irqdevres_find(struct device *dev, dr_match_t match, unsigned int irq, void *dev_id)
{
   struct irq_devres *idr, match_idr = { irq, dev_id };

   idr = devres_find(dev, devm_irq_release, match, &match_idr);

   return idr;
}

void *irqdevres_get_handler(void *data)
{
	struct irq_devres *idr = data;

	return idr->handler;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Linux_IRQHandler --
 *
 *      A generic interrupt handler that dispatch interrupt to driver.
 *      This handler is registered using vmk_AddInterruptHandler().
 *
 *-----------------------------------------------------------------------------
 */

void
Linux_IRQHandler(void *clientData, vmk_IntrCookie intr)
{
   struct irq_devres *idr = (struct irq_devres *)clientData;

   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);

   VMKLNX_ASSERT_BUG(48431, idr->handler);
   VMK_ASSERT(idr->moduleID != VMK_INVALID_MODULE_ID);

   VMKAPI_MODULE_CALL_VOID(idr->moduleID,
                           idr->handler,
                           idr->irq,
                           idr->dev_id);
}

#endif /* defined(__VMKLNX__) */

