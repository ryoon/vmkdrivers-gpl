/*
 *    Disk Array driver for HP Smart Array SAS controllers
 *    Copyright 2000-2012 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */
#ifndef _HPSA_BOARDS_H_
#define _HPSA_BOARDS_H_

/* define the PCI info for the cards we can control */
#ifndef PCI_DEVICE_ID_COMPAQ_HPSAC
#define PCI_DEVICE_ID_COMPAQ_HPSAC 0x46
#endif
#ifndef PCI_DEVICE_ID_HP_HPSA
#define PCI_DEVICE_ID_HP_HPSA 0x3210
#endif
#ifndef PCI_DEVICE_ID_HP_HPSAA
#define PCI_DEVICE_ID_HP_HPSAA 0x3220
#endif
#ifndef PCI_DEVICE_ID_HP_HPSAC
#define PCI_DEVICE_ID_HP_HPSAC 0x3230
#endif
#ifndef PCI_DEVICE_ID_HP_HPSAD
#define PCI_DEVICE_ID_HP_HPSAD 0x3238
#endif
#ifndef PCI_DEVICE_ID_HP_HPSAE
#define PCI_DEVICE_ID_HP_HPSAE 0x323A
#endif
#ifndef PCI_DEVICE_ID_HP_CISSF
#define PCI_DEVICE_ID_HP_CISSF 0x333F
#endif
#ifndef PCI_DEVICE_ID_HP_HPSAG
#define PCI_DEVICE_ID_HP_HPSAG 0x323B
#endif
#ifndef PCI_DEVICE_ID_HP_HPSAH
#define PCI_DEVICE_ID_HP_HPSAH 0x323C
#endif

/* define the PCI info for the cards we can control */
static const struct pci_device_id hpsa_pci_device_id[] = {
#if defined (__VMKLNX__)
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAC,	0x103C,0x323D},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3241},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3243},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3245},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3247},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3249},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x324a},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x324b},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAG,	0x103C,0x3350},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAG,	0x103C,0x3351},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAG,	0x103C,0x3352},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAG,	0x103C,0x3353},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAG,	0x103C,0x3354},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAG,	0x103C,0x3355},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAG,	0x103C,0x3356},
        {PCI_VENDOR_ID_HP,      PCI_DEVICE_ID_HP_HPSAH, 0x103C,0x1920},
        {PCI_VENDOR_ID_HP,      PCI_DEVICE_ID_HP_HPSAH, 0x103C,0x1921},
        {PCI_VENDOR_ID_HP,      PCI_DEVICE_ID_HP_HPSAH, 0x103C,0x1922},
        {PCI_VENDOR_ID_HP,      PCI_DEVICE_ID_HP_HPSAH, 0x103C,0x1923},
        {PCI_VENDOR_ID_HP,      PCI_DEVICE_ID_HP_HPSAH, 0x103C,0x1924},
        {PCI_VENDOR_ID_HP,      PCI_DEVICE_ID_HP_HPSAH, 0x103C,0x1926},
        {PCI_VENDOR_ID_HP,      PCI_DEVICE_ID_HP_HPSAH, 0x103C,0x1928},
	{0,}
#else // LINUX
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3241},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3243},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3245},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3247},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x3249},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x324a},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_HPSAE,	0x103C,0x324b},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_CISSE,	0x103C,0x3233},
	{PCI_VENDOR_ID_HP,	PCI_DEVICE_ID_HP_CISSF,	0x103C,0x333F},
	{PCI_VENDOR_ID_HP,	PCI_ANY_ID,	PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_STORAGE_RAID << 8, 0xffff << 8, 0},
	{0,}
#endif
};

/*  board_id = Subsystem Device ID & Vendor ID
 *  product = Marketing Name for the board
 *  access = Address of the struct of function pointers
 */
static struct board_type products[] = {
#if defined (__VMKLNX__)
	{0x323d103c,	"Smart Array P700M",	&SA5_access},
	{0x3241103C,	"Smart Array P212",	&SA5_access},
	{0x3243103C,	"Smart Array P410",	&SA5_access},
	{0x3245103C,	"Smart Array P410i",	&SA5_access},
	{0x3247103C,	"Smart Array P411",	&SA5_access},
	{0x3249103C,	"Smart Array P812",	&SA5_access},
	{0x324a103C,	"Smart Array P712m",	&SA5_access},
	{0x324b103C,	"Smart Array P711m",	&SA5_access},
	{0x3350103C,	"Smart Array P222",	&SA5_access},
	{0x3351103C,	"Smart Array P420",	&SA5_access},
	{0x3352103C,	"Smart Array P421",	&SA5_access},
	{0x3353103C,	"Smart Array",		&SA5_access},
	{0x3354103C,	"Smart Array P420i",	&SA5_access},
	{0x3355103C,	"Smart Array P220i",	&SA5_access},
	{0x3356103C,	"Smart Array",		&SA5_access},
	{0x1920103C,	"Smart Array P430i",	&SA5_access},
	{0x1921103C,	"Smart Array P830i",	&SA5_access},
	{0x1922103C,	"Smart Array P430",	&SA5_access},
	{0x1923103C,	"Smart Array P431",	&SA5_access},
	{0x1924103C,	"Smart Array P830",	&SA5_access},
	{0x1926103C,	"Smart Array P731m",	&SA5_access},
	{0x1928103C,	"Smart Array P230i",	&SA5_access},
#else // LINUX
	{0x3241103C,	"Smart Array P212",	&SA5_access},
	{0x3243103C,	"Smart Array P410",	&SA5_access},
	{0x3245103C,	"Smart Array P410i",	&SA5_access},
	{0x3247103C,	"Smart Array P411",	&SA5_access},
	{0x3249103C,	"Smart Array P812",	&SA5_access},
	{0x324a103C,	"Smart Array P712m",	&SA5_access},
	{0x324b103C,	"Smart Array P711m",	&SA5_access},
	{0x3233103C,	"StorageWorks P1210m",	&SA5_access},
	/* This ID added to cover a bug in another OS */
	{0x333F103C,	"StorageWorks P1210m",	&SA5_access},
	{0xFFFF103C,	"Unknown Smart Array",	&SA5_access},
#endif 
};
MODULE_DEVICE_TABLE(pci, hpsa_pci_device_id);

#endif /* _HPSA_BOARDS_H_ */
