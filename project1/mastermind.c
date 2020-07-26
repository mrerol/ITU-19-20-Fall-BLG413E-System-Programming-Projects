#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/switch_to.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */


#ifndef __ASM_ASM_UACCESS_H
    #include <linux/uaccess.h>
#endif

#include "mastermind_ioctl.h"

#define MMIND_MAJOR 0
#define NMIND_MAX_GUESSES 10
#define MAX_LINE_NUMBER 256
#define MMIND_LINE_SIZE_BYTES 16
#define MMIN_BUFFER_SIZE_BYTES 4096
#define MMIND_NUMBER 4283

int mmind_major = MMIND_MAJOR;
int mmind_minor = 0;
int mmind_max_guesses = NMIND_MAX_GUESSES;
char *mmind_number = "4283";

module_param(mmind_major, int, S_IRUGO);
module_param(mmind_minor, int, S_IRUGO);
module_param(mmind_max_guesses, int, S_IRUGO);
module_param(mmind_number, charp, S_IRUGO);

MODULE_AUTHOR("Samet Pilav, Muhammed Rasit Erol, Joshgun Rzabayli");
MODULE_LICENSE("Dual BSD/GPL");

struct line
{
    char *guess;
    int number_of_in_place_digits;
    int number_of_out_of_place_digits;
    int number_of_guesses;
    int line_order;
};

struct mastermind_dev
{
    struct line **data;
    char *mmind_number;
    int line_number; // total line number
    int line_number_each_game; //current line number each game
    struct semaphore sem;
    struct cdev cdev;
};

struct mastermind_dev *mastermind_devices;



int mastermind_trim(struct mastermind_dev *dev)
{
    int i;

    if (dev->data) {
        for (i = 0; i < dev->line_number; i++) {
            if (dev->data[i]){
				kfree(dev->data[i]->guess);
                kfree(dev->data[i]);
			}
        }
        kfree(dev->data);
    }
    dev->data = NULL;
    kfree(dev->mmind_number);
    dev->line_number = 0;
    dev->line_number_each_game = 0;
    return 0;
}

int mastermind_open(struct inode *inode, struct file *filp)
{
    struct mastermind_dev *dev;

    dev = container_of(inode->i_cdev, struct mastermind_dev, cdev);
    filp->private_data = dev;
	
    return 0;
}

int mastermind_release(struct inode *inode, struct file *filp)
{
    return 0;
}

ssize_t mastermind_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos)
{
	struct mastermind_dev *dev = filp->private_data;
	int line_number = dev->line_number;
    ssize_t retval = 0;
    int counter_line = count / MMIND_LINE_SIZE_BYTES;
    int i = 0, j = 0;
	char *local_buffer;
	

    if (down_interruptible(&dev->sem)){
        return -ERESTARTSYS;
	}
	// counts until fpos reach the end
	if (*f_pos >= line_number * MMIND_LINE_SIZE_BYTES){
        goto out;
	}
	// determine readable size, also due to fpos increases each iteration, so counts decreases
	if (*f_pos + count > MMIN_BUFFER_SIZE_BYTES){
        count = MMIN_BUFFER_SIZE_BYTES - *f_pos;
	}
	// decrased count affect number of reading element
	if (line_number * MMIND_LINE_SIZE_BYTES < count ){
		count = line_number * MMIND_LINE_SIZE_BYTES;
	}
	
    if (dev->data == NULL){
        goto out;
	}
	
	local_buffer = kmalloc(count * sizeof(char), GFP_KERNEL);
	memset(local_buffer, 0, count * sizeof(char));	
	
	counter_line = count / MMIND_LINE_SIZE_BYTES;
	for(i = 0;  i < counter_line ; i++){
		
		for(j=0 ; j < 4 ; j++){
			local_buffer[MMIND_LINE_SIZE_BYTES * i + j] = dev->data[i]->guess[j];
		}

		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = ' ';
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = dev->data[i]->number_of_in_place_digits + '0';
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = '+';
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = ' ';
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = dev->data[i]->number_of_out_of_place_digits + '0';
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = '-';
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = ' ';
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = '0' + (dev->data[i]->line_order / 1000) % 10;
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = '0' + (dev->data[i]->line_order / 100) % 10;
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = '0' + (dev->data[i]->line_order / 10) % 10;
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = '0' + dev->data[i]->line_order % 10;
		local_buffer[MMIND_LINE_SIZE_BYTES * i + j++] = '\n';
		
	}

    if (copy_to_user(buf, local_buffer, count)) {
        retval = -EFAULT;
        goto out;
    }
    
    *f_pos += count;
    retval = count;
    
    kfree(local_buffer);
    
  out:
    up(&dev->sem);
    return retval;
}


