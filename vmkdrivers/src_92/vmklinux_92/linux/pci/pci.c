/*
 * Portions Copyright 2008-2010, 2012-2013 VMware, Inc.
 */
/*
 *	$Id: pci.c,v 1.91 1999/01/21 13:34:01 davem Exp $
 *
 *	PCI Bus Services, see include/linux/pci.h for further explanation.
 *
 *	Copyright 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 *	David Mosberger-Tang
 *
 *	Copyright 1997 -- 2000 Martin Mares <mj@ucw.cz>
 *
 *	From linux-2.6.28/drivers/pci/pci.c
 *
 *      Copyright 1993 -- 1997 Drew Eckhardt, Frederic Potter,
 *      David Mosberger-Tang
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#if !defined(__VMKLNX__)
#include <asm/dma.h>	/* isa_dma_bridge_buggy */
#endif /* !defined(__VMKLNX__) */
#include "pci.h"

#if defined(__VMKLNX__)
#include "linux_pci.h"
#include "vmkapi.h"
#include "linux/pci_ids.h"	/* PCI_CLASS_* */
#include "linux/netdevice.h"	/* vmk_net_* interfaces */
#include "linux_task.h"
#include "linux_net.h"
#include "vmklinux_log.h"
#endif /* defined(__VMKLNX__) */

unsigned int pci_pm_d3_delay = 10;

#if !defined(__VMKLNX__)
/**
 * pci_bus_max_busnr - returns maximum PCI bus number of given bus' children
 * @bus: pointer to PCI bus structure to search
 *
 * Given a PCI bus, returns the highest PCI bus number present in the set
 * including the given PCI bus and its list of child PCI buses.
 */
unsigned char __devinit
pci_bus_max_busnr(struct pci_bus* bus)
{
	struct list_head *tmp;
	unsigned char max, n;

	max = bus->subordinate;
	list_for_each(tmp, &bus->children) {
		n = pci_bus_max_busnr(pci_bus_b(tmp));
		if(n > max)
			max = n;
	}
	return max;
}
EXPORT_SYMBOL_GPL(pci_bus_max_busnr);

/**
 * pci_max_busnr - returns maximum PCI bus number
 *
 * Returns the highest PCI bus number present in the system global list of
 * PCI buses.
 */
unsigned char __devinit
pci_max_busnr(void)
{
	struct pci_bus *bus = NULL;
	unsigned char max, n;

	max = 0;
	while ((bus = pci_find_next_bus(bus)) != NULL) {
		n = pci_bus_max_busnr(bus);
		if(n > max)
			max = n;
	}
	return max;
}

static int __pci_find_next_cap(struct pci_bus *bus, unsigned int devfn, u8 pos, int cap)
{
	u8 id;
	int ttl = 48;

	while (ttl--) {
		pci_bus_read_config_byte(bus, devfn, pos, &pos);
		if (pos < 0x40)
			break;
		pos &= ~3;
		pci_bus_read_config_byte(bus, devfn, pos + PCI_CAP_LIST_ID,
					 &id);
		if (id == 0xff)
			break;
		if (id == cap)
			return pos;
		pos += PCI_CAP_LIST_NEXT;
	}
	return 0;
}

int pci_find_next_capability(struct pci_dev *dev, u8 pos, int cap)
{
	return __pci_find_next_cap(dev->bus, dev->devfn,
				   pos + PCI_CAP_LIST_NEXT, cap);
}
EXPORT_SYMBOL_GPL(pci_find_next_capability);

static int __pci_bus_find_cap(struct pci_bus *bus, unsigned int devfn, u8 hdr_type, int cap)
{
	u16 status;
	u8 pos;

	pci_bus_read_config_word(bus, devfn, PCI_STATUS, &status);
	if (!(status & PCI_STATUS_CAP_LIST))
		return 0;

	switch (hdr_type) {
	case PCI_HEADER_TYPE_NORMAL:
	case PCI_HEADER_TYPE_BRIDGE:
		pos = PCI_CAPABILITY_LIST;
		break;
	case PCI_HEADER_TYPE_CARDBUS:
		pos = PCI_CB_CAPABILITY_LIST;
		break;
	default:
		return 0;
	}
	return __pci_find_next_cap(bus, devfn, pos, cap);
}
#endif /* !defined(__VMKLNX__) */

/**
 * pci_find_capability - query for devices' capabilities 
 * @dev: PCI device to query
 * @cap: capability code
 *
 * Tell if a device supports a given PCI capability.
 * Possible values for @cap:
 *
 *  %PCI_CAP_ID_PM           Power Management 
 *  %PCI_CAP_ID_AGP          Accelerated Graphics Port 
 *  %PCI_CAP_ID_VPD          Vital Product Data 
 *  %PCI_CAP_ID_SLOTID       Slot Identification 
 *  %PCI_CAP_ID_MSI          Message Signalled Interrupts
 *  %PCI_CAP_ID_CHSWP        CompactPCI HotSwap 
 *  %PCI_CAP_ID_PCIX         PCI-X
 *  %PCI_CAP_ID_EXP          PCI Express
 *
 * Return Value:
 * The address of the requested capability structure within the
 * device's PCI configuration space.
 * 0 in case the device does not support it.
 */
/* _VMKLNX_CODECHECK_: pci_find_capability */
int pci_find_capability(struct pci_dev *dev, int cap)
{
#if defined(__VMKLNX__)
   return LinuxPCI_FindCapability(pci_domain_nr(dev->bus),
                                  dev->bus->number,
                                  dev->devfn, cap);
#else /* !defined(__VMKLNX__) */
	return __pci_bus_find_cap(dev->bus, dev->devfn, dev->hdr_type, cap);
#endif /* defined(__VMKLNX__) */
}

/**
 * pci_bus_find_capability - query for devices' capabilities 
 * @bus:   the PCI bus to query
 * @devfn: PCI device to query
 * @cap:   capability code
 *
 * Like pci_find_capability() but works for pci devices that do not have a
 * pci_dev structure set up yet. 
 *
 * Returns the address of the requested capability structure within the
 * device's PCI configuration space or 0 in case the device does not
 * support it.
 */
int pci_bus_find_capability(struct pci_bus *bus, unsigned int devfn, int cap)
{
#if defined(__VMKLNX__)
   return LinuxPCI_FindCapability(pci_domain_nr(bus), bus->number, devfn, cap);
#else /* !defined(__VMKLNX__) */
	u8 hdr_type;

	pci_bus_read_config_byte(bus, devfn, PCI_HEADER_TYPE, &hdr_type);

	return __pci_bus_find_cap(bus, devfn, hdr_type & 0x7f, cap);
#endif /* defined(__VMKLNX__) */
}
/**
 * pci_find_ext_capability - Find an extended capability
 * @dev: PCI device to query
 * @cap: capability code
 *
 * Returns the address of the requested extended capability structure
 * within the device's PCI configuration space or 0 if the device does
 * not support it.  Possible values for @cap:
 *
 *  %PCI_EXT_CAP_ID_ERR		Advanced Error Reporting
 *  %PCI_EXT_CAP_ID_VC		Virtual Channel
 *  %PCI_EXT_CAP_ID_DSN		Device Serial Number
 *  %PCI_EXT_CAP_ID_PWR		Power Budgeting
 */
