#include <linux/fs.h>
extern const struct file_operations sfops;
extern ssize_t sdevice_read(struct file * file, char * buf, size_t requestedCount, loff_t *ppos);
extern ssize_t sdevice_write(struct file *file, const char __user *buf, size_t receivedCount, loff_t *ppos);
