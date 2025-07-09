/**
 * @file my_kernel.c
 * 
 * @author Anhton Nguyen
 * 
 **/

#include <linux/module.h>     /* Needed by all modules */
#include <linux/kernel.h>     /* Needed for KERN_INFO */
#include <linux/init.h>       /* Needed for the macros */
#include <linux/list.h>       /* Needed for linked list */
#include <linux/slab.h>       /* Needed for kmalloc */
#include <linux/fs.h>         /* Needed for major and minor */
#include <linux/kdev_t.h>     /* Needed for making device file */
#include <linux/device.h>     /* Needed for making device file */
#include <linux/cdev.h>       /* Needed for cdev */
#include <linux/uaccess.h>    /* Needed for copy_to_user() and copy_from_user() */
#include <linux/list.h>       /* Needed for linked list*/
#include <linux/mutex.h>      /* Needed for mutex */
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/time.h>  

#define MEM_SIZE     1024
#define DEVICE_FILE  "my_kernel"
#define PIN_17            17            /* Racer 1 */
#define PIN_27            27            /* Racer 2 */
#define PIN_23            23            /* Racer 3 */
#define PIN_24            24            /* Racer 4 */
#define PIN_17_IRQ_FILE   "pin_17_irq"
#define PIN_27_IRQ_FILE   "pin_27_irq"
#define PIN_23_IRQ_FILE   "pin_23_irq"
#define PIN_24_IRQ_FILE   "pin_24_irq"


int lane_num = 1;
dev_t dev = 0;
struct mutex my_mutex;
static struct class *dev_class;
static struct cdev my_cdev;
uint8_t *kernel_buffer;
unsigned char race_start = 0;
unsigned int pin_17_irq_number;
unsigned int pin_27_irq_number;
unsigned int pin_23_irq_number;
unsigned int pin_24_irq_number;
unsigned long racer_1_index = 0;
unsigned long racer_2_index = 0;
unsigned long racer_3_index = 0;
unsigned long racer_4_index = 0;
unsigned long debounce_jiffies = 50; //0.5s
unsigned long race_start_jiffies;
volatile unsigned long current_jiffies;
volatile unsigned long previous_jiffies;
LIST_HEAD(head);

static struct node{
	char *bib_number;
	int  lane_number;
	unsigned long lap_time[4];
	unsigned long race_time;
	struct list_head list;
};

/* Function prototypes */
static int __init my_driver_init(void);
static void __exit my_driver_exit(void);
static int my_open(struct inode *inode, struct file *file);
static int my_release(struct inode *inode, struct file *file);
static ssize_t my_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t my_write(struct file *filp, const char *buf, size_t len, loff_t *off);
static void display(struct list_head *head);
static void init_node(struct node *n, char *_bib_num);
static void insert_entry(struct list_head *head, struct node *n);
static void delete_entry(struct list_head *head, struct node *n);