#if !defined(__VMKLNX__)
int pci_find_ext_capability(struct pci_dev *dev, int cap)
{
	u32 header;
	int ttl = 480; /* 3840 bytes, minimum 8 bytes per capability */
	int pos = 0x100;

	if (dev->cfg_size <= 256)
		return 0;

	if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL)
		return 0;

	/*
	 * If we have no capabilities, this is indicated by cap ID,
	 * cap version and next pointer all being 0.
	 */
	if (header == 0)
		return 0;

	while (ttl-- > 0) {
		if (PCI_EXT_CAP_ID(header) == cap)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (pos < 0x100)
			break;

		if (pci_read_config_dword(dev, pos, &header) != PCIBIOS_SUCCESSFUL)
			break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pci_find_ext_capability);

/**
 * pci_find_parent_resource - return resource region of parent bus of given region
 * @dev: PCI device structure contains resources to be searched
 * @res: child resource record for which parent is sought
 *
 *  For given resource region of given device, return the resource
 *  region of parent bus the given region is contained in or where
 *  it should be allocated from.
 */
struct resource *
pci_find_parent_resource(const struct pci_dev *dev, struct resource *res)
{
	const struct pci_bus *bus = dev->bus;
	int i;
	struct resource *best = NULL;

	for(i = 0; i < PCI_BUS_NUM_RESOURCES; i++) {
		struct resource *r = bus->resource[i];
		if (!r)
			continue;
		if (res->start && !(res->start >= r->start && res->end <= r->end))
			continue;	/* Not contained */
		if ((res->flags ^ r->flags) & (IORESOURCE_IO | IORESOURCE_MEM))
			continue;	/* Wrong type */
		if (!((res->flags ^ r->flags) & IORESOURCE_PREFETCH))
			return r;	/* Exact match */
		if ((res->flags & IORESOURCE_PREFETCH) && !(r->flags & IORESOURCE_PREFETCH))
			best = r;	/* Approximating prefetchable by non-prefetchable */
	}
	return best;
}

/**
 * pci_restore_bars - restore a devices BAR values (e.g. after wake-up)
 * @dev: PCI device to have its BARs restored
 *
 * Restore the BAR values for a given device, so as to make it
 * accessible by its driver.
 */
void
pci_restore_bars(struct pci_dev *dev)
{
	int i, numres;

	switch (dev->hdr_type) {
	case PCI_HEADER_TYPE_NORMAL:
		numres = 6;
		break;
	case PCI_HEADER_TYPE_BRIDGE:
		numres = 2;
		break;
	case PCI_HEADER_TYPE_CARDBUS:
		numres = 1;
		break;
	default:
		/* Should never get here, but just in case... */
		return;
	}

	for (i = 0; i < numres; i ++)
		pci_update_resource(dev, &dev->resource[i], i);
}

int (*platform_pci_set_power_state)(struct pci_dev *dev, pci_power_t t);
#endif /* !defined(__VMKLNX__) */

/**
 * pci_set_power_state - Set the power state of a PCI device
 * @dev: PCI device to be suspended
 * @state: PCI power state (D0, D1, D2, D3hot, D3cold) we're entering
 *
 * Transition a device to a new power state, using the Power Management 
 * Capabilities in the device's config space.
 *
 * RETURN VALUE: 
 * -EINVAL if trying to enter a lower state than we're already in.
 * 0 if we're already in the requested state.
 * -EIO if device does not support PCI PM.
 * 0 if we can successfully change the power state.
 */
/* _VMKLNX_CODECHECK_: pci_set_power_state */
int
pci_set_power_state(struct pci_dev *dev, pci_power_t state)
{
	int pm, need_restore = 0;
	u16 pmcsr, pmc;

	/* bound the state we're entering */
	if (state > PCI_D3hot)
		state = PCI_D3hot;

	/* Validate current state:
	 * Can enter D0 from any state, but if we can only go deeper 
	 * to sleep if we're already in a low power state
	 */
	if (state != PCI_D0 && dev->current_state > state) {
		printk(KERN_ERR "%s(): %s: state=%d, current state=%d\n",
			__FUNCTION__, pci_name(dev), state, dev->current_state);
		return -EINVAL;
	} else if (dev->current_state == state)
		return 0;        /* we're already there */

	/*
	 * If the device or the parent bridge can't support PCI PM, ignore
	 * the request if we're doing anything besides putting it into D0
	 * (which would only happen on boot).
	 */
	if ((state == PCI_D1 || state == PCI_D2) && pci_no_d1d2(dev))
		return 0;

	/* find PCI PM capability in list */
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);
	
	/* abort if the device doesn't support PM capabilities */
	if (!pm)
		return -EIO; 

	pci_read_config_word(dev,pm + PCI_PM_PMC,&pmc);
	if ((pmc & PCI_PM_CAP_VER_MASK) > 3) {
		printk(KERN_DEBUG
		       "PCI: %s has unsupported PM cap regs version (%u)\n",
		       pci_name(dev), pmc & PCI_PM_CAP_VER_MASK);
		return -EIO;
	}

	/* check if this device supports the desired state */
	if (state == PCI_D1 && !(pmc & PCI_PM_CAP_D1))
		return -EIO;
	else if (state == PCI_D2 && !(pmc & PCI_PM_CAP_D2))
		return -EIO;

	pci_read_config_word(dev, pm + PCI_PM_CTRL, &pmcsr);

	/* If we're (effectively) in D3, force entire word to 0.
	 * This doesn't affect PME_Status, disables PME_En, and
	 * sets PowerState to 0.
	 */
	switch (dev->current_state) {
	case PCI_D0:
	case PCI_D1:
	case PCI_D2:
		pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
		pmcsr |= state;
		break;
	case PCI_UNKNOWN: /* Boot-up */
		if ((pmcsr & PCI_PM_CTRL_STATE_MASK) == PCI_D3hot
		 && !(pmcsr & PCI_PM_CTRL_NO_SOFT_RESET))
			need_restore = 1;
		/* Fall-through: force to D0 */
	default:
		pmcsr = 0;
		break;
	}

	/* enter specified state */
	pci_write_config_word(dev, pm + PCI_PM_CTRL, pmcsr);

	/* Mandatory power management transition delays */
	/* see PCI PM 1.1 5.6.1 table 18 */
	if (state == PCI_D3hot || dev->current_state == PCI_D3hot)
		msleep(pci_pm_d3_delay);
	else if (state == PCI_D2 || dev->current_state == PCI_D2)
		udelay(200);

	/*
	 * Give firmware a chance to be called, such as ACPI _PRx, _PSx
	 * Firmware method after native method ?
	 */
   /* XXX */
#if !defined(__VMKLNX__)
	if (platform_pci_set_power_state)
		platform_pci_set_power_state(dev, state);
#endif /* !defined(__VMKLNX__) */
	dev->current_state = state;

	/* According to section 5.4.1 of the "PCI BUS POWER MANAGEMENT
	 * INTERFACE SPECIFICATION, REV. 1.2", a device transitioning
	 * from D3hot to D0 _may_ perform an internal reset, thereby
	 * going to "D0 Uninitialized" rather than "D0 Initialized".
	 * For example, at least some versions of the 3c905B and the
	 * 3c556B exhibit this behaviour.
	 *
	 * At least some laptop BIOSen (e.g. the Thinkpad T21) leave
	 * devices in a D3hot state at boot.  Consequently, we need to
	 * restore at least the BARs so that the device will be
	 * accessible to its driver.
	 */
#if !defined(__VMKLNX__)
	if (need_restore)
		pci_restore_bars(dev);
#endif /* !defined(__VMKLNX__) */

	return 0;
}

