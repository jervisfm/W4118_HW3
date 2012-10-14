#include <linux/orientation.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

void print_orientation(struct dev_orientation orient)
{
	printk("Azimuth: %d\n", orient.azimuth);
	printk("Pitch: %d\n", orient.pitch);
	printk("Roll: %d\n", orient.roll);
}

int orient_equals(struct dev_orientation one, struct dev_orientation two)
{
	return (one.azimuth == two.azimuth &&
		one.pitch == two.pitch &&
		one.roll == two.roll);
}

int range_equals(struct orientation_range *range,
		 struct orientation_range *target)
{
	return (range->azimuth_range == target->azimuth_range &&
		range->pitch_range == target->pitch_range &&
		range->roll_range == target->roll_range &&
		orient_equals(range->orient, target->orient));
}

int generic_search_list(struct orientation_range *target,
			struct list_head *list, int type)
{
	int flag = 1;
	struct list_head *current_item;
	list_for_each(current_item, list) {
		struct lock_entry *entry;
		if (list == &waiters_list)
			entry = list_entry(current_item, struct lock_entry, list);
		else
			entry = list_entry(current_item, struct lock_entry,
					   granted_list);
		if (entry->type == type &&
		    range_equals(entry->range, target)) {
			flag = 0;
			break;
		}
	}
	return flag;
}

int no_writer_waiting(struct orientation_range *target)
{
	return generic_search_list(target, &waiters_list, 1);
}

int no_writer_grabbed(struct orientation_range *target)
{
	return generic_search_list(target, &granted_list, 1);
}

int no_reader_grabbed(struct orientation_range *target)
{
	return generic_search_list(target, &granted_list, 0);
}

void grant_lock(struct lock_entry *entry)
{
	printk("Setting atomic ...");
	atomic_set(&entry->granted,1);
	printk("Done\n");

	printk("Adding to grant list...");
	list_add_tail(&entry->granted_list, &granted_list);
	printk("Done\n");

	printk("Getting spink lock...");
	spin_lock(&WAITERS_LOCK);
	printk("Acquired\n");

	printk("Deleting entry...");
	list_del(&entry->list);
	printk("Doone\n");

	printk("Unlocking lock...");
	spin_unlock(&WAITERS_LOCK);
	printk("Unlocked\n");
}


/*
 * Returns the adjusted value of the orientation number so that
 * it falls within the given range. i.e. if out of range
 * use the min/max values.
 */
int adjust_orientation(int num, int min_val, int max_val)
{
	int range = abs((max_val - min_val));
	if(num > max_val) {
		printk("Max val: %d\n", max_val);
		return max_val;
	} else if (num < min_val) {
		return min_val;
	} else { /* number is in range */
		return num;
	}
}

/* *
 * Determines if a wrap around has occured.
 *
 */
int wrapped_around(int min, int max) {

	/*
	 * When you have a wrap around,
	 * the max becomes less than the min.
	 */

	if(max < min)
		return 1;
	else
		return 0;
}

int in_range(struct orientation_range *range, struct dev_orientation orient)
{
	struct dev_orientation basis = range->orient;

	const int pitch_range = MAX_PITCH;
	const int roll_range = MAX_ROLL;

	int basis_azimuth = basis.azimuth;
	int basis_pitch = basis.pitch;
	int basis_roll = basis.roll;

	int range_azimuth = (int) range->azimuth_range;
	int range_pitch = (int) range->pitch_range;
	int range_roll = (int) range->roll_range;

	if(orient.azimuth > basis_azimuth + range_azimuth) {
		printk("Fail Azimuth1: %d > %d\n", orient.azimuth, basis_azimuth + range_azimuth);
		return 0;
	}
	if(orient.azimuth < basis_azimuth - range_azimuth) {
		printk("Fail Azimuth2: %d < %d\n", orient.azimuth, basis_azimuth - range_azimuth);
		return 0;
	}
	if(orient.pitch > basis_pitch + range_pitch ||
			orient.pitch < basis_pitch - range_pitch) {
		printk("Fail Pitch\n");
		return 0;
	}
	if(orient.roll > basis_roll + range_roll ||
	   orient.roll < basis_roll - range_roll) {
		printk("Fail orient\n");
		return 0;
	}

	return 1;
}

void process_waiter(struct list_head *current_item)
{
	struct lock_entry *entry = list_entry(current_item,
					      struct lock_entry, list);
	struct orientation_range *target = entry->range;
	if (in_range(entry->range, current_orient)) {
		printk("We are in range !!!\n");
		if (entry->type == READER_ENTRY) { /* Reader */
			if (no_writer_waiting(target) &&
			   no_writer_grabbed(target))
				grant_lock(entry);
		}
		else { /* Writer */
			printk("In the writer blockl\n");
			if(target == NULL) {
				printk("Target is NULL");
			}

			if (no_writer_grabbed(target) &&
			    no_reader_grabbed(target)) {
				printk("About to grant lock");
				grant_lock(entry);
			}

		}
	}
}

