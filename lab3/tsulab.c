#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/timekeeping.h>

#define PROC_FILENAME "tsulab"
#define EVENT_EPOCH_SECONDS -1940955960LL 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("New Developer");

static struct proc_dir_entry *tsu_proc_entry;

static ssize_t tsu_proc_read(struct file *file, char __user *user_buf, size_t count, loff_t *pos)
{
    if (*pos > 0)
        return 0;

    s64 current_s = ktime_get_real_seconds();
    s64 total_elapsed_s = current_s - EVENT_EPOCH_SECONDS;
    s64 total_elapsed_min = total_elapsed_s / 60;
    
    char output_buffer[128];
    int length;
    
    length = snprintf(output_buffer, sizeof(output_buffer),
        "Elapsed time since Tunguska (30/06/1908 00:14 UTC):\n%lld minutes\n",
        (long long)total_elapsed_min);

    if (length < 0 || length >= sizeof(output_buffer))
        return -EINVAL;

    if (copy_to_user(user_buf, output_buffer, length))
        return -EFAULT;

    *pos = length;
    return length;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops file_operations_tsu = 
{
.proc_read = tsu_proc_read,
};
#else
static const struct file_operations file_operations_tsu =
{
    .read = tsu_proc_read,
};
#endif

static int __init tsu_module_init(void)
{
    pr_info("Welcome to the Tomsk State University\n");
    
    tsu_proc_entry = proc_create(PROC_FILENAME, 0444, NULL, &file_operations_tsu);
    if (!tsu_proc_entry) 
    {
        pr_err("tsulab: Failed to initialize /proc/%s\n", PROC_FILENAME);
        return -ENOMEM;
    }
    
    return 0;
}

static void __exit tsu_module_exit(void)
{
    proc_remove(tsu_proc_entry);
    pr_info("Tomsk State University forever!\n"); 
}

module_init(tsu_module_init);
module_exit(tsu_module_exit);