#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>

#include "monitor_ioctl.h"

#define DEVICE_NAME "container_monitor"
#define CLASS_NAME  "container"
#define CHECK_INTERVAL_SEC 1

/* ===================== STRUCT ===================== */
struct monitor_entry {
    char container_id[32];
    pid_t pid;
    unsigned long soft_limit;
    unsigned long hard_limit;
    int warned;
    struct list_head list;
};

/* ===================== GLOBALS ===================== */
static LIST_HEAD(monitor_list);
static DEFINE_MUTEX(monitor_lock);

static dev_t dev_num;
static struct cdev c_dev;
static struct class *cl;

static struct timer_list monitor_timer;

/* ===================== HELPER FUNCTIONS ===================== */

// Get memory usage
static long get_rss_bytes(pid_t pid)
{
    struct task_struct *task;
    struct mm_struct *mm;
    long rss = -1;

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (task)
        get_task_struct(task);
    rcu_read_unlock();

    if (!task)
        return -1;

    mm = get_task_mm(task);
    if (!mm) {
        put_task_struct(task);
        return -1;
    }

    rss = get_mm_rss(mm) << PAGE_SHIFT;

    mmput(mm);
    put_task_struct(task);

    return rss;
}

// Soft limit log
static void log_soft_limit_event(const char *id, pid_t pid,
                                unsigned long soft, unsigned long usage)
{
    printk(KERN_INFO
        "[container_monitor] SOFT LIMIT exceeded: container=%s pid=%d usage=%lu soft=%lu\n",
        id, pid, usage, soft);
}

// Kill process
static void kill_process(const char *id, pid_t pid,
                        unsigned long hard, unsigned long usage)
{
    struct task_struct *task;

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (task)
        get_task_struct(task);
    rcu_read_unlock();

    if (!task)
        return;

    printk(KERN_INFO
        "[container_monitor] HARD LIMIT exceeded: Killing container=%s pid=%d usage=%lu hard=%lu\n",
        id, pid, usage, hard);

    send_sig(SIGKILL, task, 0);

    put_task_struct(task);
}

/* ===================== TIMER ===================== */
static void timer_callback(struct timer_list *t)
{
    struct monitor_entry *entry, *tmp;

    mutex_lock(&monitor_lock);

    list_for_each_entry_safe(entry, tmp, &monitor_list, list) {

        long mem = get_rss_bytes(entry->pid);

        if (mem < 0) {
            list_del(&entry->list);
            kfree(entry);
            continue;
        }

        printk(KERN_INFO "[container_monitor] PID %d memory: %ld bytes\n",
               entry->pid, mem);

        if (mem > entry->soft_limit && !entry->warned) {
            log_soft_limit_event(entry->container_id,
                                 entry->pid,
                                 entry->soft_limit,
                                 (unsigned long)mem);
            entry->warned = 1;
        }

        if (mem > entry->hard_limit) {
            kill_process(entry->container_id,
                         entry->pid,
                         entry->hard_limit,
                         (unsigned long)mem);

            list_del(&entry->list);
            kfree(entry);
        }
    }

    mutex_unlock(&monitor_lock);

    mod_timer(&monitor_timer, jiffies + CHECK_INTERVAL_SEC * HZ);
}

/* ===================== IOCTL ===================== */
static long monitor_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct monitor_request req;

    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    if (cmd == MONITOR_REGISTER) {

        struct monitor_entry *entry;

        printk(KERN_INFO
            "[container_monitor] Registering container=%s pid=%d\n",
            req.container_id, req.pid);

        entry = kmalloc(sizeof(*entry), GFP_KERNEL);
        if (!entry)
            return -ENOMEM;

        strcpy(entry->container_id, req.container_id);
        entry->pid = req.pid;
        entry->soft_limit = req.soft_limit_bytes;
        entry->hard_limit = req.hard_limit_bytes;
        entry->warned = 0;

        mutex_lock(&monitor_lock);
        list_add(&entry->list, &monitor_list);
        mutex_unlock(&monitor_lock);

        return 0;
    }

    else if (cmd == MONITOR_UNREGISTER) {

        struct monitor_entry *entry, *tmp;

        mutex_lock(&monitor_lock);

        list_for_each_entry_safe(entry, tmp, &monitor_list, list) {
            if (entry->pid == req.pid) {
                list_del(&entry->list);
                kfree(entry);
                mutex_unlock(&monitor_lock);
                return 0;
            }
        }

        mutex_unlock(&monitor_lock);
        return -ENOENT;
    }

    return -EINVAL;
}

/* ===================== FILE OPS ===================== */
static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = monitor_ioctl,
};

/* ===================== INIT ===================== */
static int __init monitor_init(void)
{
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
        return -1;

    cdev_init(&c_dev, &fops);

    if (cdev_add(&c_dev, dev_num, 1) < 0)
        return -1;

    cl = class_create(CLASS_NAME);
    device_create(cl, NULL, dev_num, NULL, DEVICE_NAME);

    timer_setup(&monitor_timer, timer_callback, 0);
    mod_timer(&monitor_timer, jiffies + CHECK_INTERVAL_SEC * HZ);

    printk(KERN_INFO "[container_monitor] Module loaded.\n");
    return 0;
}

/* ===================== EXIT ===================== */
static void __exit monitor_exit(void)
{
    struct monitor_entry *entry, *tmp;

    del_timer_sync(&monitor_timer);

    mutex_lock(&monitor_lock);

    list_for_each_entry_safe(entry, tmp, &monitor_list, list) {
        list_del(&entry->list);
        kfree(entry);
    }

    mutex_unlock(&monitor_lock);

    cdev_del(&c_dev);
    device_destroy(cl, dev_num);
    class_destroy(cl);
    unregister_chrdev_region(dev_num, 1);

    printk(KERN_INFO "[container_monitor] Module unloaded.\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");



