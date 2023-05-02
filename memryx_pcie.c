#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>


typedef enum {
    BAR0 = 0,
    BAR1,
    BAR2,
    BAR3,
    BAR4,
    BAR5,
    MAX_BAR
} memx_pcie_bars;

typedef struct {
    void *user_address;
    void *kernel_address;
    uint64_t phys_address;
    uint64_t length;
    int memory_flag;
    int active_flag;
} memx_pcie_bar_t;

/* structure used in ioctl	PLDA_IOCG_PCI_INFO */
typedef struct {
    memx_pcie_bar_t bars[MAX_BAR];
} memx_pcie_bar_resources_t;

typedef struct {
    struct pci_dev *pDev;
    memx_pcie_bar_t bar[MAX_BAR];
    struct semaphore mutex;
} memx_pcie_board_t;

#define DRIVER_NAME "memx"
#define DEVICE_NODE_NAME "memx"

#define MEMRYX_VENDOR_ID 0x0559
#define MEMRYX_DEVICE_ID 0x4006

static int g_char_major = 0;


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
	int err = 0;
    int bar = 0;
    memx_pcie_bar_t bars[MAX_BAR] = {0};
    memx_pcie_board_t *pBoard = (memx_pcie_board_t *)kzalloc(sizeof(memx_pcie_board_t), GFP_KERNEL);
    if (pBoard == NULL) {
        printk(KERN_ERR DRIVER_NAME ": Probing: Failed to allocate memory for device extension structure\n");
        err = -ENOMEM;
        goto probe_exit;
    }

    // Enable the device
    if ((err = pci_enable_device(pDev))) {
        printk(KERN_ERR DRIVER_NAME ": Probing: Failed calling pci_enable_device: %0x:%0x, err(%d)\n", pDev->vendor, pDev->device, err);
        goto probe_free_board;
    }
    printk(KERN_NOTICE DRIVER_NAME ": Probing: Device enabled: %0x:%0x\n", pDev->vendor, pDev->device);

	pci_set_master(pDev);
    /* Check and configuration DMA length */
    if (!(err = pci_set_dma_mask(pDev, DMA_BIT_MASK(64)))) {
        printk(KERN_NOTICE DRIVER_NAME ": Probing: Enabled 64 bit dma for %0x:%0x\n", pDev->vendor, pDev->device);
    } else if (!(err = pci_set_dma_mask(pDev, DMA_BIT_MASK(48)))) {
        printk(KERN_NOTICE DRIVER_NAME ": Probing: Enabled 48 bit dma for %0x:%0x\n", pDev->vendor, pDev->device);
    } else if (!(err = pci_set_dma_mask(pDev, DMA_BIT_MASK(40)))) {
        printk(KERN_NOTICE DRIVER_NAME ": Probing: Enabled 40 bit dma for %0x:%0x\n", pDev->vendor, pDev->device);
    } else if ( !(err = pci_set_dma_mask(pDev, DMA_BIT_MASK(36))) ) {
        printk(KERN_NOTICE DRIVER_NAME ": Probing: Enabled 36 bit dma for %0x:%0x\n", pDev->vendor, pDev->device);
    } else if ( !(err = pci_set_dma_mask(pDev, DMA_BIT_MASK(32))) ) {
        printk(KERN_NOTICE DRIVER_NAME ": Probing: Enabled 32 bit dma for %0x:%0x\n", pDev->vendor, pDev->device);
    } else {
        printk(KERN_ERR DRIVER_NAME ": Probing: Error(%d) enabling dma for %0x:%0x\n", err, pDev->vendor, pDev->device);
        goto probe_disable_device;
    }

    /* Allocate and configure BARs */
    if ((err = pci_request_regions(pDev, DRIVER_NAME))) {
        printk(KERN_ERR DRIVER_NAME ": Probing: Error(%d) allocating bars for %0x:%0x\n", err, pDev->vendor, pDev->device);
        goto probe_disable_device;
    }
    for (bar = 0, err = 0; bar < MAX_BAR && !err; bar++) {
        bars[bar].user_address = 0;
        bars[bar].phys_address = pci_resource_start(pDev, bar);
        bars[bar].length = pci_resource_len(pDev, bar);
        bars[bar].kernel_address = pci_iomap(pDev, bar, bars[bar].length);
        bars[bar].memory_flag  = ((pci_resource_flags(pDev, bar) & IORESOURCE_MEM) != 0);
        bars[bar].active_flag = bars[bar].phys_address && bars[bar].kernel_address && bars[bar].length;
    }
    if (!(bars[BAR0].active_flag || bars[BAR1].active_flag || bars[BAR2].active_flag || bars[BAR3].active_flag || bars[BAR4].active_flag || bars[BAR5].active_flag) )
    {
        printk(KERN_ERR DRIVER_NAME ": Probing: No active bars for %0x:%0x\n", pDev->vendor, pDev->device);
        err = -ENODEV;
        goto probe_release_bars;
    }

	pBoard->pDev = pDev;
    sema_init(&pBoard->mutex, 1);
    for (bar = 0; bar < MAX_BAR; bar++) {
        pBoard->bar[bar] = bars[bar];
        printk(KERN_NOTICE DRIVER_NAME ": bar %d - %p 0x%08x %08x %llu %d %d\n", bar,
                pBoard->bar[bar].kernel_address,
                (uint32_t)((pBoard->bar[bar].phys_address>>32) & 0xFFFFFFFF),
                (uint32_t)(pBoard->bar[bar].phys_address & 0xFFFFFFFF),
                (unsigned long long)pBoard->bar[bar].length,
                pBoard->bar[bar].memory_flag,
                pBoard->bar[bar].active_flag);
    }

    pci_set_drvdata(pDev, pBoard);

	printk(KERN_DEBUG DRIVER_NAME ": PCie Probe Success.\n");

	return 0;