/* TODO: Do we really need __user in our definitions ??? */
SYSCALL_DEFINE1(set_orientation, struct dev_orientation __user *, orient)
{
	struct list_head *current_item;
	//TODO: Lock set_orientation for multiprocessing
	if (copy_from_user(&current_orient, orient,
				sizeof(struct dev_orientation)) != 0)
		return -EFAULT;

	// TODO: We need to automatically release locks for processes
	// that took a lock and died, without releasing the lock.

	list_for_each(current_item, &waiters_list)
		process_waiter(current_item);

	// print_orientation(current_orient);

	return 0;
}

SYSCALL_DEFINE1(orientlock_read, struct orientation_range __user *, orient)
{
	struct orientation_range *korient;
	struct lock_entry *entry;
	DEFINE_WAIT(wait);
	
	korient = kmalloc(sizeof(struct orientation_range), GFP_KERNEL);
	if (copy_from_user(korient, orient, sizeof(struct orientation_range)) != 0)
		return -EFAULT;

	entry = kmalloc(sizeof(struct lock_entry), GFP_KERNEL);
	entry->range = korient;
	atomic_set(&entry->granted,0);
	INIT_LIST_HEAD(&entry->list);
	INIT_LIST_HEAD(&entry->granted_list);
	entry->type = READER_ENTRY;

	spin_lock(&WAITERS_LOCK);
	list_add_tail(&entry->list, &waiters_list);
	spin_unlock(&WAITERS_LOCK);
	

	add_wait_queue(&sleepers, &wait);
	while(!atomic_read(&entry->granted)) {
	       prepare_to_wait(&sleepers, &wait, TASK_INTERRUPTIBLE);
	       schedule();
	}
	finish_wait(&sleepers, &wait);

	return 0;
}

SYSCALL_DEFINE1(orientlock_write, struct orientation_range __user *, orient)
{
	struct orientation_range *korient;
	struct lock_entry *entry;
	DEFINE_WAIT(wait);
	
	korient = kmalloc(sizeof(struct orientation_range), GFP_KERNEL);
	if (copy_from_user(korient, orient, sizeof(struct orientation_range)) != 0)
		return -EFAULT;

	entry = kmalloc(sizeof(entry), GFP_KERNEL);
	entry->range = korient;
	atomic_set(&entry->granted, 0);
	INIT_LIST_HEAD(&entry->list);
	INIT_LIST_HEAD(&entry->granted_list);
	entry->type = WRITER_ENTRY;
	
	spin_lock(&WAITERS_LOCK);
	list_add_tail(&entry->list, &waiters_list);
	spin_unlock(&WAITERS_LOCK);

	add_wait_queue(&sleepers, &wait);
	while(!atomic_read(&entry->granted)) {
		prepare_to_wait(&sleepers, &wait, TASK_INTERRUPTIBLE);
		schedule();
	}
	finish_wait(&sleepers, &wait);
	return 0;
}

SYSCALL_DEFINE1(orientunlock_read, struct orientation_range __user *, orient)
{
	struct orientation_range korient;
	struct list_head *current_item;
	struct lock_entry *entry = NULL;

	if (copy_from_user(&korient, orient, sizeof(struct orientation_range)) != 0)
		return -EFAULT;
	
	list_for_each(current_item, &granted_list) {
		entry = list_entry(current_item,
				   struct lock_entry, granted_list);
		if (range_equals(&korient, entry->range) &&
		    entry->type == READER_ENTRY)
			break;
		else
			; //no locks with the orientation_range available
	}
	list_del(current_item);
	kfree(entry->range);
	kfree(entry);
	return 0;
}

SYSCALL_DEFINE1(orientunlock_write, struct orientation_range __user *, orient)
{
	//TODO: Remember to free lock_entry and range
	struct orientation_range korient;
	struct list_head *current_item;
	struct lock_entry *entry = NULL;

	if (copy_from_user(&korient, orient, sizeof(struct orientation_range)) != 0)
		return -EFAULT;

	list_for_each(current_item, &granted_list) {
		entry = list_entry(current_item,
				   struct lock_entry, granted_list);
		if (range_equals(&korient, entry->range) &&
		    entry->type == WRITER_ENTRY)
			break;
		else
			; //no locks with the orientation_range available
	}
	list_del(current_item);
	kfree(entry->range);
	kfree(entry);
	return 0;
}
