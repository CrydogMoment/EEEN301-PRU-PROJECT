/**
 * @file   rootkit.c
 * @author Riley & Zuni
 * @date   7 April 2026
 * @version 0.1
 * @brief   An introductory character driver to support the second article of my series on
 * Linux loadable kernel module (LKM) development. This module maps to /dev/ebbchar and
 * comes with a helper C program that can be run in Linux user space to communicate with
 * this the LKM.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
 */

#include <linux/init.h>             // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/device.h>           // Header to support the kernel Driver Model
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel
#include <linux/fs.h>               // Header for the Linux file system support
#include <linux/uaccess.h>          // Required for the copy to user function
#include <linux/kmod.h>             //required for running shell scripts
#include <linux/io.h>               //required for writing to RAM
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/uio_driver.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define PRUSS_UIO_DEVNAME "pruss_uio"

#define  DEVICE_NAME "rootkit"      ///< The device will appear at /dev/ebbchar using this value
#define  CLASS_NAME  "ebb"          ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");              ///< The license type -- this affects available functionality
MODULE_AUTHOR("Riley&Zuni");        ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver for the BBB");
MODULE_VERSION("0.1");              ///< A version number to inform users

static int majorNumber;                     // Stores the device number -- determined automatically
static int numberOpens = 0;                 // Counts the number of times the device is opened
static struct class*  ebbcharClass  = NULL; // The device-driver class struct pointer
static struct device* ebbcharDevice = NULL; // The device-driver device struct pointer
static unsigned int gpio_interrupt = 48;     // P9_15 pin that goes low at the end of PRU program
static unsigned int irq_number;              // share IRQ num within file

// Prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static unsigned int dev_poll(struct file *file, poll_table *wait);

// prototype for the custom IRQ handler function, function below
static irq_handler_t ebb_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

//added for writing to shared RAM
#define PRUSS_SHARED_RAM_PADDR 0x4A300000
#define PRUSS_SHARED_RAM_SIZE  0x3000 // 12 KB

void __iomem *shared_ram_vaddr;

static DECLARE_WAIT_QUEUE_HEAD(rootkit_wait);
static int data_ready = 0;
static int recieved_num_sensors = 0;

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to be associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
   .poll = dev_poll,
};

static unsigned int dev_poll(struct file *file, poll_table *wait) {
    printk(KERN_INFO "rootkit: poll");
    poll_wait(file, &rootkit_wait, wait);

    // you should return POLLIN | POLLRDNORM if you have some new data to read, and 0 in case there is no new data to read
    if (data_ready == 1) {
        printk(KERN_INFO "rootkit: Data ready!");
        data_ready = 0;
        return POLLIN | POLLRDNORM;
    }
    printk(KERN_INFO "rootkit: Poll return 0");
    return 0;
}

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init ebbchar_init(void){
    int result;
   printk(KERN_INFO "rootkit: Initializing the rootkit LKM\n");

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ERR "rootkit: failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "rootkit: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ebbcharClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ERR "rootkit: Failed to register device class\n");
      return PTR_ERR(ebbcharClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "rootkit: device class registered correctly\n");

   // Register the device driver
   ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ebbcharDevice)){               // Clean up if there is an error
      class_destroy(ebbcharClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ERR "rootkit: Failed to create the device\n");
      return PTR_ERR(ebbcharDevice);
   }

   // Map physical address to kernel virtual address
   shared_ram_vaddr = ioremap(PRUSS_SHARED_RAM_PADDR, PRUSS_SHARED_RAM_SIZE);

   if (!shared_ram_vaddr) {
      printk(KERN_ERR "rootkit: Failed to map shared RAM\n");
      return -ENOMEM;
   }

   printk(KERN_INFO "rootkit: button value is currently: %d\n",
   gpio_get_value(gpio_interrupt));
   irq_number = gpio_to_irq(gpio_interrupt); // map GPIO to IRQ number
   printk(KERN_INFO "rootkit: button mapped to IRQ: %d\n", irq_number);

   // This next call requests an interrupt line
   result = request_irq(irq_number,                  // interrupt number requested
            (irq_handler_t) ebb_gpio_irq_handler,   // handler function
            IRQF_TRIGGER_FALLING,                    // on rising edge (press, not release)
            "ebb_gpio_handler",                     // used in /proc/interrupts
            NULL);                                  // *dev_id for shared interrupt lines
   printk(KERN_INFO "rootkit: IRQ request result is: %d\n", result);
   return result;
}

static irq_handler_t ebb_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs) {
    printk(KERN_INFO "rootkit: PRU Interrupt Picked up by LKM!\n");
   data_ready = 1;
   wake_up(&rootkit_wait);

    return (irq_handler_t) IRQ_HANDLED;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit ebbchar_exit(void){
   if (shared_ram_vaddr) {
      iounmap(shared_ram_vaddr);
   }

   free_irq(irq_number, NULL);
   device_destroy(ebbcharClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(ebbcharClass);                          // unregister the device class
   class_destroy(ebbcharClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "rootkit: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "rootkit: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the samples array
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    printk(KERN_INFO "rootkit: Read\n");

    u32 vals[len];
    int i = 0;
    while (i < len) {
        vals[i] = readl(shared_ram_vaddr + (i * 4) + 12);
        i ++;
    }
    error_count = copy_to_user(buffer, vals, len*4);

    if (error_count == 0) {
        printk(KERN_INFO "rootkit: Sent data to the user\n");
        return len + 1; // Return bytes read so the user space program knows data arrived
    } else {
        printk(KERN_ALERT "rootkit: Failed to send characters to the user\n");
        return -EFAULT;
    }
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char kbuf[16]; // Temporary buffer to hold the input string
    int user_value;
    int error_count;

    // prevent buffer overflows
    if (len > sizeof(kbuf) - 1) {
        printk(KERN_ALERT "rootkit: Input too long\n");
        return -EINVAL;
    }

    // copy data from user space to kernel space
    error_count = copy_from_user(kbuf, buffer, len);
    if (error_count != 0) {
        printk(KERN_ALERT "rootkit: Failed to copy data from user\n");
        return -EFAULT;
    }

    //  null-terminate the string so parsing functions work safely
    kbuf[len] = '\0';

    //    Convert the ASCII string into an actual integer
    //    kstrtoint handles white spaces and returns 0 on success
    if (kstrtoint(kbuf, 10, &user_value) != 0) {
        printk(KERN_ALERT "rootkit: Invalid integer string received\n");
        return -EINVAL;
    }

    // write the actual integer value to the PRU Shared RAM
    if (shared_ram_vaddr) {
        if(recieved_num_sensors != 0){
            writel(user_value, shared_ram_vaddr);
            printk(KERN_INFO "rootkit: Wrote integer %d to Shared RAM address: %pa\n", user_value, shared_ram_vaddr);
        }else{
            writel(user_value, shared_ram_vaddr + 8);
            printk(KERN_INFO "rootkit: Wrote integer %d to Shared RAM address: %pa\n", user_value, shared_ram_vaddr + 8);
            recieved_num_sensors = user_value;
        }
        
    } else {
        printk(KERN_ALERT "rootkit: Shared RAM pointer is NULL\n");
        return -EIO;
    }

    return len; // Return the number of bytes handled
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "rootkit: Device successfully closed\n");
   return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(ebbchar_init);
module_exit(ebbchar_exit);
