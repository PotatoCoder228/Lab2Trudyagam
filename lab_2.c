#include <asm/io.h>
#include <linux/blk-cgroup.h>
#include <linux/blkdev.h>
#include <linux/blktrace_api.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/part_stat.h>
#include <linux/percpu.h>
#include <linux/pid.h>
#include <linux/pid_namespace.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/slab.h> //kmalloc()
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/uaccess.h> //copy_to/from_user()
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linux module for OS Lab2");
MODULE_VERSION("1.0");

#define WR_VALUE _IOW('w', 0xEEE, int32_t *)
#define RD_VALUE _IOR('r', 0xFFF, int32_t *)

int32_t value = 0;

dev_t dev = 33333;
static struct class *dev_class;
static struct cdev etx_cdev;
/*
** Function Prototypes
*/
static int __init blk_init(void);
static void __exit blk_exit(void);
static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len,
                        loff_t *off);
static ssize_t etx_write(struct file *filp, const char *buf, size_t len,
                         loff_t *off);
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
/*
** File operation structure
*/
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = etx_read,
    .write = etx_write,
    .open = etx_open,
    .unlocked_ioctl = etx_ioctl,
    .release = etx_release,
};

// #define PROCFS_MAX_SIZE 200
#define ioctl_name "lab2_blktrace"

static atomic_t already_open = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(waitq);

/*
** This function will be called when we open the Device file
*/
static int etx_open(struct inode *inode, struct file *file) {
  if ((file->f_flags & O_NONBLOCK) && atomic_read(&already_open))
    return -EAGAIN;
  try_module_get(THIS_MODULE);
  while (atomic_cmpxchg(&already_open, 0, 1)) {
    int i, is_sig = 0;
    wait_event_interruptible(waitq, !atomic_read(&already_open));
    for (i = 0; i < _NSIG_WORDS && !is_sig; i++)
      is_sig = current->pending.signal.sig[i] & ~current->blocked.sig[i];
    if (is_sig) {
      module_put(THIS_MODULE);
      return -EINTR;
    }
  }
  return 0; /* Разрешение доступа. */
}
/*
** This function will be called when we close the Device file
*/
static int etx_release(struct inode *inode, struct file *file) {
  atomic_set(&already_open, 0);
  wake_up(&waitq);
  module_put(THIS_MODULE);
  pr_info("Device File Closed...!!!\n");
  return 0;
}
/*
** This function will be called when we read the Device file
*/
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len,
                        loff_t *off) {
  return 0;
}
/*
** This function will be called when we write the Device file
*/
static ssize_t etx_write(struct file *filp, const char __user *buf, size_t len,
                         loff_t *off) {
  pr_info("Getting the writing speed\n");
  return len;
}

/*
** This function will be called when we write IOCTL on the Device file
*/
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  switch (cmd) {
  case WR_VALUE:
    pr_info("Writing Mock\n");
    break;
  case RD_VALUE:
    pr_info("Reading is started\n");
    struct block_device *nvme;
    nvme = blkdev_get_by_path("/dev/nvme0n1p6", FMODE_READ, NULL);
    if (IS_ERR(nvme)) {
      pr_info("Reading is finished on blk getting\n");
      return 0;
    }
    // struct request_queue *q = NULL;
    // q = bdev_get_queue(nvme);
    // if (q == NULL) {
    //   pr_info("Reading is finished on request_queue\n");
    //   return 0;
    // }
    // struct request *rq = 0xFFF;
    // blk_mq_quiesce_queue(q);
    part_stat_lock();
    __u64 start_iops = part_stat_read(nvme, ios[STAT_READ]);
    __u64 start_sectors = part_stat_read(nvme, sectors[STAT_READ]);
    part_stat_unlock();

    // rq = q->last_merge;
    // pr_info("Hop...\n");
    // if (rq == NULL) {
    //   pr_info("Last merge is nullptr, shutdown...\n");
    //   blk_mq_unquiesce_queue(q);
    //   return 0;
    // }
    // do {
    //   if (rq_data_dir(rq) == READ) {
    //     r_bites += blk_rq_bytes(rq);
    //   }
    //   rq = rq->rq_next;
    // } while (rq != NULL);
    // blk_mq_unquiesce_queue(q);
    // pr_info("Getting the reading speed. Result: r_bites=%llu", r_bites);
    // r_bites = blk_queue_depth(q);
    ssleep(1);
    part_stat_lock();
    __u64 finish_iops = part_stat_read(nvme, ios[STAT_READ]);
    __u64 finish_sectors = part_stat_read(nvme, sectors[STAT_READ]);
    part_stat_unlock();
    finish_iops = finish_iops - start_iops;
    finish_sectors = finish_sectors - start_sectors;
    __u64 result[2] = {finish_iops, finish_sectors * 4};
    int res = copy_to_user(arg, &result, sizeof(result));
    pr_info("Reading is finished. Code: %i\n", res);
    break;
  default:
    pr_info("Default\n");
    break;
  }
  return 0;
}

static int __init blk_init(void) {
  /*Allocating Major number*/
  if ((alloc_chrdev_region(&dev, 0, 1, "etx_Dev")) < 0) {
    pr_err("Cannot allocate major number\n");
    return -1;
  }
  pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

  /*Creating cdev structure*/
  cdev_init(&etx_cdev, &fops);

  /*Adding character device to the system*/
  if ((cdev_add(&etx_cdev, dev, 1)) < 0) {
    pr_err("Cannot add the device to the system\n");
    goto r_class;
  }

  /*Creating struct class*/
  if (IS_ERR(dev_class = class_create(THIS_MODULE, "etx_class"))) {
    pr_err("Cannot create the struct class\n");
    goto r_class;
  }

  /*Creating device*/
  if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "etx_device"))) {
    pr_err("Cannot create the Device 1\n");
    goto r_device;
  }
  pr_info("Device Driver %s Insert...Done!!!\n", ioctl_name);
  return 0;

r_device:
  class_destroy(dev_class);
r_class:
  unregister_chrdev_region(dev, 1);
  return -1;
}

static void __exit blk_exit(void) {
  device_destroy(dev_class, dev);
  class_destroy(dev_class);
  cdev_del(&etx_cdev);
  unregister_chrdev_region(dev, 1);
  pr_info("Device Driver %s Remove...Done!!!\n", ioctl_name);
}

module_init(blk_init);
module_exit(blk_exit);