probe_release_bars:
    for (bar = 0; bar < MAX_BAR; bar++) {
		if (pBoard->bar[bar].active_flag) {
			pci_iounmap(pDev, pBoard->bar[bar].kernel_address);
		}
    }
    pci_release_regions(pDev);

probe_disable_device:
    pci_disable_device(pDev);

probe_free_board:
    kfree(pBoard);

probe_exit:

	return err;
}

static void memx_pcie_remove(struct pci_dev *pDev) 
{
    int bar = 0;
    memx_pcie_board_t *pBoard = (memx_pcie_board_t *)pci_get_drvdata(pDev);
	if (pBoard) {
		for (bar = 0; bar < MAX_BAR; bar++) {
			if (pBoard->bar[bar].active_flag) {
				pci_iounmap(pDev, pBoard->bar[bar].kernel_address);
			}
		}
		pci_release_regions(pDev);
		pci_disable_device(pDev);
		pci_set_drvdata(pDev, NULL);
	}
	printk(KERN_DEBUG DRIVER_NAME ": PCie Remove Success.\n");
}


long memx_pcie_fops_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long err = 0;
    uint32_t major = MAJOR(filp->f_inode->i_rdev);
    uint32_t minor = MINOR(filp->f_inode->i_rdev);

    printk(KERN_DEBUG DRIVER_NAME ": (%d-%d): fops_unlockedioctl. cmd:%d\n", major, minor, cmd);

	// ToDo: add related ioctl code here.
    switch (cmd) {
	default:
		err = 0;
		break;
    }

    printk(KERN_DEBUG DRIVER_NAME ": (%d-%d): fops_unlockedioct: SUCCESS\n", major, minor);
    return err;
}

int memx_pcie_fops_open(struct inode *inode, struct file *filp)
{
    uint32_t major = MAJOR(inode->i_rdev);
    uint32_t minor = MINOR(inode->i_rdev);

    printk(KERN_DEBUG DRIVER_NAME ": (%d-%d): fops_open SUCCESS.\n", major, minor);
	
	// ToDo: add enabling interrupts code here. 


    return 0;
}

 /* ****************************
  ******************************* */
int memx_pcie_fops_release(struct inode *inode, struct file *filp)
{
    uint32_t major = MAJOR(inode->i_rdev);
    uint32_t minor = MINOR(inode->i_rdev);

    printk(KERN_INFO DRIVER_NAME ": (%d-%d): fops_close SUCCESS.\n", major, minor);

    return 0;
}

static struct file_operations memx_pcie_fops = 
{
	owner : THIS_MODULE,
	unlocked_ioctl : memx_pcie_fops_unlocked_ioctl,
	open: memx_pcie_fops_open,
	release: memx_pcie_fops_release
};

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

	g_char_major = register_chrdev(0, DRIVER_NAME, &memx_pcie_fops);
    if (g_char_major < 0) {
        printk(KERN_ERR DRIVER_NAME ": Init Error, failed to call register_chrdev(%d).\n", err);
        return g_char_major;
    }
	err = pci_register_driver(&memx_pcie_driver);
    if (err != 0) {
        printk(KERN_ERR DRIVER_NAME ": Init Error, failed to call pci_register_driver(%d).\n", err);
		unregister_chrdev(g_char_major, DRIVER_NAME);
        return err;
    }
    printk(KERN_INFO DRIVER_NAME ": Init Success, Char Major Id(%d).\n", g_char_major);
    return 0;
}

void __exit memx_pcie_module_exit(void) 
{
	pci_unregister_driver(&memx_pcie_driver);
	unregister_chrdev(g_char_major, DRIVER_NAME);
	printk(KERN_INFO DRIVER_NAME ": PCie Exit.\n");
}

module_init(memx_pcie_module_init);
module_exit(memx_pcie_module_exit);

MODULE_AUTHOR("ZHI-WEI CHEN");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1 demo");