#if !defined(__VMKLNX__)
int (*platform_pci_choose_state)(struct pci_dev *dev, pm_message_t state);
#endif /* !defined(__VMKLNX__) */
 
/**
 *  pci_choose_state - Choose the power state of a PCI device
 *  @dev: PCI device to be suspended
 *  @state: target sleep state for the whole system. This is the value
 *  that is passed to suspend() function.
 *
 *  Choose the power state of a PCI device
 *
 *  RETURN VALUE:
 *  Returns PCI power state suitable for given device and given system
 *  message.
 */
/* _VMKLNX_CODECHECK_: pci_choose_state */
pci_power_t pci_choose_state(struct pci_dev *dev, pm_message_t state)
{
#if !defined(__VMKLNX__)
	int ret;
#else
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif /* !defined(__VMKLNX__) */

	if (!pci_find_capability(dev, PCI_CAP_ID_PM))
		return PCI_D0;

#if !defined(__VMKLNX__)
	if (platform_pci_choose_state) {
		ret = platform_pci_choose_state(dev, state);
		if (ret >= 0)
			state.event = ret;
	}
#endif /* !defined(__VMKLNX__) */

	switch (state.event) {
	case PM_EVENT_ON:
		return PCI_D0;
	case PM_EVENT_FREEZE:
	case PM_EVENT_SUSPEND:
		return PCI_D3hot;
	default:
		printk("They asked me for state %d\n", state.event);
		BUG();
	}
	return PCI_D0;
}

EXPORT_SYMBOL(pci_choose_state);
/**
 * pci_save_state - save the PCI configuration space of a device before suspending
 * @dev: - PCI device that we're dealing with
 *
 * Saves the PCI configuration space of a device before suspending.
 *
 * ESX Deviation Notes:
 * MSI and MSI-X states are not saved
 *
 * RETURN VALUE:
 * Returns 0 on success.
 */
/* _VMKLNX_CODECHECK_: pci_save_state */
int
pci_save_state(struct pci_dev *dev)
{
	int i;
	/* XXX: 100% dword access ok here? */
	for (i = 0; i < 16; i++)
		pci_read_config_dword(dev, i * 4,&dev->saved_config_space[i]);

#if !defined(__VMKLNX__)
	if ((i = pci_save_msi_state(dev)) != 0)
		return i;
	if ((i = pci_save_msix_state(dev)) != 0)
		return i;
#endif /* !defined(__VMKLNX__) */
	return 0;
}

/** 
 * pci_restore_state - Restore the saved state of a PCI device
 * @dev: - PCI device that we're dealing with
 *
 * Restores the saved state of a PCI device from its PCI configuration space.
 *
 * ESX Deviation Notes: 
 * MSI and MSI-X states might not be fully restored after this call.
 *
 * RETURN VALUE:
 * Returns 0 on success.
 */
/* _VMKLNX_CODECHECK_: pci_restore_state */
int 
pci_restore_state(struct pci_dev *dev)
{
	int i;
	u32 val;

	/*
	 * The Base Address register should be programmed before the command
	 * register(s)
	 */
	for (i = 15; i >= 0; i--) {
		pci_read_config_dword(dev, i * 4, &val);
		if (val != dev->saved_config_space[i]) {
			printk(KERN_DEBUG "PM: Writing back config space on "
				"device %s at offset %x (was %x, writing %x)\n",
				pci_name(dev), i,
				val, (int)dev->saved_config_space[i]);
			pci_write_config_dword(dev,i * 4,
				dev->saved_config_space[i]);
		}
	}
#if !defined(__VMKLNX__)
	pci_restore_msi_state(dev);
	pci_restore_msix_state(dev);
#endif /* !defined(__VMKLNX__) */
	return 0;
}

/**
 * pci_enable_device_bars - Initialize some of a device for use
 * @dev: PCI device to be initialized
 * @bars: bitmask of BAR's that must be configured
 *
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable selected I/O and memory resources. Wake up the device if it 
 *  was suspended. Beware, this function can fail.
 */
 
#if !defined(__VMKLNX__)
int
pci_enable_device_bars(struct pci_dev *dev, int bars)
{
	int err;

	err = pci_set_power_state(dev, PCI_D0);
	if (err < 0 && err != -EIO)
		return err;
	err = pcibios_enable_device(dev, bars);
	if (err < 0)
		return err;
	return 0;
}
#endif /* !defined(__VMKLNX__) */
/**
 * pci_enable_device - Enable the PCI device before it's used by a driver.
 * @dev: PCI device to be enabled
 *
 * Enable the PCI device.
 *
 *  ESX Deviation Notes:
 *  Does not do any initialization or PCI configuration.
 *  This function can never fail.
 *
 * Return Value:
 *  0 if operation is successful.
 *
 */
/* _VMKLNX_CODECHECK_: pci_enable_device */
int
pci_enable_device(struct pci_dev *dev)
{
#if defined(__VMKLNX__)
        if (atomic_add_return(1, &dev->enable_cnt) > 1)
                return 0;               /* already enabled */
#else /* !defined(__VMKLNX__) */
	int err;

	if (dev->is_enabled)
		return 0;

	err = pci_enable_device_bars(dev, (1 << PCI_NUM_RESOURCES) - 1);
	if (err)
		return err;
	pci_fixup_device(pci_fixup_enable, dev);
#endif /*  defined(__VMKLNX__) */
	dev->is_enabled = 1;
	return 0;
}

#if defined(__VMKLNX__)
/*
 * Managed PCI resources.  This manages device on/off, intx/msi/msix
 * on/off and BAR regions.  pci_dev itself records msi/msix status, so
 * there's no need to track it separately.  pci_devres is initialized
 * when a device is enabled using managed PCI device enable interface.
 */
struct pci_devres {
	unsigned int enabled:1;
	unsigned int pinned:1;
	unsigned int orig_intx:1;
	unsigned int restore_intx:1;
	u32 region_mask;
};

static void pcim_release(struct device *gendev, void *res)
{
	struct pci_dev *dev = container_of(gendev, struct pci_dev, dev);
	struct pci_devres *this = res;
	int i;

	if (dev->msi_enabled)
		pci_disable_msi(dev);
	if (dev->msix_enabled)
		pci_disable_msix(dev);

	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++)
		if (this->region_mask & (1 << i))
			pci_release_region(dev, i);

	if (this->restore_intx)
		pci_intx(dev, this->orig_intx);

	if (this->enabled && !this->pinned)
		pci_disable_device(dev);
}

static struct pci_devres * get_pci_dr(struct pci_dev *pdev)
{
	struct pci_devres *dr, *new_dr;

	dr = devres_find(&pdev->dev, pcim_release, NULL, NULL);
	if (dr)
		return dr;

	new_dr = devres_alloc(pcim_release, sizeof(*new_dr), GFP_KERNEL);
	if (!new_dr)
		return NULL;
	return devres_get(&pdev->dev, new_dr, NULL, NULL);
}

static struct pci_devres * find_pci_dr(struct pci_dev *pdev)
{
	if (pci_is_managed(pdev))
		return devres_find(&pdev->dev, pcim_release, NULL, NULL);
	return NULL;
}

/**
 * pcim_enable_device - Managed pci_enable_device()
 * @pdev: PCI device to be initialized
 *
 * Managed pci_enable_device().
 *
 * RETURN VALUE:
 * 0 if the operation successful
 *
 */
