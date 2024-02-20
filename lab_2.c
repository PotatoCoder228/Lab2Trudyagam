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

dev_t dev = 0;
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

  pr_info("Getting the reading speed\n");
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
    if (copy_from_user(&value, (int32_t *)arg, sizeof(value))) {
      pr_err("Data Write : Err!\n");
    }
    pr_info("Value = %d\n", value);
    break;
  case RD_VALUE:
    if (copy_to_user((int32_t *)arg, &value, sizeof(value))) {
      pr_err("Data Read : Err!\n");
    }
    break;
  default:
    pr_info("Default\n");
    break;
  }
  return 0;
}

// static ssize_t procfs_read(struct file *file, char __user *buffer,
//                            size_t length, loff_t *offset) {
//   // semaphore
//   printk(KERN_NOTICE "procfs_read: procfs_buffer=%s\n", procfs_buffer);
//   static int finished = 0;
//   procfs_buffer_size = PROCFS_MAX_SIZE - 1;
//   if (finished) {
//     pr_debug("procfs_read: END\n");
//     finished = 0;
//     return 0;
//   }
//   finished = 1;
//   pid_t pid = 0;
//   char number[PROCFS_MAX_SIZE] = {0};
//   strcpy(number, procfs_buffer);
//   kstrtol(number, 10, (long *)&pid);
//   struct task_struct *task = pid_task(find_vpid(pid), PIDTYPE_PID);
//   struct cpu_itimer tmr = {.expires = 0};
//   if (task != NULL) {
//     printk(KERN_NOTICE "procfs_read: task_struct is not null!\n");
//     if (task->signal != NULL) {
//       printk(KERN_NOTICE "procfs_read: task_struct->signal is not null!\n");
//       tmr = task->signal->it[0];
//     } else {
//       printk(KERN_NOTICE "procfs_read: task_struct->signal is null!\n");
//       return 0;
//     }
//   } else {
//     printk(KERN_NOTICE "procfs_read: task_struct is null");
//     return 0;
//   }
//   char str[100] = {0};
//   snprintf(str, 100, "\ncpu_itime: expires=%llu, incr=%llu\n", tmr.expires,
//            tmr.incr);
//   strcat(procfs_buffer, str);
//   struct mm_struct *mm = task->mm;
//   unsigned long address = get_unmapped_area(file, 0, 10, 0, 0);
//   struct vm_area_struct *vma = find_vma(mm, address);
//   printk(KERN_NOTICE "procfs_read: vm_area_struct ptr %llu\n", vma);
//   snprintf(str, 200,
//            "\nvm_area_struct: vm_start=%llu, vm_end=%llu, vm_mm ptr=%llu\n",
//            vma->vm_start, vma->vm_end, vma->vm_mm);
//   strcat(procfs_buffer, str);
//   printk(KERN_NOTICE "%s", procfs_buffer);
//   if (copy_to_user(buffer, procfs_buffer, procfs_buffer_size))
//     return -EFAULT;
//   printk(KERN_NOTICE "procfs_read: read %lu bytes\n", procfs_buffer_size);

//   // printk(KERN_NOTICE "procfs_read: vm_area_struct ptr
//   %llu\n",current->mm); return procfs_buffer_size;
// }

// static int procfs_open(struct inode *inode, struct file *file) {
//   if ((file->f_flags & O_NONBLOCK) && atomic_read(&already_open))
//     return -EAGAIN;
//   try_module_get(THIS_MODULE);
//   while (atomic_cmpxchg(&already_open, 0, 1)) {
//     int i, is_sig = 0;
//     wait_event_interruptible(waitq, !atomic_read(&already_open));
//     for (i = 0; i < _NSIG_WORDS && !is_sig; i++)
//       is_sig = current->pending.signal.sig[i] & ~current->blocked.sig[i];
//     if (is_sig) {
//       module_put(THIS_MODULE);
//       return -EINTR;
//     }
//   }
//   return 0; /* Разрешение доступа. */
// }
// static int procfs_close(struct inode *inode, struct file *file) {
//   atomic_set(&already_open, 0);
//   wake_up(&waitq);
//   module_put(THIS_MODULE);
//   return 0; /* Успех. */
// }

// static ssize_t procfs_write(struct file *file, const char __user *buffer,
//                             size_t len, loff_t *off) {
//   memset(procfs_buffer, 0, PROCFS_MAX_SIZE);
//   procfs_buffer_size = len;
//   if (procfs_buffer_size > PROCFS_MAX_SIZE)
//     procfs_buffer_size = PROCFS_MAX_SIZE;
//   if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size))
//     return -EFAULT;
//   procfs_buffer[procfs_buffer_size & (PROCFS_MAX_SIZE - 1)] = '\0';
//   pr_info("procfile write %s\n", procfs_buffer);
//   return procfs_buffer_size;
// }

// static const struct proc_ops proc_file_fops = {.proc_read = procfs_read,
//                                                .proc_write = procfs_write,
//                                                .proc_open = procfs_open,
//                                                .proc_release = procfs_close};

// static int __init procfs_init(void) {
//   our_proc_file = proc_create(procfs_name, 0777, NULL, &proc_file_fops);
//   if (NULL == our_proc_file) {
//     proc_remove(our_proc_file);
//     pr_alert("Error:Could not initialize /proc/%s\n", procfs_name);
//     return -ENOMEM;
//   }
//   proc_set_user(our_proc_file, GLOBAL_ROOT_UID, GLOBAL_ROOT_GID);
//   pr_info("/proc/%s created\n", procfs_name);
//   return 0;
// }
// static void __exit procfs_exit(void) {
//   proc_remove(our_proc_file);
//   pr_info("/proc/%s removed\n", procfs_name);
// }

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