ssize_t mastermind_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos)
{
	struct mastermind_dev *dev = filp->private_data;
    int line_pos = dev->line_number;
    char *local_buffer;
    int i=0;
    int m = 0 ,n = 0;
    int a = 0, b = 0;
    char *guess_string;
    char *mmind_number_temp;
    ssize_t retval = -ENOMEM;
  
    if (down_interruptible(&dev->sem)){
        return -ERESTARTSYS;
	}
	if (*f_pos >= MMIN_BUFFER_SIZE_BYTES) {
        retval = 0;
        goto out;
    }
    
	if(dev->line_number_each_game  >= mmind_max_guesses){
		printk(KERN_ALERT "You have reached maximum guess number %d!\n",mmind_max_guesses);
		retval = -ENOMEM;
		goto out;
	}
	
	if(dev->line_number  >= MAX_LINE_NUMBER){
		printk(KERN_ALERT "You have reached maximum guess number %d\n!", MAX_LINE_NUMBER);
		retval = -ENOMEM;
		goto out;
	}
	
	if(count != 5){
		printk(KERN_ALERT "Invalid input! The guess must be size 4!");
		retval = -EINVAL;
		goto out;
	}	
	
    if (!dev->data) {
        dev->data = kmalloc(MAX_LINE_NUMBER * sizeof(struct line *), GFP_KERNEL);
        if (!dev->data){
            goto out;
		}
        memset(dev->data, 0, MAX_LINE_NUMBER * sizeof(struct line *));
    }
    if (!dev->data[line_pos]) {
        dev->data[line_pos] = kmalloc(sizeof(struct line), GFP_KERNEL);
        if (!dev->data[line_pos]){
            goto out;
		}
    }

    local_buffer = kmalloc(count * sizeof(char), GFP_KERNEL);

    if (copy_from_user(local_buffer, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    guess_string = kmalloc(5 * sizeof(char), GFP_KERNEL);
	mmind_number_temp = kmalloc(5 * sizeof(char), GFP_KERNEL);
	
    for(i=0;i<4;i++){
		guess_string[i] = local_buffer[i];
		mmind_number_temp[i] = dev->mmind_number[i];
	}
	guess_string[i]='\0';
	
    dev->data[line_pos]->guess = kmalloc(5 * sizeof(char), GFP_KERNEL);
	strncpy(dev->data[line_pos]->guess , guess_string, 5);
	
	for (a = 0; a < 4; a++) {
		if (mmind_number_temp[a] == guess_string[a]){
			m++;
			continue;
		}
		for (b = 0; b < 4; b++) {
			if(a != b){
				if(mmind_number_temp[a] == guess_string[b]){
					n++;
					mmind_number_temp[a] = '!';	
				}
			}
		}
	}
	dev->data[line_pos]->number_of_in_place_digits = m;
	dev->data[line_pos]->number_of_out_of_place_digits = n;
	dev->data[line_pos]->line_order = dev->line_number_each_game++ + 1;
	dev-> line_number++;

    *f_pos += count;
    retval = count;
    
    kfree(guess_string);
    kfree(mmind_number_temp);
    kfree(local_buffer);
  out:

    up(&dev->sem);
    return retval;
}

long mastermind_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, i = 0;
	int retval = 0;
	struct mastermind_dev *dev = filp->private_data;
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != MASTERMIND_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > MASTERMIND_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
		case MASTERMIND_MMIND_REMAINING:
			if (! capable (CAP_SYS_ADMIN)){
				return -EPERM;
			}
			return mmind_max_guesses - dev->line_number_each_game;
			break;
		case MASTERMIND_MMIND_ENDGAME:
			if (! capable (CAP_SYS_ADMIN)){
				return -EPERM;
			}
			mastermind_trim(mastermind_devices);
			break;
		case MASTERMIND_MMIND_NEWGAME:

			if (! capable (CAP_SYS_ADMIN)){
				return -EPERM;
			}
			
			dev->line_number_each_game = 0;
			
			if(strlen((char *)arg) != 4){
				printk(KERN_ALERT "Invalid magic number! The Number must be 4 digit!\n");
				return -EINVAL; 
			}
			
			for(i = 0 ; i < 4; i++){
				if( *((char *)arg + i) - '0' < 0 || *((char *)arg + i) - '0' > 9){
					printk(KERN_ALERT "Invalid magic number! The Number must be digit!\n");
					return -EINVAL; 
				}
			}	
			strncpy(dev->mmind_number,  (char *)arg, 4);
	
			break;	
		default:  /* redundant, as cmd was checked against MAXNR */
			return -ENOTTY;
	}
	return retval;
}