/* _VMKLNX_CODECHECK_: pcim_enable_device */
int pcim_enable_device(struct pci_dev *pdev)
{
	struct pci_devres *dr;
	int rc;

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	dr = get_pci_dr(pdev);
	if (unlikely(!dr))
		return -ENOMEM;
	WARN_ON(!!dr->enabled);

	rc = pci_enable_device(pdev);
	if (!rc) {
		pdev->is_managed = 1;
		dr->enabled = 1;
	}
	return rc;
}
EXPORT_SYMBOL(pcim_enable_device);

/**
 * pcim_pin_device - Pin managed PCI device
 * @pdev: PCI device to pin
 *
 * Pin managed PCI device @pdev.  Pinned device won't be disabled on
 * driver detach.  @pdev must have been enabled with
 * pcim_enable_device().
 *
 * RETURN VALUE:
 * None.
 *
 */
/* _VMKLNX_CODECHECK_: pcim_pin_device */
void pcim_pin_device(struct pci_dev *pdev)
{
	struct pci_devres *dr;

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	dr = find_pci_dr(pdev);
	WARN_ON(!dr || !dr->enabled);
	if (dr)
		dr->pinned = 1;
}
EXPORT_SYMBOL(pcim_pin_device);
#endif /* defined(__VMKLNX__) */

/**
 * pcibios_disable_device - disable arch specific PCI resources for device dev
 * @dev: the PCI device to disable
 *
 * Disables architecture specific PCI resources for the device. This
 * is the default implementation. Architecture implementations can
 * override this.
 */
#if !defined(__VMKLNX__)
void __attribute__ ((weak)) pcibios_disable_device (struct pci_dev *dev) {}
#endif /* !defined(__VMKLNX__) */
/**
 * pci_disable_device - Disable PCI device after use
 * @dev: PCI device to be disabled
 *
 * Disables the PCI device and PCI bus mastering.
 *
 * ESX Deviation Notes:
 * No signaling to the system.
 *
 * Return Value:
 * Does not return any value
 */
/* _VMKLNX_CODECHECK_: pci_disable_device */
void
pci_disable_device(struct pci_dev *dev)
{
#if defined(__VMKLNX__)
	struct pci_devres *dr;

	dr = find_pci_dr(dev);
	if (dr)
		dr->enabled = 0;

        if (atomic_sub_return(1, &dev->enable_cnt) != 0)
                return;
#else /* !defined(__VMKLNX__) */
	u16 pci_command;

	if (dev->msi_enabled)
		disable_msi_mode(dev, pci_find_capability(dev, PCI_CAP_ID_MSI),
			PCI_CAP_ID_MSI);
	if (dev->msix_enabled)
		disable_msi_mode(dev, pci_find_capability(dev, PCI_CAP_ID_MSI),
			PCI_CAP_ID_MSIX);

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	if (pci_command & PCI_COMMAND_MASTER) {
		pci_command &= ~PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, pci_command);
	}
#endif /*  defined(__VMKLNX__) */
	dev->is_busmaster = 0;

#if !defined(__VMKLNX__)
	pcibios_disable_device(dev);
#endif /* !defined(__VMKLNX__) */
	dev->is_enabled = 0;
}

/**
 * pci_enable_wake - enable device to generate PME# when suspended
 * @dev: PCI device to operate on
 * @state: Current state of device.
 * @enable: Flag to enable or disable generation
 * 
 * Set the bits in the device's PM Capabilities to generate PME# when
 * the system is suspended. 
 *
 * Return Value:
 * -EIO is returned if device doesn't have PM Capabilities. 
 * -EINVAL is returned if device supports it, but can't generate wake events.
 * 0 if operation is successful.
 * 
 */
/* _VMKLNX_CODECHECK_: pci_enable_wake */
int pci_enable_wake(struct pci_dev *dev, pci_power_t state, int enable)
{
	int pm;
	u16 value;

	/* find PCI PM capability in list */
	pm = pci_find_capability(dev, PCI_CAP_ID_PM);

	/* If device doesn't support PM Capabilities, but request is to disable
	 * wake events, it's a nop; otherwise fail */
	if (!pm) 
		return enable ? -EIO : 0; 

	/* Check device's ability to generate PME# */
	pci_read_config_word(dev,pm+PCI_PM_PMC,&value);

	value &= PCI_PM_CAP_PME_MASK;
	value >>= ffs(PCI_PM_CAP_PME_MASK) - 1;   /* First bit of mask */

	/* Check if it can generate PME# from requested state. */
	if (!value || !(value & (1 << state))) 
		return enable ? -EINVAL : 0;

	pci_read_config_word(dev, pm + PCI_PM_CTRL, &value);

	/* Clear PME_Status by writing 1 to it and enable PME# */
	value |= PCI_PM_CTRL_PME_STATUS | PCI_PM_CTRL_PME_ENABLE;

	if (!enable)
		value &= ~PCI_PM_CTRL_PME_ENABLE;

	pci_write_config_word(dev, pm + PCI_PM_CTRL, value);
	
	return 0;
}

#if !defined(__VMKLNX__)
int
pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge)
{
	u8 pin;

	pin = dev->pin;
	if (!pin)
		return -1;
	pin--;
	while (dev->bus->self) {
		pin = (pin + PCI_SLOT(dev->devfn)) % 4;
		dev = dev->bus->self;
	}
	*bridge = dev;
	return pin;
}
#endif /* !defined(__VMKLNX__) */
/**
 *	pci_release_region - Release a PCI bar
 *	@pdev: PCI device whose resources were previously reserved by pci_request_region
 *	@bar: BAR to release
 *
 *	Releases the PCI I/O and memory resources previously reserved by a
 *	successful call to pci_request_region.  Call this function only
 *	after all use of the PCI regions has ceased.
 */
/* _VMKLNX_CODECHECK_: pci_release_region */
void pci_release_region(struct pci_dev *pdev, int bar)
{
#if defined(__VMKLNX__)
	struct pci_devres *dr;
#endif /* defined(__VMKLNX__) */

	if (pci_resource_len(pdev, bar) == 0)
		return;
	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO)
		release_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));
	else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM)
		release_mem_region(pci_resource_start(pdev, bar),
				pci_resource_len(pdev, bar));

#if defined(__VMKLNX__)
	dr = find_pci_dr(pdev);
	if (dr)
		dr->region_mask &= ~(1 << bar);
#endif /* defined(__VMKLNX__) */
}

/**
 *	pci_request_region - Reserved PCI I/O and memory resource
 *	@pdev: PCI device whose resources are to be reserved
 *	@bar: BAR to be reserved
 *	@res_name: Name to be associated with resource.
 *
 *	Mark the PCI region associated with PCI device @pdev BR @bar as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	RETURN VALUE:
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
/* _VMKLNX_CODECHECK_: pci_request_region */
int pci_request_region(struct pci_dev *pdev, int bar, const char *res_name)
{
#if defined(__VMKLNX__)
	struct pci_devres *dr;
#endif /* defined(__VMKLNX__) */

	if (pci_resource_len(pdev, bar) == 0)
		return 0;
		
	if (pci_resource_flags(pdev, bar) & IORESOURCE_IO) {
		if (!request_region(pci_resource_start(pdev, bar),
			    pci_resource_len(pdev, bar), res_name))
			goto err_out;
	}
	else if (pci_resource_flags(pdev, bar) & IORESOURCE_MEM) {
		if (!request_mem_region(pci_resource_start(pdev, bar),
				        pci_resource_len(pdev, bar), res_name))
			goto err_out;
	}

#if defined(__VMKLNX__)
	dr = find_pci_dr(pdev);
	if (dr)
		dr->region_mask |= 1 << bar;
#endif /* defined(__VMKLNX__) */
	
	return 0;

err_out:
	printk (KERN_WARNING "PCI: Unable to reserve %s region #%d:%llx@%llx "
		"for device %s\n",
		pci_resource_flags(pdev, bar) & IORESOURCE_IO ? "I/O" : "mem",
		bar + 1, /* PCI BAR # */
		(unsigned long long)pci_resource_len(pdev, bar),
		(unsigned long long)pci_resource_start(pdev, bar),
		pci_name(pdev));
	return -EBUSY;
}