/* gpio isr */
static irq_handler_t gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_reg *regs){
    current_jiffies = (unsigned long)jiffies;

    /* check if race even started */
    if (!race_start){
        pr_info("Race hasn't started\n");
        return (irq_handler_t)IRQ_HANDLED;
    }

    /* checks for button debouncing */
    if ((current_jiffies - previous_jiffies) < debounce_jiffies){
        //pr_info("debounce: %lu - %lu = %lu\n", current_jiffies, previous_jiffies, current_jiffies - previous_jiffies);
        return (irq_handler_t)IRQ_HANDLED;
    }

    /* inserts button-time since race starts into racer linked-list */
    struct node *temp;
    int count = 1;
    list_for_each_entry(temp, &head, list){
        if (count == 1){
            if (irq == pin_17_irq_number){
                previous_jiffies = current_jiffies;
                temp->lap_time[racer_1_index++] = current_jiffies - race_start_jiffies;
                pr_info("racer_1_lap_time: [%lu, %lu, %lu, %lu]\n", temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3]);
                return (irq_handler_t)IRQ_HANDLED;
            }
        }
        else if (count == 2){
            if (irq == pin_27_irq_number){
                previous_jiffies = current_jiffies;
                temp->lap_time[racer_2_index++] = current_jiffies - race_start_jiffies;
                pr_info("racer_2_lap_time: [%lu, %lu, %lu, %lu]\n", temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3]);
                return (irq_handler_t)IRQ_HANDLED;
            }
        }
        if (count == 3){
            if (irq == pin_23_irq_number){
                previous_jiffies = current_jiffies;
                temp->lap_time[racer_3_index++] = current_jiffies - race_start_jiffies;
                pr_info("racer_3_lap_time: [%lu, %lu, %lu, %lu]\n", temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3]);
                return (irq_handler_t)IRQ_HANDLED;
            }
        }
        if (count == 4){
            if (irq == pin_24_irq_number){
                previous_jiffies = current_jiffies;
                temp->lap_time[racer_4_index++] = current_jiffies - race_start_jiffies;
                pr_info("racer_4_lap_time: [%lu, %lu, %lu, %lu]\n", temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3]);
                return (irq_handler_t)IRQ_HANDLED;
            }
        }
        count++;
    }            
    pr_info("This should never print. Did you run your user space program? Did you put too few amount of contestants in the race?\n");
    return (irq_handler_t)IRQ_HANDLED;
}


/************************* Linked list code ***********************************/

/* function to initialize a node in the linked-list */
static void init_node(struct node *n, char *_bib_num){
	INIT_LIST_HEAD(&n->list);
	n->bib_number = _bib_num;
	n->lane_number = lane_num++;
    int i;
    for (i=0; i<4; i++){
    	n->lap_time[i] = 0;
    }
	n->race_time = 0;
}

/* function to add a node to the linked-list */
static void insert_entry(struct list_head *head, struct node *n){
	mutex_lock(&my_mutex);
	list_add_tail(&n->list, head);
	mutex_unlock(&my_mutex);
}

/* function to delete a node from the linked-list */
static void delete_entry(struct list_head *head, struct node *n){
	mutex_lock(&my_mutex);
	list_del_init(&n->list);
	mutex_unlock(&my_mutex);
}


/******************************* File operations *******************************/
static struct file_operations fops = {
	.owner   = THIS_MODULE,
	.open    = my_open,
	.release = my_release,
	.write   = my_write,
	.read    = my_read,
};

/* function to handle the open() user-space system call */
static int my_open(struct inode *inode, struct file *file){
	/* notifying the kernel buffer the open function has been called */
	pr_info("%s opened\n", DEVICE_FILE);

	/* allocates memory in kernel space to obtain user data */
	if((kernel_buffer = kmalloc(MEM_SIZE, GFP_KERNEL)) == 0){
		pr_err("Can't allocate memory in kernel.\n");
		return -1;
	}

	return 0;
}

/* function to handle the close() user-space system call */
static int my_release(struct inode *inode, struct file *file){
	/* notifying the kernel buffer the release/close function has been called*/
	pr_info("%s released\n", DEVICE_FILE);

	/* deallocates memory in kernel space which was used to obtain user data */
	kfree(kernel_buffer);

	return 0;
}

/* function to handle the write() user-space system call */
static ssize_t my_write(struct file *filp, const char *buf, size_t len, loff_t *off){
	/* notifying the kernel buffer the write function has been called */
	pr_info("%s write\n", DEVICE_FILE);

    /* user program sends a '!' to start race */
    if (buf[0] == '!'){
        race_start_jiffies = (unsigned long)jiffies;
        race_start = 1; 
        pr_info("Race started at %lu jiffies\n", race_start_jiffies);
        return len;
    }

    /* user inputs a 'q' to stop race */
    if (buf[0] == 'q'){
        pr_info("Race ended at %lu jiffies. Total race time: %lu\n", (unsigned long)jiffies, (unsigned long)jiffies - race_start_jiffies);
        return len;
    }

    /* user enters bib number for racers */
    else {
        /* brings user buffer to kernel buffer */
        if(copy_from_user(kernel_buffer, buf, len)){
            pr_err("writing to %s error\n", DEVICE_FILE);
        }

        /* creates a string which contains the runner's bib number */
        char runner_bib[len];
        memcpy(runner_bib, kernel_buffer, len);

        /* creates and inserts runner node to linked list */
        struct node *n = kmalloc(sizeof(struct node), GFP_KERNEL);
        init_node(n, runner_bib);
        insert_entry(&head, n);
    }
	return len;
}

