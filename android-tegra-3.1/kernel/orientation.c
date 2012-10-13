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
	atomic_set(&entry->granted,1);
	list_add_tail(&entry->granted_list, &granted_list);
	spin_lock(&WAITERS_LOCK);
	list_del(&entry->list);
	spin_unlock(&WAITERS_LOCK);
}

int in_range(struct orientation_range *range, struct dev_orientation orient)
{
	struct dev_orientation basis = range->orient;
	if (orient.azimuth > basis.azimuth + range->azimuth_range ||
	    orient.azimuth < basis.azimuth - range->azimuth_range)
		return 0;
	if (orient.pitch > basis.pitch + range->pitch_range ||
	    orient.pitch < basis.pitch - range->pitch_range)
		return 0;
	if (orient.roll > basis.roll + range->roll_range ||
	    orient.roll < basis.roll - range->roll_range)
		return 0;
	return 1;
}

void process_waiter(struct list_head *current_item)
{
	struct lock_entry *entry = list_entry(current_item,
					      struct lock_entry, list);
	struct orientation_range *target = entry->range;
	if (in_range(entry->range, current_orient)) {
		if (entry->type == READER_ENTRY) { /* Reader */
			if (no_writer_waiting(target) &&
			   no_writer_grabbed(target))
				grant_lock(entry);
		}
		else { /* Writer */
			if (no_writer_grabbed(target) &&
			    no_reader_grabbed(target))
				grant_lock(entry);
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

	list_for_each(current_item, &waiters_list)
		process_waiter(current_item);

	print_orientation(current_orient);

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
	struct lock_entry *entry;

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
	struct lock_entry *entry;

	if (copy_from_user(&korient, orient, sizeof(struct orientation_range)) != 0)
		return -EFAULT;

	list_for_each(current_item, &granted_list) {
		entry = list_entry(current_item, struct lock_entry, granted_list);
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
