#include <linux/orientation.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

static void print_orientation(struct dev_orientation orient)
{
	printk("Azimuth: %d\n", orient.azimuth);
	printk("Pitch: %d\n", orient.pitch);
	printk("Roll: %d\n", orient.roll);
}

static int orient_equals(struct dev_orientation one, struct dev_orientation two)
{
	return (one.azimuth == two.azimuth &&
		one.pitch == two.pitch &&
		one.roll == two.roll);
}

static int range_equals(struct orientation_range *range,
		 struct orientation_range *target)
{
	return (range->azimuth_range == target->azimuth_range &&
		range->pitch_range == target->pitch_range &&
		range->roll_range == target->roll_range &&
		orient_equals(range->orient, target->orient));
}

static int generic_search_list(struct orientation_range *target,
			struct list_head *list, int type)
{
	int flag = 1;
	struct list_head *current_item;
	list_for_each(current_item, list) {
		struct lock_entry *entry;
		if (list == &waiters_list)
			entry = list_entry(current_item,
					struct lock_entry, list);
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

static int no_writer_grabbed(struct orientation_range *target)
{	
	int rc;
	spin_lock(&GRANTED_LOCK);
	rc = generic_search_list(target, &granted_list, 1);
	spin_unlock(&GRANTED_LOCK);
	return rc;
}

static int no_reader_grabbed(struct orientation_range *target)
{
	int rc;
	spin_lock(&GRANTED_LOCK);
	rc = generic_search_list(target, &granted_list, 0);
	spin_unlock(&GRANTED_LOCK);
	return rc;
}

static void grant_lock(struct lock_entry *entry)
{
	atomic_set(&entry->granted,1);

	spin_lock(&GRANTED_LOCK);
	list_add_tail(&entry->granted_list, &granted_list);
	spin_unlock(&GRANTED_LOCK);

	if (&entry->list == NULL) {
		printk("OOPS: &entry->list is NULLLLLLL ");
	}

	if (entry->list.next == NULL) {
		printk("Next entry is NULLL\n");
	}

	if(entry->list.prev == NULL) {
		printk("Preve entry is NULL");
	}

	list_del(&entry->list);
	wake_up(&sleepers);
}


/*
 * Returns the adjusted value of the orientation number so that
 * it falls within the given range. i.e. if out of range
 * use the min/max values.
 */
static int adjust_orientation(int num, int min_val, int max_val)
{
	int range = abs((max_val - min_val));
	if(num > max_val) {
		return max_val;
	} else if (num < min_val) {
		return min_val;
	} else { /* number is in range */
		return num;
	}
}

static int in_range(struct orientation_range *range,
		struct dev_orientation orient)
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
		return 0;
	}
	if(orient.azimuth < basis_azimuth - range_azimuth) {
		return 0;
	}
	if(orient.pitch > basis_pitch + range_pitch ||
			orient.pitch < basis_pitch - range_pitch) {
		return 0;
	}
	if(orient.roll > basis_roll + range_roll ||
	   orient.roll < basis_roll - range_roll) {
		return 0;
	}

	return 1;
}


/* *
 * Determines if the task with given pid is still running.
 * Returns 1 if true and 0 if false.
 * Tested this on emulator to confirm it's working.
 */
static int is_running(int pid ){

	struct pid *pid_struct;
	struct task_struct *task;
	int is_dead, is_wakekill, exit_zombie, exit_dead;
	pid_struct = find_get_pid(pid);

	if(pid_struct == NULL) {
		return 0;
	}
	task = pid_task(pid_struct,PIDTYPE_PID);
	if(task == NULL) {
		return 0;
	}

	is_dead = (task->state & TASK_DEAD) != 0;
	is_wakekill = (task->state & TASK_WAKEKILL) != 0;
	exit_zombie = (task->exit_state & EXIT_ZOMBIE) != 0;
	exit_dead = (task->exit_state & EXIT_DEAD) != 0;

	if(is_dead || is_wakekill || exit_zombie || exit_dead) {
		printk("Task is not RUNNING ever: %d", task->pid);
		return 0;
	} else {
		printk("Task is normal: %d ", task->pid);
		return 1;
	}
}

/**
 * Removes the locks for process that are no longer running
 * from the granted list.
 * NOTICE we acquire the GRANT LIST LOCK
 */
static void release_dead_tasks_locks(void) {
	struct list_head *current_item;
	struct list_head *next_item;
	int counter = 1;
	spin_lock(&GRANTED_LOCK);
	list_for_each_safe(current_item, next_item, &granted_list) {
		struct lock_entry *entry = list_entry(current_item,
						      struct lock_entry,
						      granted_list);
		int pid = entry->pid;
		if(!is_running(pid)) {
			list_del(current_item);
		}
		++counter;
	}
	spin_unlock(&GRANTED_LOCK);
}

