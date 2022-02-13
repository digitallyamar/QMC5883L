#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

static int __init qmc5883_init(void)
{
	pr_info("qmc5883_init++\n");

	return 0;
}

static void __exit qmc5883_exit(void)
{
	pr_info("qmc5883_exit--\n");
}

module_init(qmc5883_init);
module_exit(qmc5883_exit);

MODULE_AUTHOR("Amarnath Revanna");
MODULE_DESCRIPTION("QMC5883 Core Driver");
MODULE_LICENSE("GPL");
