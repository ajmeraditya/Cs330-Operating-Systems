#include <context.h>
#include <memory.h>
#include <lib.h>
#include <entry.h>
#include <file.h>
#include <tracer.h>

///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit)
{
	struct exec_context *current = get_current_ctx();
	for (int i = 0; i < MAX_MM_SEGS; i++)
	{
		if (current->mms[i].start <= buff && current->mms[i].end >= buff + count)
		{
			if (access_bit == 0)
			{
				if (current->mms[i].access_flags & 1)
					return 1;
				
				else
					return 0;
			}
			else if (access_bit == 1)
			{
				if (current->mms[i].access_flags & 2)
					return 1;
				else
					return 0;
			}
			else
			{
				if (current->mms[i].access_flags & 4)
					return 1;
				else
					return 0;
			}
		}
	}
	struct vm_area *temp = current->vm_area;
	while (temp != NULL)
	{
		if (temp->vm_start <= buff && temp->vm_end >= buff + count)
		{
			if (access_bit == 0)
			{
				if (temp->access_flags & 1)
					return 1;
				else
					return 0;
			}
			else if (access_bit == 1)
			{
				if (temp->access_flags & 2)
					return 1;
				else
					return 0;
			}
			else
			{
				if (temp->access_flags & 4)
					return 1;
				else
					return 0;
			}
		}
		temp = temp->vm_next;
	}
	return 0;
}

long trace_buffer_close(struct file *filep)
{
	// free buffer in trace buffer
	os_free(filep->fops, sizeof(struct fileops));
	// free trace_buffer_info structure
	os_page_free(2, filep->trace_buffer->buff);
	os_free(filep->trace_buffer, sizeof(struct trace_buffer_info));
	// free files structure
	os_free(filep, sizeof(struct file));
	// tb_fd as NULL
	filep = NULL;
	return 0;
}

int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
	// check validity of buff and trace buffer
	if (filep->mode % 2 != 1)
		return -EINVAL;

	if (is_valid_mem_range((unsigned long)buff, count, 1) == 0)
		return -EBADMEM;

	if (filep->trace_buffer->space_left == 4096)
		return 0;
	
	else
	{
		s32 i = 0;
		while (i < count)
		{
			if (filep->trace_buffer->read_offset == filep->trace_buffer->write_offset && i != 0)
				break;

			buff[i] = *(char *)(filep->trace_buffer->buff + filep->trace_buffer->read_offset);
			filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + 1) % TRACE_BUFFER_MAX_SIZE;
			i++;
		}
		filep->trace_buffer->space_left = filep->trace_buffer->space_left + i;
		return i;
	}
	return 0;
}