static void process_waiter(struct list_head *current_item)
{
	struct lock_entry *entry = list_entry(current_item,
					      struct lock_entry, list);

	struct orientation_range *target = entry->range;

	if (in_range(entry->range, current_orient)) {
		if (entry->type == READER_ENTRY) { /* Reader */
			if (no_writer_grabbed(target))
				grant_lock(entry);
		}
		else { /* Writer */
			if (no_writer_grabbed(target) &&
			    no_reader_grabbed(target))
				grant_lock(entry);
		}
	}
}

SYSCALL_DEFINE1(set_orientation, struct dev_orientation __user *, orient)
{
	spin_lock(&SET_LOCK);
	struct list_head *current_item, *next_item;
	int counter;

	counter = 0;
	if (copy_from_user(&current_orient, orient,
				sizeof(struct dev_orientation)) != 0)
		return -EFAULT;

	spin_lock(&WAITERS_LOCK);
	release_dead_tasks_locks();

	list_for_each_safe(current_item, next_item, &waiters_list) {
		process_waiter(current_item);
	}

	spin_unlock(&WAITERS_LOCK);
	spin_unlock(&SET_LOCK);
	return 0;
}

// TODO: Fix the list traversal in all SYS calls : use list_for_each_safe.
SYSCALL_DEFINE1(orientlock_read, struct orientation_range __user *, orient)
{
	struct orientation_range *korient;
	struct lock_entry *entry;
	DEFINE_WAIT(wait);
	
	korient = kmalloc(sizeof(struct orientation_range), GFP_KERNEL);

	if (korient == NULL) {
		printk("Kmalloc failure: orientlock_read");
		return  -ENOMEM;
	}

	if (copy_from_user(korient, orient,
				sizeof(struct orientation_range)) != 0)
		return -EFAULT;

	entry = kmalloc(sizeof(struct lock_entry), GFP_KERNEL);

	if (entry == NULL) {
		printk("\nKmalloc failure: orientlock_read");
		return -ENOMEM;
	}

	entry->range = korient;
	entry->pid = current->pid;
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

	if (korient == NULL) {
		printk("Kmalloc failure: orientlock_write\n");
		return  -ENOMEM;
	}
	if (copy_from_user(korient, orient,
				sizeof(struct orientation_range)) != 0)
		return -EFAULT;

	entry = kmalloc(sizeof(entry), GFP_KERNEL);

	if (entry == NULL) {
		printk("Kmalloc failure!...\n");
		return -ENOMEM;
	}
	
	entry->range = korient;
	entry->pid = current->pid;
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
	struct list_head *current_item, *next_item;
	struct lock_entry *entry = NULL;
	int did_unlock = 0;
	if (copy_from_user(&korient, orient,
				sizeof(struct orientation_range)) != 0)
		return -EFAULT;
	
	spin_lock(&GRANTED_LOCK);
	list_for_each_safe(current_item, next_item, &granted_list) {
		entry = list_entry(current_item,
				   struct lock_entry, granted_list);
		if (range_equals(&korient, entry->range) &&
		    entry->type == READER_ENTRY) {
			/* Unlock is to be done original locking pid process */
			int curr_pid = current->pid;
			/* DO NOT RETURN eearly, process the entire list */
			if (curr_pid != entry->pid)
				continue;

			list_del(current_item);
			kfree(entry->range);
			kfree(entry);
			spin_unlock(&GRANTED_LOCK);
			did_unlock = 1;
			break;
		}
		else
			; //no locks with the orientation_range available
	}


	if (!did_unlock) { /* failed to unlocked */
		spin_unlock(&GRANTED_LOCK);
		return -1;
	} else {
		return 0;
	}
}

SYSCALL_DEFINE1(orientunlock_write, struct orientation_range __user *, orient)
{
	struct orientation_range korient;
	struct list_head *current_item, *next_item;

	if (copy_from_user(&korient, orient,
				sizeof(struct orientation_range)) != 0)
		return -EFAULT;
	
	spin_lock(&GRANTED_LOCK);
	list_for_each_safe(current_item, next_item, &granted_list) {
		struct lock_entry *entry;
		entry = list_entry(current_item,
				   struct lock_entry, granted_list);
		if (range_equals(&korient, entry->range) &&
		    entry->type == WRITER_ENTRY) {
			list_del(current_item);
			kfree(entry->range);
			kfree(entry);
			spin_unlock(&GRANTED_LOCK);
			return 0;
		}	
		else
			; //no locks with the orientation_range available
	}
	spin_unlock(&GRANTED_LOCK);
	printk("Undefined error\n");
	return 0;
}