/* function to handle the read() user-space system call */
static ssize_t my_read(struct file *filp, char __user *buf, size_t len, loff_t *off){
    char buffer[300] = "";
    char buffer1[75], buffer2[75], buffer3[75], buffer4[75];
    struct node *temp;
    int count = 1;
	list_for_each_entry(temp, &head, list){
        if (count == 1){
            if (racer_1_index >= 5) racer_1_index = 4;
            temp->race_time = temp->lap_time[racer_1_index-1];
            sprintf(buffer1, "Lane %d: Lap Time [%lu, %lu, %lu, %lu], Race Time %lu\n", temp->lane_number, temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3], temp->race_time);
        }
        if (count == 2){
            if (racer_2_index >= 5) racer_2_index = 4;
            temp->race_time = temp->lap_time[racer_2_index-1];
		    sprintf(buffer2, "Lane %d: Lap Time [%lu, %lu, %lu, %lu], Race Time %lu\n", temp->lane_number, temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3], temp->race_time);
        }
        if (count == 3){
            if (racer_2_index >= 5) racer_2_index = 4;
            temp->race_time = temp->lap_time[racer_3_index-1];
		    sprintf(buffer3, "Lane %d: Lap Time [%lu, %lu, %lu, %lu], Race Time %lu\n", temp->lane_number, temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3], temp->race_time);
        }
        if (count == 4){
            if (racer_2_index >= 5) racer_2_index = 4;
            temp->race_time = temp->lap_time[racer_4_index-1];
		    sprintf(buffer4, "Lane %d: Lap Time [%lu, %lu, %lu, %lu], Race Time %lu\n", temp->lane_number, temp->lap_time[0], temp->lap_time[1], temp->lap_time[2], temp->lap_time[3], temp->race_time);
        }
        count += 1;
	}
    sprintf(buffer, "RACE TIMES:\n%s%s%s%s", buffer1, buffer2, buffer3, buffer4);
    pr_info("%s", buffer);
    copy_to_user(buf, buffer, 300);
	pr_info("%s read\n", DEVICE_FILE);
	return 0;
}


/********************* Module Init ************************************/