int trace_buffer_write(struct file *filep, char *buff, u32 count)
{
	if ((((filep->mode) / 2) % 2) != 1)
		return -EINVAL;
	
	// check validity of buff and trace buffer
	if (is_valid_mem_range((unsigned long)buff, count, 0) == 0)
		return -EBADMEM;

	if (filep->trace_buffer->space_left == 0)
		return 0;
	else
	{
		s32 i = 0;
		while (i < count)
		{
			if (filep->trace_buffer->write_offset == filep->trace_buffer->read_offset && i != 0)
				break;
			
			*(char *)(filep->trace_buffer->buff + filep->trace_buffer->write_offset) = buff[i];
			filep->trace_buffer->write_offset = (filep->trace_buffer->write_offset + 1) % TRACE_BUFFER_MAX_SIZE;
			i++;
		}
		filep->trace_buffer->space_left = filep->trace_buffer->space_left - i;
		return i;
	}
	return 0;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{

	// find the next free file descriptor using the file array in current
	u32 tb_fd = 0;
	while (current->files[tb_fd] != NULL)
		tb_fd++;

	if (tb_fd >= MAX_OPEN_FILES)
		return -EINVAL;

	// alocate a new struct file
	current->files[tb_fd] = (struct file *)os_alloc(sizeof(struct file));
	if (current->files[tb_fd] == NULL)
		return -ENOMEM;

	// assign mode
	current->files[tb_fd]->mode = mode;

	// allocate memory using os_page_alloc and initialize its members
	current->files[tb_fd]->trace_buffer = os_alloc(sizeof(struct trace_buffer_info));
	if (current->files[tb_fd]->trace_buffer == NULL)
		return -ENOMEM;

	current->files[tb_fd]->type = TRACE_BUFFER;
	current->files[tb_fd]->inode = NULL;
	current->files[tb_fd]->trace_buffer->buff = (char *)os_page_alloc(2);
	if (current->files[tb_fd]->trace_buffer->buff == NULL)
		return -ENOMEM;

	// assign the new functions to the fileops structure
	current->files[tb_fd]->fops = os_alloc(sizeof(struct fileops));
	if (current->files[tb_fd]->fops == NULL)
		return -ENOMEM;

	current->files[tb_fd]->fops->read = trace_buffer_read;
	current->files[tb_fd]->fops->write = trace_buffer_write;
	current->files[tb_fd]->fops->close = trace_buffer_close;
	current->files[tb_fd]->trace_buffer->space_left = TRACE_BUFFER_MAX_SIZE;

	// initialize the trace_buffer_info structure
	current->files[tb_fd]->trace_buffer->read_offset = 0;
	current->files[tb_fd]->trace_buffer->write_offset = 0;

	return tb_fd;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

int syscall_argument_number(int syscall_num) {
    switch (syscall_num) {
        // Case 0
        case SYSCALL_GETPID:
        case SYSCALL_GETPPID:
        case SYSCALL_FORK:
        case SYSCALL_CFORK:
        case SYSCALL_VFORK:
        case SYSCALL_PHYS_INFO:
        case SYSCALL_STATS:
        case SYSCALL_GET_USER_P:
        case SYSCALL_GET_COW_F:
        case SYSCALL_END_STRACE:
            return 0;
        
        // Case 1
        case SYSCALL_ALARM:
        case SYSCALL_EXIT:
        case SYSCALL_CONFIGURE:
        case SYSCALL_SLEEP:
        case SYSCALL_DUMP_PTT:
        case SYSCALL_PMAP:
        case SYSCALL_DUP:
        case SYSCALL_CLOSE:
        case SYSCALL_TRACE_BUFFER:
            return 1;
        
        // Case 2
        case SYSCALL_SHRINK:
        case SYSCALL_SIGNAL:
        case SYSCALL_EXPAND:
        case SYSCALL_CLONE:
        case SYSCALL_MUNMAP:
        case SYSCALL_OPEN:
        case SYSCALL_DUP2:
        case SYSCALL_START_STRACE:
        case SYSCALL_STRACE:
            return 2;

        // Case 3
        case SYSCALL_MPROTECT:
        case SYSCALL_WRITE:
        case SYSCALL_READ:
        case SYSCALL_LSEEK:
        case SYSCALL_READ_STRACE:
        case SYSCALL_READ_FTRACE:
            return 3;

        // Case 4
        case SYSCALL_MMAP:
        case SYSCALL_FTRACE:
            return 4;
        
        default:
            return -1;
    }
}


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	struct exec_context *current = get_current_ctx();
	if (syscall_num == 1 || syscall_num == 37 || syscall_num == 38)
		return 0; // exit, end_strace and start_strace will not be handled
	if (current->st_md_base == NULL || current->st_md_base->is_traced == 0)
		return 0;
	
	if (current->st_md_base->tracing_mode == FILTERED_TRACING)
	{
		s32 found = 0;
		struct strace_info *temp = current->st_md_base->next;
		while (temp != NULL)
		{
			if (temp->syscall_num == syscall_num)
			{
				found = 1;
				break;
			}
			temp = temp->next;
		}
		if (found == 0)
			return 0;
	}
	u64 *array = (u64 *)os_alloc(5 * sizeof(u64));
	if(array == NULL)
		return -EINVAL;

	u64 params = syscall_argument_number(syscall_num);

	array[0] = syscall_num;
	array[1] = param1;
	array[2] = param2;
	array[3] = param3;
	array[4] = param4;

	if (params == -1)
		return -EINVAL;

	int write;
	struct file *trace_buff = current->files[current->st_md_base->strace_fd];

	*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = syscall_num;

	for (int i = 1; i <= params; i++)
	{
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset + i * sizeof(u64)) = array[i];
	}
	trace_buff->trace_buffer->write_offset = (trace_buff->trace_buffer->write_offset + (params + 1) * sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
	trace_buff->trace_buffer->space_left = trace_buff->trace_buffer->space_left - (params + 1) * sizeof(u64);

	return 0;
}

int sys_strace(struct exec_context *current, int syscall_num, int action)
{
	// Check validity of current, syscall_num, action?
	if (current->st_md_base == NULL)
	{
		current->st_md_base = os_alloc(sizeof(struct strace_head));
		if(current->st_md_base == NULL)
			return -EINVAL;

		current->st_md_base->is_traced = 0;
		current->st_md_base->count = 0;
		current->st_md_base->next = NULL;
		current->st_md_base->last = NULL;
	}

	if (action == ADD_STRACE)
	{
		if (current->st_md_base->count > MAX_STRACE)
			return -EINVAL;
		struct strace_info *temp = current->st_md_base->next;
		while (temp != NULL)
		{ // if syscall already exists
			if (temp->syscall_num == syscall_num)
				return -EINVAL;
			temp = temp->next;
		}

		struct strace_info *new_strace_info = os_alloc(sizeof(struct strace_info));
		if(new_strace_info == NULL)
			return -EINVAL;

		new_strace_info->syscall_num = syscall_num;
		new_strace_info->next = NULL;

		if (current->st_md_base->last != NULL)
			current->st_md_base->last->next = new_strace_info;
		current->st_md_base->last = new_strace_info; // Add new node at end of list and update the list

		if (current->st_md_base->next == NULL)
			current->st_md_base->next = new_strace_info;

		current->st_md_base->count++;
		return 0;
	}
	else if (action == REMOVE_STRACE)
	{
		struct strace_info *temp = current->st_md_base->next;
		while (temp != NULL)
		{
			if (temp->syscall_num == syscall_num)
			{
				struct strace_info *temp1 = current->st_md_base->next;
				while (temp1 != NULL)
				{
					if (temp1->next == temp)
						break;
					temp1 = temp1->next;
				}
				struct strace_info *temp2 = temp->next;

				if (temp1 == NULL)
					current->st_md_base->next = temp2;
				else
					temp1->next = temp2;

				os_free(temp, sizeof(struct strace_info));
				current->st_md_base->count--;
				return 0;
			}
			temp = temp->next;
		}
		return -EINVAL;
	}
	return -EINVAL;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	if (buff == NULL || filep == NULL || filep->type != TRACE_BUFFER || filep->trace_buffer == NULL)
		return -EINVAL;

	int i, read, j;
	int bytes = 0;

	u64 num, params;
	u64 params_arr[5];
	for (i = 0; i < count && filep->trace_buffer->space_left != 4096; i++)
	{
		num = *(u64 *)(filep->trace_buffer->buff + filep->trace_buffer->read_offset);
		params_arr[0] = num;

		params = syscall_argument_number(num);
		if (params == -1)
			return -EINVAL; // invalid syscall

		for (j = 1; j <= params; j++)
			params_arr[j] = *(u64 *)(filep->trace_buffer->buff + filep->trace_buffer->read_offset + j * sizeof(u64));
		
		filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + (params + 1) * sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
		filep->trace_buffer->space_left = filep->trace_buffer->space_left + (params + 1) * sizeof(u64);

		for (int j = 0; j <= params; j++)
			*(u64 *)(buff + j * sizeof(u64)) = params_arr[j];

		buff = buff + (params + 1) * sizeof(u64);
		bytes += (params + 1) * sizeof(u64);
	}
	return bytes;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{
	if (current->st_md_base == NULL)
	{
		current->st_md_base = os_alloc(sizeof(struct strace_head));

		if(current->st_md_base == NULL)
			return -EINVAL;

		// Initialize the pointers to NULL
		current->st_md_base->next = NULL;
		current->st_md_base->last = NULL;
		// Initializing count to 0
		current->st_md_base->count = 0;
	}

	// Enable bit to show that we have started tracing
	current->st_md_base->is_traced = 1;

	// Initializing the trace buffer
	current->st_md_base->strace_fd = fd;

	// Trace mode
	current->st_md_base->tracing_mode = tracing_mode;

	return 0;
}

int sys_end_strace(struct exec_context *current)
{

	// Free all the pointers in strace_info
	struct strace_info *temp = current->st_md_base->next;
	while (temp != NULL)
	{
		struct strace_info *temp1 = temp;
		temp = temp->next;
		os_free(temp1, sizeof(struct strace_info));
	}

	current->st_md_base->is_traced = 0;

	// Finally free the strace_head
	os_free(current->st_md_base, sizeof(struct strace_head));
	current->st_md_base = NULL;

	return 0;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

long do_ftrace(struct exec_context *current, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{

	// Allocate memory to the st_md_base : Do I have to allot memory or is already alloted??
	if (current->ft_md_base == NULL)
	{
		current->ft_md_base = os_alloc(sizeof(struct ftrace_head));

		if(current->ft_md_base == NULL)
			return -EINVAL;
	
		// Initialize the pointers to NULL
		current->ft_md_base->next = NULL;
		current->ft_md_base->last = NULL;
		// Initializing count to 0
		current->ft_md_base->count = 0;
	}

	if (action == ADD_FTRACE)
	{
		if (current->st_md_base->count > FTRACE_MAX)
			return -EINVAL;
		struct ftrace_info *temp = current->ft_md_base->next;
		while (temp != NULL)
		{ // if syscall already exists
			if (temp->faddr == faddr)
				return -EINVAL;
			temp = temp->next;
		}

		struct ftrace_info *new_ftrace_info = os_alloc(sizeof(struct ftrace_info));
		if(new_ftrace_info == NULL)
			return -EINVAL;

		new_ftrace_info->faddr = faddr;
		new_ftrace_info->num_args = nargs;
		new_ftrace_info->fd = fd_trace_buffer;
		new_ftrace_info->capture_backtrace = 0;
		new_ftrace_info->next = NULL;

		if (current->ft_md_base->last != NULL)
			current->ft_md_base->last->next = new_ftrace_info;
		current->ft_md_base->last = new_ftrace_info; // Add new node at end of list and update the list

		if (current->ft_md_base->next == NULL)
			current->ft_md_base->next = new_ftrace_info;

		current->ft_md_base->count++;
	}
	else if (action == REMOVE_FTRACE)
	{
		int present = 0;
		struct ftrace_info *temp = current->ft_md_base->next;
		while (temp != NULL)
		{
			if (temp->faddr == faddr)
			{
				present = 1;
				struct ftrace_info *temp1 = current->ft_md_base->next;
				while (temp1 != NULL)
				{
					if (temp1->next == temp)
						break;
					temp1 = temp1->next;
				}
				struct ftrace_info *temp2 = temp->next;

				if (temp1 == NULL)
					current->ft_md_base->next = temp2;
				else
					temp1->next = temp2;

				os_free(temp, sizeof(struct ftrace_info));
				current->ft_md_base->count--;
			}
			temp = temp->next;
		}
		if (!present)
			return -EINVAL;
	}
	else if (action == ENABLE_FTRACE)
	{
		struct ftrace_info *temp = current->ft_md_base->next;
		int found = 0;

		while (temp != NULL)
		{ // if syscall already exists
			if (temp->faddr == faddr)
			{
				u8 *faddr_ptr = (u8 *)faddr;
				for (int i = 0; i < 4; i++)
					temp->code_backup[i] = *(faddr_ptr + i);
				
				for (int i = 0; i < 4; i++)
					*(faddr_ptr + i) = (INV_OPCODE);
				
				found = 1;
				break;
			}

			temp = temp->next;
		}
		if (!found)
			return -EINVAL;
	}
	else if (action == DISABLE_FTRACE)
	{
		struct ftrace_info *temp = current->ft_md_base->next;
		int found = 0;
		while (temp != NULL)
		{ // if syscall already exists
			if (temp->faddr == faddr)
			{
				u8 *faddr_ptr = (u8 *)faddr;
				for (int i = 0; i < 4; i++)
					*(faddr_ptr + i) = temp->code_backup[i];
				
				found = 1;
				break;
			}

			temp = temp->next;
		}
		if (!found)
			return -EINVAL;
	}
	else if (action == ENABLE_BACKTRACE)
	{
		struct ftrace_info *temp = current->ft_md_base->next;
		int found = 0;
		while (temp != NULL)
		{
			// Check if the syscall already exists
			if (temp->faddr == faddr)
			{
				u8 *faddr_ptr = (u8 *)faddr;
				for (int i = 0; i < 4; i++)
					temp->code_backup[i] = *(faddr_ptr + i);
				
				for (int i = 0; i < 4; i++)
					*(faddr_ptr + i) = (INV_OPCODE);
				
				temp->capture_backtrace = 1;
				found = 1;
				break;
			}
			temp = temp->next;
		}
		if (!found)
			return -EINVAL;
	}
	else if (action == DISABLE_BACKTRACE)
	{
		struct ftrace_info *temp = current->ft_md_base->next;
		int found = 0;
		while (temp != NULL)
		{
			// Check if the syscall already exists
			if (temp->faddr == faddr)
			{
				u8 *faddr_ptr = (u8 *)faddr;
				for (int i = 0; i < 4; i++)
					*(faddr_ptr + i) = temp->code_backup[i];

				temp->capture_backtrace = 0;
				found = 1;
				break;
			}
			temp = temp->next;
		}
		if (!found)
			return -EINVAL;
	}
	return 0;
}

// Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
	struct exec_context *current = get_current_ctx();
	struct ftrace_info *temp = current->ft_md_base->next;
	struct ftrace_info *curr_ftrace = NULL;
	long file_desc = -1;
	while (temp)
	{
		if (temp->faddr == regs->entry_rip)
		{
			curr_ftrace = temp;
			file_desc = temp->fd;
			break;
		}
		temp = temp->next;
	}
	if (temp == NULL)
		return -EINVAL;

	int nargs = curr_ftrace->num_args;
	struct file *filep = current->files[curr_ftrace->fd];
	u64 de_lim = nargs;

	// to count how many args are to be pushed as delimiter
	if (curr_ftrace->capture_backtrace)
	{
		de_lim++;
		u64 return_addr = *(u64 *)regs->entry_rsp;
		u64 rbp = regs->rbp;
		while (return_addr != END_ADDR)
		{
			de_lim++;
			return_addr = *(u64 *)(rbp + 8);
			rbp = *(u64 *)rbp;
		}
	}
	de_lim++;
	struct file *trace_buff = current->files[file_desc];

	// trace_buffer_write(filep, (char *)&de_lim, 8);
	*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = de_lim;
	trace_buff->trace_buffer->write_offset += sizeof(u64);
	trace_buff->trace_buffer->space_left -= sizeof(u64);
	trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;

	// trace_buffer_write(filep, (char *)&curr_ftrace->faddr, 8);
	*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = curr_ftrace->faddr;
	trace_buff->trace_buffer->write_offset += sizeof(u64);
	trace_buff->trace_buffer->space_left -= sizeof(u64);
	trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;

	if (nargs >= 1)
	{
		// trace_buffer_write(filep, (char *)&regs->rdi, 8);
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = regs->rdi;
		trace_buff->trace_buffer->write_offset += sizeof(u64);
		trace_buff->trace_buffer->space_left -= sizeof(u64);
		trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;
	}

	if (nargs >= 2)
	{
		// trace_buffer_write(filep, (char *)&regs->rsi, 8);
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = regs->rsi;
		trace_buff->trace_buffer->write_offset += sizeof(u64);
		trace_buff->trace_buffer->space_left -= sizeof(u64);
		trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;
	}

	if (nargs >= 3)
	{
		// trace_buffer_write(filep, (char *)&regs->rdx, 8);
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = regs->rdx;
		trace_buff->trace_buffer->write_offset += sizeof(u64);
		trace_buff->trace_buffer->space_left -= sizeof(u64);
		trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;
	}

	if (nargs >= 4)
	{
		// trace_buffer_write(filep, (char *)&regs->rcx, 8);
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = regs->rcx;
		trace_buff->trace_buffer->write_offset += sizeof(u64);
		trace_buff->trace_buffer->space_left -= sizeof(u64);
		trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;
	}

	if (nargs >= 5)
	{
		// trace_buffer_write(filep, (char *)&regs->r8, 8);
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = regs->r8;
		trace_buff->trace_buffer->write_offset += sizeof(u64);
		trace_buff->trace_buffer->space_left -= sizeof(u64);
		trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;
	}

	if (curr_ftrace->capture_backtrace)
	{
		// trace_buffer_write(filep, (char *)&curr_ftrace->faddr, 8);
		*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = curr_ftrace->faddr;
		trace_buff->trace_buffer->write_offset += sizeof(u64);
		trace_buff->trace_buffer->space_left -= sizeof(u64);
		trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;
		
		u64 return_addr = *(u64 *)regs->entry_rsp;
		u64 rbp = regs->rbp;
		while (return_addr != END_ADDR)
		{
			// trace_buffer_write(filep, (char *)&return_addr, 8);
			*(u64 *)(trace_buff->trace_buffer->buff + trace_buff->trace_buffer->write_offset) = return_addr;
			trace_buff->trace_buffer->write_offset += sizeof(u64);
			trace_buff->trace_buffer->space_left -= sizeof(u64);
			trace_buff->trace_buffer->write_offset %= TRACE_BUFFER_MAX_SIZE;
			
			return_addr = *(u64 *)(rbp + 8);
			rbp = *(u64 *)rbp;
		}
	}
	regs->entry_rsp -= 8;
	*((u64 *)regs->entry_rsp) = regs->rbp;
	regs->rbp = regs->entry_rsp;
	regs->entry_rip += 4;
	return 0;
}

int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
	if (buff == NULL || filep == NULL || filep->type != TRACE_BUFFER || filep->trace_buffer == NULL)
		return -EINVAL;
	
	int i, read, j;
	int bytes = 0;

	u64 num_of_params;
	u64 params_arr[550];
	for (i = 0; i < count && filep->trace_buffer->space_left != 4096; i++)
	{
		num_of_params = *(u64 *)(filep->trace_buffer->buff + filep->trace_buffer->read_offset);

		for (j = 1; j <= num_of_params; j++)
		{
			params_arr[j - 1] = *(u64 *)(filep->trace_buffer->buff + filep->trace_buffer->read_offset + j * sizeof(u64));
		}
		filep->trace_buffer->read_offset = (filep->trace_buffer->read_offset + (num_of_params + 1) * sizeof(u64)) % TRACE_BUFFER_MAX_SIZE;
		filep->trace_buffer->space_left = filep->trace_buffer->space_left + (num_of_params + 1) * sizeof(u64);
		for (int j = 0; j < num_of_params; j++)
		{
			*(u64 *)(buff + j * sizeof(u64)) = params_arr[j];
		}
		buff = buff + (num_of_params) * sizeof(u64);
		bytes += (num_of_params) * sizeof(u64);
	}
	return bytes;
}