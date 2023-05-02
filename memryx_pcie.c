#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#define DRIVER_NAME "memx"
#define DEVICE_NODE_NAME "memx"

#define MEMRYX_VENDOR_ID 0x0559
#define MEMRYX_DEVICE_ID 0x4006

static struct pci_device_id memx_pcie_id_table[] = 
{
	{
		vendor : MEMRYX_VENDOR_ID,
		device : MEMRYX_DEVICE_ID,
		subvendor : PCI_ANY_ID,
		subdevice : PCI_ANY_ID,
		class : 0,
		class_mask : 0,
	        driver_data : 0,	
	},
	{ 0 },
};
MODULE_DEVICE_TABLE(pci, memx_pcie_id_table);


static int memx_pcie_probe(struct pci_dev* pDev, const struct pci_device_id* id) 
{
	printk(KERN_ALERT "Memryx PCie Probe");
	return 0;
}

static void memx_pcie_remove(struct pci_dev* pDev) 
{
	printk(KERN_ALERT "Memryx PCie Remove");
}

static struct pci_driver memx_pcie_driver = 
{
	name: DRIVER_NAME,
	id_table: memx_pcie_id_table,
	probe: memx_pcie_probe,
	remove: memx_pcie_remove,
};



int __init memx_pcie_module_init(void) 
{
	int err = 0;
	printk(KERN_ALERT "Memryx PCie Init");
	err = pci_register_driver(&memx_pcie_driver);
	if (err != 0) {
		printk(KERN_ALERT "pcie register driver fail\n");
		return err;
	}
	
	return 0;
}

void __exit memx_pcie_module_exit(void) 
{
	pci_unregister_driver(&memx_pcie_driver);
	printk(KERN_ALERT "Memryx PCie Exit");
}
/*
static struct file_operations memx_pcie_fops = 
{
	owner : THIS_MODULE,
	unlocked_ioctl : memx_pcie_fops_unlocked_ioctl,
	open: memx_open,
	release: memx_relase,
};
*/

module_init(memx_pcie_module_init);
module_exit(memx_pcie_module_exit);

MODULE_AUTHOR("ZHI-WEI CHEN");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1 demo");