/* function to dynamically initialize this kernel module into a device file */
static int __init my_driver_init(void)
{
	/* Dynamically allocating major and minor number */
	if((alloc_chrdev_region(&dev, 0, 1, "my_kernel")) < 0){
		pr_info("Cannot allocate major number for ll device\n");
		return -1;
	}
	pr_info("Major=%d  Minor=%d", MAJOR(dev), MINOR(dev));

	/* creating cdev structure */
	cdev_init(&my_cdev, &fops);

	/* adding chracter device to the system */
	if((cdev_add(&my_cdev, dev, 1)) < 0){
		pr_err("Cannot add the device to the system\n");
		unregister_chrdev_region(dev, 1);
		return -1;
	}

	/* creating struct class for device file */
	dev_class = class_create(THIS_MODULE, "my_class");
	if(IS_ERR(dev_class)){
		pr_err("Can't create the struct class for this device\n");
		unregister_chrdev_region(dev, 1);
		return -1;
	}

	/* creating device */
	if(IS_ERR(device_create(dev_class, NULL, dev, NULL, DEVICE_FILE))){
		pr_err("Cannot create device file\n");
		class_destroy(dev_class);
	}

    /* setup the gpio */
    if (gpio_request(PIN_17, "rpi-gpio-17")){
        pr_info("Error: Can not allocate GPIO 17\n");
        return -1;
    }
    if (gpio_request(PIN_27, "rpi-gpio-27")){
        pr_info("Error: Can not allocate GPIO 27\n");
        return -1;
    }
    if (gpio_request(PIN_23, "rpi-gpio-23")){
        pr_info("Error: Can not allocate GPIO 23\n");
        return -1;
    }
    if (gpio_request(PIN_24, "rpi-gpio-24")){
        pr_info("Error: Can not allocate GPIO 24\n");
        return -1;
    }

    /* set GPIO direction */
    if (gpio_direction_input(PIN_17)){
        pr_info("Error: Can not set GPIO 17 to input.\n");
        gpio_free(PIN_17);
        return -1;
    }
    if (gpio_direction_input(PIN_27)){
        pr_info("Error: Can not set GPIO 27 to input.\n");
        gpio_free(PIN_27);
        return -1;
    }
    if (gpio_direction_input(PIN_23)){
        pr_info("Error: Can not set GPIO 23 to input.\n");
        gpio_free(PIN_23);
        return -1;
    }
    if (gpio_direction_input(PIN_24)){
        pr_info("Error: Can not set GPIO 24 to input.\n");
        gpio_free(PIN_24);
        return -1;
    }

    /* setup the interrupt */
    pin_17_irq_number = gpio_to_irq(PIN_17);
    if (request_irq(pin_17_irq_number, (irq_handler_t)gpio_irq_handler, IRQF_TRIGGER_RISING, PIN_17_IRQ_FILE, NULL) != 0){
        pr_info("Error: Can not request interrupt number: %d\n", pin_17_irq_number);
        gpio_free(PIN_17);
        return -1;
    }
    pin_27_irq_number = gpio_to_irq(PIN_27);
    if (request_irq(pin_27_irq_number, (irq_handler_t)gpio_irq_handler, IRQF_TRIGGER_RISING, PIN_27_IRQ_FILE, NULL) != 0){
        pr_info("Error: Can not request interrupt number: %d\n", pin_27_irq_number);
        gpio_free(PIN_27);
        return -1;
    }
    pin_23_irq_number = gpio_to_irq(PIN_23);
    if (request_irq(pin_23_irq_number, (irq_handler_t)gpio_irq_handler, IRQF_TRIGGER_RISING, PIN_23_IRQ_FILE, NULL) != 0){
        pr_info("Error: Can not request interrupt number: %d\n", pin_23_irq_number);
        gpio_free(PIN_23);
        return -1;
    }
    pin_24_irq_number = gpio_to_irq(PIN_24);
    if (request_irq(pin_24_irq_number, (irq_handler_t)gpio_irq_handler, IRQF_TRIGGER_RISING, PIN_24_IRQ_FILE, NULL) != 0){
        pr_info("Error: Can not request interrupt number: %d\n", pin_24_irq_number);
        gpio_free(PIN_24);
        return -1;
    }
    current_jiffies = (unsigned long)jiffies;
    previous_jiffies = current_jiffies;
    pr_info("module_load_jiffies: %lu\n", current_jiffies);

    pr_info("GPIO 17 mapped to IRQ number %d\n", pin_17_irq_number);
    pr_info("GPIO 27 mapped to IRQ number %d\n", pin_27_irq_number);
    pr_info("GPIO 23 mapped to IRQ number %d\n", pin_23_irq_number);
    pr_info("GPIO 24 mapped to IRQ number %d\n", pin_24_irq_number);

	/* end of app */
	pr_info("successfully loaded module");
	return 0;
}
  
/****************************** Module Exit *************************************/
/* function to deinitialize this kernel module */
static void __exit my_driver_exit(void)
{
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	unregister_chrdev_region(dev, 1);
    free_irq(pin_17_irq_number, NULL);
    free_irq(pin_27_irq_number, NULL);
    free_irq(pin_23_irq_number, NULL);
    free_irq(pin_24_irq_number, NULL);
    gpio_free(PIN_17);
    gpio_free(PIN_27);
    gpio_free(PIN_23);
    gpio_free(PIN_24);
	pr_info("successfully unloaded module\n");
}
  
module_init(my_driver_init);
module_exit(my_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anhton Nguyen");
MODULE_DESCRIPTION("A linux device driver to simulate a race using buttons as the device ");
MODULE_VERSION("1.0");
