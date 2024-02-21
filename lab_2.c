#define CONFIG_BLK_DEV_IO_TRACE

#include <asm/io.h>
#include <linux/blk-cgroup.h>
#include <linux/blkdev.h>
#include <linux/blktrace_api.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
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

#define TRACER_VALUE _IO('T', 0xEEE)
#define BLK_NAME "lab2_blktracer"
#define BLK_MAJOR 240

static atomic_t already_open = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(waitq);

static int blk_open(struct block_device *blk_device, fmode_t mode) {
  if (atomic_read(&already_open))
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
  pr_info("Device file was opened!");
  return 0;
}

static void blk_release(struct gendisk *gnd, fmode_t mode) {
  atomic_set(&already_open, 0);
  wake_up(&waitq);
  module_put(THIS_MODULE);
  pr_info("Device file was closed...!!!\n");
}

int blk_compat_ioctl(struct block_device * blk_device, fmode_t mode,  unsigned int placeholder_,  long unsigned int placeholder__) {
  return 0;
}

int blk_getgeo(struct block_device *blk_device, struct hd_geometry *hdg) {
  return 0;
}

int blk_ioctl(struct block_device *blk_device, fmode_t mode,
              unsigned cmd, unsigned long arg) {
  pr_info("ioctl request to tracer driver!");
  switch (cmd){
    case TRACER_VALUE:{
      __u64 iters = 0;
      int res = copy_from_user(&iters, (void __user *) arg, sizeof(iters));
      if(!res) return -1;
      blk_trace_ioctl(blk_device, BLKTRACESETUP, arg);
      blk_trace_ioctl(blk_device, BLKTRACESTART, arg);
      blk_trace_ioctl(blk_device, BLKTRACESTOP, arg);
      blk_trace_ioctl(blk_device, BLKTRACETEARDOWN, arg);
      break;
    }
  }
  return 0;
}

static struct block_device_operations fops = {
    .owner = THIS_MODULE,
    .open = blk_open,
    .release = blk_release,
    .ioctl = blk_ioctl,
    .compat_ioctl = blk_compat_ioctl,
    .getgeo = blk_getgeo,
};

static int __init blk_init(void) {
  int status;

  status = register_blkdev(BLK_MAJOR, BLK_NAME);
  if (status < 0) {
    printk(KERN_ERR "unable to register mybdev block device\n");
    return -EBUSY;
  }
  pr_info("Device Driver %s Install...Done!!!\n", BLK_NAME);
  return 0;
}

static void __exit blk_exit(void) {
  unregister_blkdev(BLK_MAJOR, BLK_NAME);
  pr_info("Device Driver %s Remove...Done!!!\n", BLK_NAME);
}

module_init(blk_init);
module_exit(blk_exit);