struct file_operations mastermind_fops = {
    .owner =    THIS_MODULE,
    .read =     mastermind_read,
    .write =    mastermind_write,
    .unlocked_ioctl =  mastermind_ioctl,
    .open =     mastermind_open,
    .release =  mastermind_release,
};

void mastermind_cleanup_module(void)
{

    dev_t devno = MKDEV(mmind_major, mmind_minor);

    if (mastermind_devices) {
		mastermind_trim(mastermind_devices );
        cdev_del(&mastermind_devices->cdev);
		kfree(mastermind_devices);
    }

    unregister_chrdev_region(devno, 1);
}


int mastermind_init_module(void)
{
    int result, i = 0;
    int err;
    dev_t devno = 0;
    struct mastermind_dev *dev;
    
    if(mmind_max_guesses > MAX_LINE_NUMBER){
		printk(KERN_ALERT "Invalid size! Size must be between 0-256\n");
		return -EINVAL; 
	}
	if(strlen(mmind_number) != 4){
		printk(KERN_ALERT "Invalid magic number! Number of digits must be 4!\n");
		return -EINVAL; 
	}
	
	for(i = 0 ; i < 4; i++){
		if( mmind_number[i] - '0' < 0 || mmind_number[i] - '0' > 9){
			printk(KERN_ALERT "Invalid magic number! The Number must be digit!\n");
			return -EINVAL; 
		}
	}	
	
    if (mmind_major) {
        devno = MKDEV(mmind_major, mmind_minor);
        result = register_chrdev_region(devno, 1, "mastermind");
    } else {
        result = alloc_chrdev_region(&devno, mmind_minor, 1,
                                     "mastermind");
        mmind_major = MAJOR(devno);
    }
    if (result < 0) {
        printk(KERN_WARNING "mastermind: can't get major %d\n", mmind_major);
        return result;
    }

    mastermind_devices = kmalloc(1 * sizeof(struct mastermind_dev),
                            GFP_KERNEL);
    if (!mastermind_devices) {
        result = -ENOMEM;
        goto fail;
    }
    memset(mastermind_devices, 0, 1 * sizeof(struct mastermind_dev));

    /* Initialize device. */
	dev = mastermind_devices;
	dev->line_number = 0;
	dev->line_number_each_game = 0;
	dev->mmind_number = kmalloc(4 *sizeof(char),GFP_KERNEL);
	strncpy(dev->mmind_number,mmind_number, 4);
	sema_init(&dev->sem,1);
	devno = MKDEV(mmind_major, mmind_minor);
	cdev_init(&dev->cdev, &mastermind_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mastermind_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding mastermind", err);
		
	

    return 0; /* succeed */

  fail:
    mastermind_cleanup_module();
    return result;
}

module_init(mastermind_init_module);
module_exit(mastermind_cleanup_module);