#if defined(__VMKLNX__)
/**
 * pci_release_selected_regions - Release selected PCI I/O and memory resources
 * @pdev: PCI device whose resources were previously reserved
 * @bars: Bitmask of BARs to be released
 *
 * Release selected PCI I/O and memory resources previously reserved.
 * Call this function only after all use of the PCI regions has ceased.
 *
 * RETURN VALUE:
 * None.
 *
 */
/* _VMKLNX_CODECHECK_: pci_release_selected_regions */
void pci_release_selected_regions(struct pci_dev *pdev, int bars)
{
        int i;

        for (i = 0; i < 6; i++)
                if (bars & (1 << i))
                        pci_release_region(pdev, i);
}

/**
 * pci_request_selected_regions - Reserve selected PCI I/O and memory resources
 * @pdev: PCI device whose resources are to be reserved
 * @bars: Bitmask of BARs to be requested
 * @res_name: Name to be associated with resource
 *
 * Reserve selected PCI I/O and memory resources.
 *
 * RETURN VALUE:
 * 0 for success, or %EBUSY on error.
 *
 */
/* _VMKLNX_CODECHECK_: pci_request_selected_regions */
int pci_request_selected_regions(struct pci_dev *pdev, int bars,
                                 const char *res_name)
{
        int i;

        for (i = 0; i < 6; i++)
                if (bars & (1 << i))
                        if(pci_request_region(pdev, i, res_name))
                                goto err_out;
        return 0;

err_out:
        while(--i >= 0)
                if (bars & (1 << i))
                        pci_release_region(pdev, i);

        return -EBUSY;
}
#endif /* defined(__VMKLNX__) */

/**
 *	pci_release_regions - Release reserved PCI I/O and memory resources
 *	@pdev: PCI device whose resources were previously reserved by pci_request_regions
 *
 *	Releases all PCI I/O and memory resources previously reserved by a
 *	successful call to pci_request_regions.  Call this function only
 *	after all use of the PCI regions has ceased.
 */

/* _VMKLNX_CODECHECK_: pci_release_regions */
void pci_release_regions(struct pci_dev *pdev)
{
	int i;
	
	for (i = 0; i < 6; i++)
		pci_release_region(pdev, i);
}

/**
 *	pci_request_regions - Reserved PCI I/O and memory resources
 *	@pdev: PCI device whose resources are to be reserved
 *	@res_name: Name to be associated with resource.
 *
 *	Mark all PCI regions associated with PCI device @pdev as
 *	being reserved by owner @res_name.  Do not access any
 *	address inside the PCI regions unless this call returns
 *	successfully.
 *
 *	Returns 0 on success, or %EBUSY on error.  A warning
 *	message is also printed on failure.
 */
/* _VMKLNX_CODECHECK_: pci_request_regions */
int pci_request_regions(struct pci_dev *pdev, const char *res_name)
{
	int i;
	
	for (i = 0; i < 6; i++)
		if(pci_request_region(pdev, i, res_name))
			goto err_out;
	return 0;

err_out:
	while(--i >= 0)
		pci_release_region(pdev, i);
		
	return -EBUSY;
}

#if defined(__VMKLNX__)
/*
 *  Wrappers for all PCI configuration access functions.  They just check
 *  alignment, do locking and call the low-level functions pointed to
 *  by pci_dev->ops.
 */

#define PCI_byte_BAD 0
#define PCI_word_BAD (where & 1)
#define PCI_dword_BAD (where & 3)

#define PCI_OP(rw,size,type,vmkrw,vmksize) \
int pci_##rw##_config_##size (struct pci_dev *dev, int where, type val) \
{									\
	if (PCI_##size##_BAD) return PCIBIOS_BAD_REGISTER_NUMBER;	\
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);		\
	return LinuxPCIConfigSpace##vmkrw(dev, vmksize, where, val);	\
}

/*                                          
 *  Please refer to vmkdrivers/src_92/doc/dummyDefs.doc for actual kernel-doc comments.
 */                                          
PCI_OP(read, byte, u8 *, Read, 8)
EXPORT_SYMBOL(pci_read_config_byte);

/*                                          
 *  Please refer to vmkdrivers/src_92/doc/dummyDefs.doc for actual kernel-doc comments.
 */                                          
PCI_OP(read, word, u16 *, Read, 16)
EXPORT_SYMBOL(pci_read_config_word);

/*                                          
 *  Please refer to vmkdrivers/src_92/doc/dummyDefs.doc for actual kernel-doc comments.
 */                                          
PCI_OP(read, dword, u32 *, Read, 32)
EXPORT_SYMBOL(pci_read_config_dword);

/*                                          
 *  Please refer to vmkdrivers/src_92/doc/dummyDefs.doc for actual kernel-doc comments.
 */                                          
PCI_OP(write, byte, u8, Write, 8)
EXPORT_SYMBOL(pci_write_config_byte);

/*                                          
 *  Please refer to vmkdrivers/src_92/doc/dummyDefs.doc for actual kernel-doc comments.
 */                                          
PCI_OP(write, word, u16, Write, 16)
EXPORT_SYMBOL(pci_write_config_word);

/*                                          
 *  Please refer to vmkdrivers/src_92/doc/dummyDefs.doc for actual kernel-doc comments.
 */                                          
PCI_OP(write, dword, u32, Write, 32)
EXPORT_SYMBOL(pci_write_config_dword);

#endif /* defined(__VMKLNX__) */

/**
 * pci_set_master - enables bus-mastering for device dev
 * @dev: the PCI device to enable
 *
 * Enables bus-mastering on the device.
 *
 */
/* _VMKLNX_CODECHECK_: pci_set_master */
void
pci_set_master(struct pci_dev *dev)
{
	u16 cmd;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (! (cmd & PCI_COMMAND_MASTER)) {
		pr_debug("PCI: Enabling bus mastering for device %s\n", pci_name(dev));
		cmd |= PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	dev->is_busmaster = 1;
#if !defined(__VMKLNX__)
	pcibios_set_master(dev);
#endif /* !defined(__VMKLNX__) */
}

#ifndef HAVE_ARCH_PCI_MWI
/* This can be overridden by arch code. */
u8 pci_cache_line_size = L1_CACHE_BYTES >> 2;

/**
 * pci_generic_prep_mwi - helper function for pci_set_mwi
 * @dev: the PCI device for which MWI is enabled
 *
 * Helper function for generic implementation of pcibios_prep_mwi
 * function.  Originally copied from drivers/net/acenic.c.
 * Copyright 1998-2001 by Jes Sorensen, <jes@trained-monkey.org>.
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
static int
pci_generic_prep_mwi(struct pci_dev *dev)
{
	u8 cacheline_size;

	if (!pci_cache_line_size)
		return -EINVAL;		/* The system doesn't support MWI. */

	/* Validate current setting: the PCI_CACHE_LINE_SIZE must be
	   equal to or multiple of the right value. */
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size >= pci_cache_line_size &&
	    (cacheline_size % pci_cache_line_size) == 0)
		return 0;

	/* Write the correct value. */
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, pci_cache_line_size);
	/* Read it back. */
	pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &cacheline_size);
	if (cacheline_size == pci_cache_line_size)
		return 0;

	printk(KERN_DEBUG "PCI: cache line size of %d is not supported "
	       "by device %s\n", pci_cache_line_size << 2, pci_name(dev));

	return -EINVAL;
}
#endif /* !HAVE_ARCH_PCI_MWI */


