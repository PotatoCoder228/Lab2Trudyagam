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
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linux module for OS Lab2");
MODULE_VERSION("1.0");

#define WR_VALUE _IOW('w', 0xEEE, int32_t *)
#define RD_VALUE _IOR('r', 0xFFF, int32_t *)

int32_t value = 0;

dev_t dev = 666;
static struct class *dev_class;
static struct cdev etx_cdev;

static int __init blk_init(void);
static void __exit blk_exit(void);
static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = etx_open,
    .unlocked_ioctl = etx_ioctl,
    .release = etx_release,
};

#define module_name "lab2_iostat"

static atomic_t already_open = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(waitq);

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
  return 0;
}

static int etx_release(struct inode *inode, struct file *file) {
  atomic_set(&already_open, 0);
  wake_up(&waitq);
  module_put(THIS_MODULE);
  pr_info("Device File Closed...!!!\n");
  return 0;
}

static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  const __u64 dev_name_size = 20;
  char device_path[dev_name_size];
  char buff[dev_name_size];
  int res = copy_from_user(&device_path, arg, sizeof(20));
  strcpy(device_path, buff);

  switch (cmd) {
  case RD_VALUE:
    pr_info("Statistics collecting started\n");
    struct block_device *nvme =
        blkdev_get_by_path(device_path, FMODE_READ, NULL);
    if (IS_ERR(nvme)) {
      pr_info("Statistics collecting finished on the blkdev getting\n");
      return 0;
    }

    struct request_queue *q = NULL;
    q = bdev_get_queue(nvme);
    if (q == NULL) {
      pr_info("Statistics collecting finished on the request_queue getting\n");
      return 0;
    }

    const __u64 sector_sz = queue_physical_block_size(q);

    part_stat_lock();
    __u64 start_rd_iops = part_stat_read(nvme, ios[STAT_READ]);
    __u64 start_rd_sectors = part_stat_read(nvme, sectors[STAT_READ]);
    __u64 start_wr_iops = part_stat_read(nvme, ios[STAT_WRITE]);
    __u64 start_wr_sectors = part_stat_read(nvme, sectors[STAT_WRITE]);
    part_stat_unlock();

    ssleep(1);

    part_stat_lock();
    __u64 finish_rd_iops = part_stat_read(nvme, ios[STAT_READ]);
    __u64 finish_rd_sectors = part_stat_read(nvme, sectors[STAT_READ]);
    __u64 finish_wr_iops = part_stat_read(nvme, ios[STAT_WRITE]);
    __u64 finish_wr_sectors = part_stat_read(nvme, sectors[STAT_WRITE]);
    part_stat_unlock();

    finish_rd_iops = finish_rd_iops - start_rd_iops;
    finish_rd_sectors = finish_rd_sectors - start_rd_sectors;
    finish_wr_iops = finish_wr_iops - start_wr_iops;
    finish_wr_sectors = finish_wr_sectors - start_wr_sectors;

    __u64 result[4] = {finish_rd_iops, finish_rd_sectors * sector_sz,
                       finish_wr_iops, finish_wr_sectors * sector_sz};
    res = copy_to_user(arg, &result, sizeof(result));
    pr_info("Statistics collecting completed. Code: %i\n", res);
    break;
  default:
    pr_warn("Unexpected code\n");
    break;
  }
  return 0;
}

static int __init blk_init(void) {
  if ((alloc_chrdev_region(&dev, 0, 1, "etx_Dev")) < 0) {
    pr_err("Cannot allocate major number\n");
    return -1;
  }
  pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

  cdev_init(&etx_cdev, &fops);

  if ((cdev_add(&etx_cdev, dev, 1)) < 0) {
    pr_err("Cannot add the device to the system\n");
    goto r_class;
  }

  if (IS_ERR(dev_class = class_create(THIS_MODULE, "etx_class"))) {
    pr_err("Cannot create the struct class\n");
    goto r_class;
  }

  if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "etx_device"))) {
    pr_err("Cannot create the Device 1\n");
    goto r_device;
  }
  pr_info("Device Driver %s Insert...Done!!!\n", module_name);
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
  pr_info("Device Driver %s Remove...Done!!!\n", module_name);
}

module_init(blk_init);
module_exit(blk_exit);
