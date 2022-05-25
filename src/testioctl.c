#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/delay.h>
#define PWN_FREE _IO('p', 1)
#define PWN_ALLOC _IO('p', 2)

MODULE_LICENSE("GPL"); 

struct th_t {
	uint8_t task;
	uint32_t a2d_size;
	uint32_t d2a_size;
};

struct audio_region_t {
	void* offset;
	uint32_t size;
	//uint8_t resv_128[120];
	uint32_t read_idx;
	//uint8_t resv_256[124];
	uint32_t write_idx;
	
};

#define TASK_SCENE_SIZE 5

struct audio_ipi_dma_t {
	 /*struct aud_ptr_t base_phy;
	 struct aud_ptr_t base_vir;
	 uint32_t size; 
	 uint8_t  resv_128[108];*/
	 struct audio_region_t region[TASK_SCENE_SIZE][2]; 
	 //uint32_t checksum;
	 //uint32_t pool_offset;
};

#define NUM_OPENDSP_TYPE 5 // idk actual value
static struct audio_ipi_dma_t *g_dma[NUM_OPENDSP_TYPE];
static struct gen_pool *g_dma_pool[NUM_OPENDSP_TYPE];
static uint8_t g_cache_alilgn_order[NUM_OPENDSP_TYPE];
int id = 0;

static int device_open(struct inode *inode, struct file *filp)
{
    	printk(KERN_ALERT "Device opened.\n");
  	return 0;
}

static int device_release(struct inode *inode, struct file *filp)
{
    	printk(KERN_ALERT "Device closed.\n");
  	return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
	return -EINVAL;
}

static ssize_t device_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
  	return -EINVAL;
}

int free_region(const uint8_t task)
{
	if(g_dma[task] == NULL) {
		printk(KERN_ALERT "g_dma %d invalid\n", id);
		return -ENODEV;
	}
	size_t i;
	struct audio_region_t* region = NULL;
	//printk(KERN_ALERT "free region task %lx\n", task);
	for(i = 0; i < 2; i++) {
		region = &(g_dma[task]->region[task][i]);
		// make race more stable(also why i commented out checks)
		mdelay(50);
		//printk(KERN_ALERT "free region %lx %d\n", region, region->size);
		/*if(region->size == 0 || region->offset == NULL) {
			printk(KERN_ALERT "null pointer or size\n");
			break;
		}*/
		gen_pool_free(g_dma_pool[task], 
				region->offset,
				region->size);
		region->offset = 0;
		region->size = 0;
	/*	region->read_idx = 0;
		region->write_idx = 0;
	*/	
	}
	return 0;	
}

int alloc_region(const uint8_t task, uint32_t sizea, uint32_t sizeb) {
	uint32_t size[2] = {sizea, sizeb};
	size_t i = 0;
	struct audio_region_t* region = NULL;
	uint32_t dsp_id = task;  // audio_get_dsp_id(task);
	unsigned long new_addr = 0;
	phys_addr_t phy_value = 0;
	if(g_dma[dsp_id] == NULL) {
		printk(KERN_ALERT "index %d is null\n", dsp_id);
		return -ENODEV;
	}
	for(i = 0; i < 2; i++) {
		region = &g_dma[dsp_id]->region[task][i];
	 	if(region == NULL) {
			printk(KERN_ALERT "region is NULL\n");
			return -ENODEV;
		}
		new_addr = gen_pool_alloc(g_dma_pool[dsp_id], size[i]);
		if(new_addr == NULL) { 
			printk(KERN_ALERT "gen_pool_alloc failed\n"); 
			return -ENOMEM;
		}
		region->offset =  new_addr;//phy_addr_to_offset(phy_value);
		region->size = size[i];
		/*region->read_idx = 0;
		region->write_idx = 0;
		mdelay(50);*/
		printk(KERN_ALERT "alloc region %lx %d\n", region, region->size);
	}
	return 0;
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	 struct th_t thing;
	copy_from_user(&thing, (void __user*)arg, sizeof(struct th_t));
	switch(cmd) {
		case PWN_ALLOC: 
			ret = alloc_region(thing.task, thing.a2d_size, thing.d2a_size);
			break;
		case PWN_FREE:
		default:
			ret = free_region(thing.task); break;
	}        
	return ret;
}

int init_audio_ipi_dma(uint32_t dsp_id) {
	if (g_dma[dsp_id] != NULL) {
		printk(KERN_ALERT "dsp_id %d already set\n", dsp_id);
		g_dma[dsp_id] = NULL;
		return -ENODEV;
	}
	if (dsp_id % 2) {
		g_cache_alilgn_order[dsp_id] = 5;
	} else {
		 g_cache_alilgn_order[dsp_id] =	7;
	}
	// mt6853/audio_ipi_platform.c gives 5 or 7 get_cache_aligned_order(dsp_id);
	g_dma_pool[dsp_id] = 
		 gen_pool_create(5, -1);
	if(g_dma_pool[dsp_id] == NULL) {
		printk(KERN_ALERT "gen_pool_create fail");
		return -ENOMEM;
	}
	size_t size = 0x2000;
	unsigned long addr = kzalloc(size, GFP_KERNEL);
	if(addr == NULL) { printk(KERN_ALERT "kzalloc returned null\n"); return -1; }
	gen_pool_add(g_dma_pool[dsp_id], addr, size, -1);
	g_dma[dsp_id] = (struct audio_ipi_dma_t *)
			kzalloc(sizeof(struct audio_ipi_dma_t), GFP_KERNEL);
	return 0;
}

int deinit_audio_ipi_dma(uint32_t dsp_id) {
	if (g_dma[dsp_id] == NULL)
		return 0;
	
	if (g_dma_pool[dsp_id] != NULL) {
		gen_pool_destroy(g_dma_pool[dsp_id]);
		g_dma_pool[dsp_id] = NULL;
	}
	g_dma[dsp_id] = NULL;
	return 0;
}

static struct file_operations fops = {
  	.read = device_read,
  	.write = device_write,
  	.unlocked_ioctl = device_ioctl,
  	.open = device_open,
  	.release = device_release
};

struct proc_dir_entry *proc_entry = NULL;

int init_module(void)
{
	//printk(KERN_ALERT "ioctl address: %#lx\b", (unsigned long)device_ioctl);
	printk(KERN_ALERT "PWN_ALLOC value: %#x\b", PWN_ALLOC);
	printk(KERN_ALERT "PWN_FREE value: %#x\b", PWN_FREE);
    	proc_entry = proc_create("test-ioctl", 0666, NULL, &fops);
	uint32_t dsp_id;
	for(dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) {
		init_audio_ipi_dma(dsp_id);
	}
	printk(KERN_ALERT "done creating gen_pools\n");
	return 0;
}

void cleanup_module(void)
{
	uint32_t dsp_id;
	 for(dsp_id = 0; dsp_id < NUM_OPENDSP_TYPE; dsp_id++) { 
		deinit_audio_ipi_dma(dsp_id);
	}
	if (proc_entry) proc_remove(proc_entry);
}