#if defined(__VMKLNX__)
/**
 *  pci_try_set_mwi - enables memory-write-invalidate PCI transaction
 *  @dev: the PCI device for which MWI is enabled
 *
 *  Enables the Memory-Write-Invalidate transaction in %PCI_COMMAND.
 *  Callers are not required to check the return value.
 *
 *  RETURN VALUE:
 *  0 on Success
 *  Appropriate -ERRNO on error
 *
 */
 /* _VMKLNX_CODECHECK_: pci_try_set_mwi */
int pci_try_set_mwi(struct pci_dev *dev)
{
    int rc;
    VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
    rc = pci_set_mwi(dev);
    return rc;
}
EXPORT_SYMBOL(pci_try_set_mwi);
#endif /* defined(__VMKLNX__) */

/**
 * pci_set_mwi - enables memory-write-invalidate PCI transaction
 * @dev: the PCI device for which MWI is enabled
 *
 * Enables the Memory-Write-Invalidate transaction in %PCI_COMMAND,
 * and then calls @pcibios_set_mwi to do the needed arch specific
 * operations or a generic mwi-prep function.
 *
 * RETURN VALUE:
 * An appropriate -ERRNO error value on error, or zero for success.
 */
/* _VMKLNX_CODECHECK_: pci_set_mwi */
int
pci_set_mwi(struct pci_dev *dev)
{
	int rc;
	u16 cmd;

#ifdef HAVE_ARCH_PCI_MWI
	rc = pcibios_prep_mwi(dev);
#else
	rc = pci_generic_prep_mwi(dev);
#endif

	if (rc)
		return rc;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (! (cmd & PCI_COMMAND_INVALIDATE)) {
		pr_debug("PCI: Enabling Mem-Wr-Inval for device %s\n", pci_name(dev));
		cmd |= PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	
	return 0;
}

/**
 *  pci_clear_mwi - disables Memory-Write-Invalidate for device dev
 *  @dev: the PCI device to disable
 *
 *  Disables PCI Memory-Write-Invalidate transaction on the device
 *
 *  RETURN VALUE:
 *  This function does not return a value
 */
/* _VMKLNX_CODECHECK_: pci_clear_mwi */
void
pci_clear_mwi(struct pci_dev *dev)
{
	u16 cmd;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	if (cmd & PCI_COMMAND_INVALIDATE) {
		cmd &= ~PCI_COMMAND_INVALIDATE;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
}

/**
 * pci_intx - enables/disables PCI INTx for device dev
 * @pdev: the PCI device to operate on
 * @enable: boolean value whether to enable or disable PCI INTx
 *
 * Enables/disables PCI INTx for device dev
 *
 * Return Value:
 * Does not return any value
 */
/* _VMKLNX_CODECHECK_: pci_intx */
void
pci_intx(struct pci_dev *pdev, int enable)
{
	u16 pci_command, new;

	pci_read_config_word(pdev, PCI_COMMAND, &pci_command);

	if (enable) {
		new = pci_command & ~PCI_COMMAND_INTX_DISABLE;
	} else {
		new = pci_command | PCI_COMMAND_INTX_DISABLE;
	}

	if (new != pci_command) {
#if defined(__VMKLNX__)
		struct pci_devres *dr;
#endif /* defined(__VMKLNX__) */

		pci_write_config_word(pdev, PCI_COMMAND, new);

#if defined(__VMKLNX__)
		dr = find_pci_dr(pdev);
		if (dr && !dr->restore_intx) {
			dr->restore_intx = 1;
			dr->orig_intx = !enable;
		}
#endif /* defined(__VMKLNX__) */
	}
}

#ifndef HAVE_ARCH_PCI_SET_DMA_MASK
#if !defined(__VMKLNX__)
/*
 * These can be overridden by arch-specific implementations
 */

/**
 *  pci_set_dma_mask - set the DMA mask for the PCI device
 *  @dev: pointer to the PCI device
 *  @mask: the DMA mask for the device
 *
 *  If DMA is supported by the @dev, the @mask is set. 
 *
 *  RETURN VALUE:
 *  Returns 0 if @dev supports DMA, -EIO otherwise.
 *
 */
/* _VMKLNX_CODECHECK_: pci_set_dma_mask */
int
pci_set_dma_mask(struct pci_dev *dev, u64 mask)
{
	if (!pci_dma_supported(dev, mask))
		return -EIO;

	dev->dma_mask = mask;

	return 0;
}

/**
 *  pci_set_consistent_dma_mask - set the DMA mask for the PCI device
 *  @dev: pointer to the PCI device
 *  @mask: the DMA mask for the device
 *
 *  If DMA is supported by the @dev, the @mask is set.
 *
 *  RETURN VALUE:
 *  Returns 0 if @dev supports DMA, -EIO otherwise.
 *
 */
/* _VMKLNX_CODECHECK_: pci_set_consistent_dma_mask */
int
pci_set_consistent_dma_mask(struct pci_dev *dev, u64 mask)
{
	if (!pci_dma_supported(dev, mask))
		return -EIO;

	dev->dev.coherent_dma_mask = mask;

	return 0;
}
#endif /* !defined(__VMKLNX__) */
#endif /* !defined(HAVE_ARCH_PCI_SET_DMA_MASK) */

#if !defined(__VMKLNX__)
static int __devinit pci_init(void)
{
	struct pci_dev *dev = NULL;

	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		pci_fixup_device(pci_fixup_final, dev);
	}
	return 0;
}

static int __devinit pci_setup(char *str)
{
	while (str) {
		char *k = strchr(str, ',');
		if (k)
			*k++ = 0;
		if (*str && (str = pcibios_setup(str)) && *str) {
			if (!strcmp(str, "nomsi")) {
				pci_no_msi();
			} else {
				printk(KERN_ERR "PCI: Unknown option `%s'\n",
						str);
			}
		}
		str = k;
	}
	return 1;
}

device_initcall(pci_init);

__setup("pci=", pci_setup);
#endif /* !defined(__VMKLNX__) */

#if defined(__VMKLNX__)

/*
 *  Registration of PCI drivers and handling of hot-pluggable devices.
 */

static LIST_HEAD(pci_drivers);
LIST_HEAD(pci_devices);
EXPORT_SYMBOL(pci_devices);

/**
 * pci_match_device - Tell if a PCI device structure has a matching PCI device id structure
 * @ids: array of PCI device id structures to search in
 * @dev: the PCI device structure to match against
 *
 * Used by a driver to check whether a PCI device present in the
 * system is in its list of supported devices.Returns the matching
 * pci_device_id structure or %NULL if there is no match.
 */
const struct pci_device_id *
pci_match_device(const struct pci_device_id *ids, const struct pci_dev *dev)
{
   while (ids->vendor || ids->subvendor || ids->class_mask) {
      if ((ids->vendor == PCI_ANY_ID || ids->vendor == dev->vendor) &&
          (ids->device == PCI_ANY_ID || ids->device == dev->device) &&
          (ids->subvendor == PCI_ANY_ID || ids->subvendor == dev->subsystem_vendor) &&
          (ids->subdevice == PCI_ANY_ID || ids->subdevice == dev->subsystem_device) &&
          !((ids->class ^ dev->class) & ids->class_mask))
         return ids;
      ids++;
   }  
   return NULL;
}  

static int
pci_announce_device(struct pci_driver *drv, struct pci_dev *dev)
{
   const struct pci_device_id *id;
   int ret = 0; 
   vmk_ModuleID moduleID;
   LinuxPCIDevExt *pe = NULL;

   if (drv->id_table) {
      id = pci_match_device(drv->id_table, dev);
      if (!id) {
         goto out;
      }  
   } else {
      id = NULL;
   }

   pe = container_of(dev, LinuxPCIDevExt, linuxDev);
   moduleID = vmklnx_get_driver_module_id(&drv->driver);
   /* set moduleID before probe, needed for request_irq */
   pe->moduleID = moduleID;

   /* bind driver with pci device before probe since vmklinux scsi needs
    * dev->driver->driver.owner to retrieve ModuleID  */
   dev->driver = drv;
   dev->dev.driver = &drv->driver;
   dev->dev.bus = &pci_bus_type;
   VMKAPI_MODULE_CALL(moduleID, ret, drv->probe, dev, id);
   if (ret >= 0) {
      if (dev->netdev) {
         printk("PCI: Registering network device %s\n", dev->dev.bus_id);
         //XXX: Add stress option
         if (LinNet_ConnectUplink(dev->netdev, dev)) {
            if (drv->remove) {
               VMKAPI_MODULE_CALL_VOID(moduleID, drv->remove, dev);
            }  
            goto probe_failed;
         }
      }
      ret = 1;
      LinuxPCI_DeviceClaimed(pe, moduleID);
      printk("PCI: driver %s claimed device %s\n", drv->name, dev->dev.bus_id);
      goto out;
   }

probe_failed:
   VMKLNX_WARN("PCI: driver %s probe failed for device %s", drv->name, dev->dev.bus_id);

   /* Only call for devres mananged device, different from Linux */
   if (pci_is_managed(dev)) {
      /* VMKAPI_MODULE_CALL_VOID wrapper is required for inter-module
       * call. devres_release_all() will call driver's release function
       * for every managed resource, this should be done in driver's
       * context in ESX (different from Linux)
       */
      VMKAPI_MODULE_CALL_VOID(moduleID, devres_release_all, &dev->dev);
   }

   /* unbind the 'drv' & 'dev' */
   dev->driver = NULL;
   dev->dev.driver = NULL;
   LinuxPCI_DeviceUnclaimed(pe);
   ret = 0;

out:
   return ret;
}

/**
 * pci_announce_device_to_drivers - tell the drivers a new device has appeared
 * @dev: the device that has shown up
 *
 * Notifys the drivers that a new device has appeared, and also notifys
 * userspace through /sbin/hotplug.
 */
void
pci_announce_device_to_drivers(struct pci_dev *dev)
{
   struct list_head *ln;

   VMKLNX_DEBUG(0, "PCI: device %s is looking for driver...", dev->dev.bus_id);

   /*
    * Hold pci_bus_sem while iterating to prevent drivers from
    * registering or unregistering during device announcement
    */
   down_read(&pci_bus_sem);
   for(ln=pci_drivers.next; ln != &pci_drivers; ln=ln->next) {
      struct pci_driver *drv = list_entry(ln, struct pci_driver, node);
      up_read(&pci_bus_sem);

      if (drv->remove && pci_announce_device(drv, dev))
         return;

      down_read(&pci_bus_sem);
   }
   up_read(&pci_bus_sem);
}

/**
 * __pci_register_driver - register a new pci driver
 * @drv: the driver structure to register
 * @owner: owner module of drv
 * 
 * Adds the driver structure to the list of registered drivers.
 * Returns a negative value on error, otherwise 0. 
 * If no error occurred, the driver remains registered even if 
 * no device was claimed during registration.
 */
int __pci_register_driver(struct pci_driver *drv, struct module *owner)
{
   struct pci_dev *dev;
   int count = 0;

   VMK_ASSERT(owner != NULL);
   VMK_ASSERT(owner->moduleID != VMK_INVALID_MODULE_ID);
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   /* initialize common driver fields */
   drv->driver.name = drv->name;
   drv->driver.bus = &pci_bus_type;
   drv->driver.owner = owner;

   down_write(&pci_bus_sem);
   printk("PCI: driver %s is looking for devices\n", drv->name);
   list_add_tail(&drv->node, &pci_drivers);
   up_write(&pci_bus_sem);

   /*
    * Use for_each_pci_dev to safely iterate pci_devices list
    */
   dev = NULL;
   for_each_pci_dev(dev) {
      VMKLNX_DEBUG(3, "PCI: Trying %s\n", dev->dev.bus_id);
      if (!pci_dev_driver(dev)) {
         count += pci_announce_device(drv, dev);
      }  
   }  
   printk("PCI: driver %s claimed %d device%c\n",
         drv->name, count, count > 1 ? 's':' ');

   return 0;
}
EXPORT_SYMBOL(__pci_register_driver);

/**
 * pci_unregister_driver - unregister a pci driver
 * @drv: the driver structure to unregister
 * 
 * Deletes the driver structure from the list of registered PCI drivers,
 * gives it a chance to clean up by calling its remove() function for
 * each device it was responsible for, and marks those devices as
 * driverless.
 */

/* _VMKLNX_CODECHECK_: pci_unregister_driver */
void
pci_unregister_driver(struct pci_driver *drv)
{
   struct pci_dev *dev;
   
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
   down_write(&pci_bus_sem);
   list_del(&drv->node);
   up_write(&pci_bus_sem);

   /*
    * Use for_each_pci_dev to safely iterate pci_devices list
    */
   dev = NULL;
   for_each_pci_dev(dev) {
      if (dev->driver == drv) {
         LinuxPCIDevExt *pciDevExt = container_of(dev, LinuxPCIDevExt, linuxDev);
         vmk_ModuleID moduleID = pciDevExt->moduleID;

	 vmk_PCIDoPreRemove(moduleID, pciDevExt->vmkDev);
         if (drv->remove)
                 VMKAPI_MODULE_CALL_VOID(moduleID, drv->remove, dev);

         /* Only call for devres mananged device, different from Linux */
         if (pci_is_managed(dev)) {
            /* VMKAPI_MODULE_CALL_VOID wrapper is required for inter-module
             * call. devres_release_all() will call driver's release function
             * for every managed resource, this should be done in driver's
             * context in ESX (different from Linux)
             */
            VMKAPI_MODULE_CALL_VOID(moduleID, devres_release_all, &dev->dev);
         }

         dev->driver = NULL;
         dev->dev.driver = NULL;
         LinuxPCI_DeviceUnclaimed(pciDevExt);
      }  
   }  
}
EXPORT_SYMBOL(pci_unregister_driver);

/*
 * Translate the low bits of the PCI base
 * to the resource type
 */
unsigned int pci_calc_resource_flags(unsigned int flags)
{
   if (flags & PCI_BASE_ADDRESS_SPACE_IO)
      return IORESOURCE_IO;

   if (flags & PCI_BASE_ADDRESS_MEM_PREFETCH)
      return IORESOURCE_MEM | IORESOURCE_PREFETCH;

   return IORESOURCE_MEM;
}

/*
 * Find the extent of a PCI decode, do sanity checks.
 */
static u32 pci_size(u32 base, u32 maxbase, unsigned long mask)
{
   u32 size = mask & maxbase; /* Find the significant bits */
   if (!size)
      return 0;
   size = size & ~(size-1);   /* Get the lowest of them to find the decode size */
   size -= 1;        /* extent = size - 1 */
   if (base == maxbase && ((base | size) & mask) != mask)
      return 0;      /* base == maxbase can be valid only
                  if the BAR has been already
                  programmed with all 1s */
   return size;
}

void pci_fill_rom_bar(struct pci_dev *dev, int rom)
{
	u32 l, sz;
	struct resource *res;

	dev->rom_base_reg = rom;
	res = &dev->resource[PCI_ROM_RESOURCE];
	res->name = pci_name(dev);
	pci_read_config_dword(dev, rom, &l);
	pci_write_config_dword(dev, rom, ~PCI_ROM_ADDRESS_ENABLE);
	pci_read_config_dword(dev, rom, &sz);
	pci_write_config_dword(dev, rom, l);
	if (l == 0xffffffff)
		l = 0;
	if (sz && sz != 0xffffffff) {
		sz = pci_size(l, sz, (u32)PCI_ROM_ADDRESS_MASK);
		if (sz) {
			res->flags = (l & IORESOURCE_ROM_ENABLE) |
			  IORESOURCE_MEM | IORESOURCE_PREFETCH |
			  IORESOURCE_READONLY | IORESOURCE_CACHEABLE;
			res->start = l & PCI_ROM_ADDRESS_MASK;
			res->end = res->start + (unsigned long) sz;
		}
	}
}

static struct pci_driver pci_compat_driver = {
   .name = "compat"
};

/**
 *  pci_dev_driver - get the pci_driver of a device
 *  @dev: the device to query
 *
 *  Get the pci_driver of a device
 *
 *  RETURN VALUE:
 *  Returns the appropriate pci_driver structure or %NULL if there is no 
 *  registered driver for the device.
 */
/* _VMKLNX_CODECHECK_: pci_dev_driver */
struct pci_driver *
pci_dev_driver(const struct pci_dev *dev)
{
#if defined(__VMKLNX__)
   VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
   if (dev->driver)
      return dev->driver;
   else {
      int i;
      for(i=0; i<=PCI_ROM_RESOURCE; i++)
         if (dev->resource[i].flags & IORESOURCE_BUSY)
            return &pci_compat_driver;
   }
   return NULL;
}
EXPORT_SYMBOL(pci_dev_driver);

/*
 *  Determine whether some value is a power of two, where zero is
 *  *not* considered a power of two.
 */

static inline __attribute__((const))
int is_power_of_2(unsigned long n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

/**
 *  pcie_set_readrq - set PCI Express maximum memory read request
 *  @dev: PCI device to query
 *  @rq: maximum memory read count in bytes
 *
 *  If possible sets maximum read byte count.
 *  Valid values are 128, 256, 512, 1024, 2048, 4096
 *
 *  RETURN VALUE:
 *  0 on Success
 *  On Failure, any errors that are seen while setting the passed in value
 *  If rq is not correct, -EINVAL is returned.
 *
 */
 /* _VMKLNX_CODECHECK_: pcie_set_readrq */
int pcie_set_readrq(struct pci_dev *dev, int rq)
{
	int cap, err = -EINVAL;
	u16 ctl, v;

#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (rq < 128 || rq > 4096 || !is_power_of_2(rq))
		goto out;

	v = (ffs(rq) - 8) << 12;

	cap = pci_find_capability(dev, PCI_CAP_ID_EXP);
	if (!cap)
		goto out;

	err = pci_read_config_word(dev, cap + PCI_EXP_DEVCTL, &ctl);
	if (err)
		goto out;

	if ((ctl & PCI_EXP_DEVCTL_READRQ) != v) {
		ctl &= ~PCI_EXP_DEVCTL_READRQ;
		ctl |= v;
		err = pci_write_config_dword(dev, cap + PCI_EXP_DEVCTL, ctl);
	}

out:
	return err;
}
EXPORT_SYMBOL(pcie_set_readrq);
#endif /* defined(__VMKLNX__) */

/**
 *  pci_match_id - See if a pci device matches a given pci_id table
 *  @ids: array of PCI device id structures to search in
 *  @dev: the PCI device structure to match against.
 *
 *  Used by a driver to check whether a PCI device present in the
 *  system is in its list of supported devices.
 *
 *  This is Deprecated.
 *  Do NOT use this as it will not catch any dynamic ids that a driver might
 *  want to check for.
 *
 *  RETURN VALUE:
 *  Returns the matching pci_device_id structure or %NULL if there is no match.
 *
 */
 /* _VMKLNX_CODECHECK_: pci_match_id */
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
					 struct pci_dev *dev)
{
#if defined(__VMKLNX__)
	VMK_ASSERT(vmk_PreemptionIsEnabled() == VMK_FALSE);
#endif
	if (ids) {
		while (ids->vendor || ids->subvendor || ids->class_mask) {
			if (pci_match_one_device(ids, dev))
				return ids;
			ids++;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(pci_match_id);

#if defined(CONFIG_ISA) || defined(CONFIG_EISA)
/* FIXME: Some boxes have multiple ISA bridges! */
struct pci_dev *isa_bridge;
EXPORT_SYMBOL(isa_bridge);
#endif

#if !defined(__VMKLNX__) 
EXPORT_SYMBOL_GPL(pci_restore_bars);
EXPORT_SYMBOL(pci_enable_device_bars);
#endif /* !defined(__VMKLNX__) */
EXPORT_SYMBOL(pci_enable_device);
EXPORT_SYMBOL(pci_disable_device);
EXPORT_SYMBOL(pci_find_capability);
EXPORT_SYMBOL(pci_bus_find_capability);
EXPORT_SYMBOL(pci_release_regions);
EXPORT_SYMBOL(pci_request_regions);
EXPORT_SYMBOL(pci_release_region);
EXPORT_SYMBOL(pci_request_region);
#if defined(__VMKLNX__)
EXPORT_SYMBOL(pci_release_selected_regions);
EXPORT_SYMBOL(pci_request_selected_regions);
#endif /* defined(__VMKLNX__) */
EXPORT_SYMBOL(pci_set_master);
EXPORT_SYMBOL(pci_set_mwi);
EXPORT_SYMBOL(pci_clear_mwi);
EXPORT_SYMBOL_GPL(pci_intx);
#if !defined(__VMKLNX__) 
EXPORT_SYMBOL(pci_set_dma_mask);
EXPORT_SYMBOL(pci_set_consistent_dma_mask);
EXPORT_SYMBOL(pci_assign_resource);
EXPORT_SYMBOL(pci_find_parent_resource);
#endif /* !defined(__VMKLNX__) */
EXPORT_SYMBOL(pci_set_power_state);
EXPORT_SYMBOL(pci_save_state);
EXPORT_SYMBOL(pci_restore_state);
EXPORT_SYMBOL(pci_enable_wake);

/* Quirk info */

#if !defined(__VMKLNX__) 
EXPORT_SYMBOL(isa_dma_bridge_buggy);
EXPORT_SYMBOL(pci_pci_problems);
#endif /* !defined(__VMKLNX__) */
