
#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/*
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables
 * */
#define KB 1024

/**
 * mprotect System call Implementation.
 */

/**
 * mmap system call implementation.
 */

long vm_areafree(struct exec_context *current, u64 addr, int length)
{

    // do error checking for -1 return
    if (length % 4 * KB != 0)
    {
        length = (length / 4 * KB + 1) * 4 * KB;
    }

    struct vm_area *vm_head = current->vm_area;
    if (vm_head == NULL)
    {
        vm_head = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        vm_head->vm_start = MMAP_AREA_START;
        vm_head->vm_end = MMAP_AREA_START + 4 * KB;
        vm_head->access_flags = 0;
        vm_head->vm_next = NULL;
        current->vm_area = vm_head;
        stats->num_vm_area++;
    }
    struct vm_area *tt = vm_head;
    int sub_count = 0;
    while (tt != NULL)
    {
        sub_count++;
        tt = tt->vm_next;
    }
    stats->num_vm_area -= sub_count;

    while (length > 0)
    {
        struct vm_area *temp = vm_head;
        int found = 0;
        while (temp != NULL)
        {
            if (temp->vm_start <= addr && temp->vm_end >= addr + 4 * KB)
            {
                found = 1;
                break;
            }
            temp = temp->vm_next;
        }
        if (found)
        {
            if (temp->vm_start == addr)
            { // area to unmap is the left-most
                temp->vm_start += 4 * KB;
            }
            else
            {
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                new_node->vm_next = temp->vm_next;
                temp->vm_next = new_node;
                new_node->vm_start = addr + 4 * KB;
                new_node->vm_end = temp->vm_end;
                temp->vm_end = addr;
                new_node->access_flags = temp->access_flags;
            }
        }
        temp = vm_head;

        while (temp->vm_next != NULL)
        {
            if (temp->vm_next->vm_start == temp->vm_next->vm_end)
            {
                temp->vm_next = temp->vm_next->vm_next;
            }

            temp = temp->vm_next;
        }

        addr += 4 * KB;
        length -= 4 * KB;
    }
    struct vm_area *tip = vm_head;
    int vm_count = 0;
    while (tip != NULL)
    {

        vm_count++;
        tip = tip->vm_next;
    }
    stats->num_vm_area += vm_count;

    return 0;
}

long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{

    if (length % 4 * KB != 0)
    {
        length = (length / 4 * KB + 1) * 4 * KB;
    }
    u64 answer;
    struct vm_area *vm_head = current->vm_area;
    if (vm_head == NULL)
    {
        vm_head = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        vm_head->vm_start = MMAP_AREA_START;
        vm_head->vm_end = MMAP_AREA_START + 4 * KB;
        vm_head->access_flags = 0;
        vm_head->vm_next = NULL;
        current->vm_area = vm_head;
        stats->num_vm_area++;
    }
    struct vm_area *tt = vm_head;
    int sub_count = 0;
    while (tt != NULL)
    {
        sub_count++;
        tt = tt->vm_next;
    }
    stats->num_vm_area -= sub_count;
    if (addr == 0)
    {
        if (flags == MAP_FIXED)
        {
            stats->num_vm_area += sub_count;
            return -1;
        }
        struct vm_area *temp = vm_head;
        while (temp->vm_next != NULL)
        {
            if (temp->vm_next->vm_start - temp->vm_end >= length)
            {
                break;
            }
            temp = temp->vm_next;
        }
        if (temp->vm_next != NULL)
        { // suitable space found
            if (temp->access_flags == prot)
            { // left-mergeable
                answer = temp->vm_end;
                temp->vm_end += length;
                if (temp->vm_next->vm_start == temp->vm_end && temp->vm_next->access_flags == prot)
                { // right-mergeable
                    temp->vm_end = temp->vm_next->vm_end;
                    temp->vm_next = temp->vm_next->vm_next;
                }
                temp = vm_head;
                int vm_count = 0;
                while (temp != NULL)
                {

                    temp = temp->vm_next;
                    vm_count++;
                }
                stats->num_vm_area += vm_count;
                return answer;
            }
            else
            { // not left-mergeable
                answer = temp->vm_end;
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                new_node->vm_next = temp->vm_next;
                temp->vm_next = new_node;
                new_node->vm_start = temp->vm_end;
                new_node->vm_end = temp->vm_end + length;
                new_node->access_flags = prot;
                if (new_node->vm_next->vm_start == new_node->vm_end && new_node->access_flags == new_node->vm_next->access_flags)
                { // new_node is right-mergeable
                    new_node->vm_end = new_node->vm_next->vm_end;
                    new_node->vm_next = new_node->vm_next->vm_next;
                }
                temp = vm_head;
                int vm_count = 0;
                while (temp != NULL)
                {

                    temp = temp->vm_next;
                    vm_count++;
                }
                stats->num_vm_area += vm_count;
                return answer;
            }
        }
        else
        {
            temp = vm_head;
            while (temp->vm_next != NULL)
            {
                temp = temp->vm_next;
            }
            if (temp->access_flags == prot)
            { // left-mergeable with the last node
                answer = temp->vm_end;
                temp->vm_end += length;
                temp = vm_head;
                int vm_count = 0;
                while (temp != NULL)
                {

                    temp = temp->vm_next;
                    vm_count++;
                }
                stats->num_vm_area += vm_count;
                return answer;
            }
            else
            { // not left-mergeable with the last node, no question abt right-mergeable
                answer = temp->vm_end;
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                new_node->vm_next = temp->vm_next;
                temp->vm_next = new_node;
                new_node->vm_start = temp->vm_end;
                new_node->vm_end = temp->vm_end + length;
                new_node->access_flags = prot;
                temp = vm_head;
                int vm_count = 0;
                while (temp != NULL)
                {
                    temp = temp->vm_next;
                    vm_count++;
                }
                stats->num_vm_area += vm_count;

                return answer;
            }
        }
    }

    else
    { // addr != NULL
        if (flags == MAP_FIXED)
        {
            struct vm_area *temp = vm_head;
            while (temp != NULL)
            {
                if (temp->vm_next != NULL && temp->vm_next->vm_start >= addr + length && temp->vm_end <= addr)
                { // found suitable space
                    if (addr == temp->vm_end && temp->access_flags == prot)
                    { // mergeable with left
                        temp->vm_end += length;
                        if (addr + length == temp->vm_next->vm_start && temp->vm_next->access_flags == prot)
                        { // mergeable with left AND right
                            temp->vm_end = temp->vm_next->vm_end;
                            temp->vm_next = temp->vm_next->vm_next;

                            // also free temp->vm_next
                            temp = vm_head;
                            int vm_count = 0;
                            while (temp != NULL)
                            {

                                temp = temp->vm_next;
                                vm_count++;
                            }
                            stats->num_vm_area += vm_count;
                            return addr;
                        }
                    }
                    else if (addr + length == temp->vm_next->vm_start && temp->vm_next->access_flags == prot)
                    { // mergeable with right ONLY, and not left
                        temp->vm_next->vm_start = addr;
                        temp = vm_head;
                        int vm_count = 0;
                        while (temp != NULL)
                        {

                            temp = temp->vm_next;
                            vm_count++;
                        }
                        stats->num_vm_area += vm_count;
                        return addr;
                    }
                    else
                    { // addr space not mergeable
                        struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                        new_node->vm_next = temp->vm_next;
                        temp->vm_next = new_node;
                        new_node->vm_start = addr;
                        new_node->vm_end = addr + length;
                        new_node->access_flags = prot;
                        temp = vm_head;
                        int vm_count = 0;
                        while (temp != NULL)
                        {

                            temp = temp->vm_next;
                            vm_count++;
                        }
                        stats->num_vm_area += vm_count;
                        return addr;
                    }
                }
                else if (temp->vm_end > addr && temp->vm_start <= addr)
                { // addr lies in between an allocated region
                    stats->num_vm_area += sub_count;
                    return -1;
                }
                else if (temp->vm_next != NULL && temp->vm_end <= addr && addr < temp->vm_next->vm_start && addr + length > temp->vm_next->vm_start)
                { // addr lies in free, but length not sufficient
                    stats->num_vm_area += sub_count;
                    return -1;
                }
                else
                { // suitable space not found, but addr isn't yet wrong, so move ahead
                    temp = temp->vm_next;
                }
            }
            // suitable space not found
            temp = vm_head;
            while (temp->vm_next != NULL)
            { // find last node of vm_area
                temp = temp->vm_next;
            }
            if (temp->vm_end == addr && temp->access_flags == prot)
            { // left-mergeable with last node
                temp->vm_end += length;
                temp = vm_head;
                int vm_count = 0;
                while (temp != NULL)
                {

                    temp = temp->vm_next;
                    vm_count++;
                }
                stats->num_vm_area += vm_count;
                return addr;
            }
            else
            { // not left-mergeable with last node
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                new_node->vm_next = temp->vm_next;
                temp->vm_next = new_node;
                new_node->vm_start = addr;
                new_node->vm_end = addr + length;
                new_node->access_flags = prot;
                temp = vm_head;
                int vm_count = 0;
                while (temp != NULL)
                {

                    temp = temp->vm_next;
                    vm_count++;
                }
                stats->num_vm_area += vm_count;
                return addr;
            }
        }
        else if (flags == 0)
        {
            struct vm_area *temp = vm_head;
            int addr_found = 0;
            while (temp != NULL)
            {
                if (temp->vm_next != NULL && temp->vm_next->vm_start >= addr + length && temp->vm_end <= addr)
                { // found suitable space

                    if (addr == temp->vm_end && temp->access_flags == prot)
                    { // mergeable with left
                        temp->vm_end += length;
                        if (addr + length == temp->vm_next->vm_start && temp->vm_next->access_flags == prot)
                        { // mergeable with left AND right
                            temp->vm_end = temp->vm_next->vm_end;
                            temp->vm_next = temp->vm_next->vm_next;
                        }
                        temp = vm_head;
                        int vm_count = 0;
                        while (temp != NULL)
                        {

                            temp = temp->vm_next;
                            vm_count++;
                        }
                        stats->num_vm_area += vm_count;
                        return addr;
                    }
                    else if (addr + length == temp->vm_next->vm_start && temp->vm_next->access_flags == prot)
                    { // mergeable with right ONLY, and not left
                        temp->vm_next->vm_start = addr;
                        temp = vm_head;
                        int vm_count = 0;
                        while (temp != NULL)
                        {

                            temp = temp->vm_next;
                            vm_count++;
                        }
                        stats->num_vm_area += vm_count;
                        return addr;
                    }
                    else
                    { // addr space not mergeable
                        struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                        new_node->vm_next = temp->vm_next;
                        temp->vm_next = new_node;
                        new_node->vm_start = addr;
                        new_node->vm_end = addr + length;
                        new_node->access_flags = prot;
                        temp = vm_head;
                        int vm_count = 0;
                        while (temp != NULL)
                        {

                            temp = temp->vm_next;
                            vm_count++;
                        }
                        stats->num_vm_area += vm_count;
                        return addr;
                    }
                }
                if (temp->vm_start <= addr && temp->vm_end > addr)
                {
                    addr_found = 1;
                    break;
                }
                if (temp->vm_next != NULL && temp->vm_end <= addr && temp->vm_next->vm_start > addr)
                {
                    addr_found = 1;
                    break;
                }
                temp = temp->vm_next;
            }
            if (!addr_found)
            {
                temp = vm_head;
                while (temp->vm_next != NULL)
                {
                    temp = temp->vm_next;
                }
                if (temp->vm_end == addr && prot == temp->access_flags)
                {
                    temp->vm_end += length;
                    temp = vm_head;
                    int vm_count = 0;
                    while (temp != NULL)
                    {

                        temp = temp->vm_next;
                        vm_count++;
                    }
                    stats->num_vm_area += vm_count;
                    return addr;
                }
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                new_node->vm_next = temp->vm_next;
                temp->vm_next = new_node;
                new_node->vm_start = addr;
                new_node->vm_end = addr + length;
                new_node->access_flags = prot;
                temp = vm_head;
                int vm_count = 0;
                while (temp != NULL)
                {

                    temp = temp->vm_next;
                    vm_count++;
                }
                stats->num_vm_area += vm_count;
                return addr;
            }
            while (temp->vm_next != NULL)
            { // first search for valid space ka temp
                if (temp->vm_next->vm_start - temp->vm_end >= length)
                {
                    break;
                }
                temp = temp->vm_next;
            }
            if (temp->vm_next != NULL)
            { // suitable space found
                if (temp->access_flags == prot)
                { // left-mergeable
                    answer = temp->vm_end;
                    temp->vm_end += length;
                    if (temp->vm_next->vm_start == temp->vm_end && temp->vm_next->access_flags == prot)
                    { // right-mergeable too
                        temp->vm_end = temp->vm_next->vm_end;
                        temp->vm_next = temp->vm_next->vm_next;
                    }
                    temp = vm_head;
                    int vm_count = 0;
                    while (temp != NULL)
                    {

                        temp = temp->vm_next;
                        vm_count++;
                    }
                    stats->num_vm_area += vm_count;
                    return answer;
                }
                else
                { // not left-mergeable
                    answer = temp->vm_end;
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    new_node->vm_next = temp->vm_next;
                    temp->vm_next = new_node;
                    new_node->vm_start = temp->vm_end;
                    new_node->vm_end = temp->vm_end + length;
                    new_node->access_flags = prot;
                    if (new_node->vm_next->vm_start == new_node->vm_end && new_node->access_flags == new_node->vm_next->access_flags)
                    { // new_node is right-mergeable
                        new_node->vm_end = new_node->vm_next->vm_end;
                        new_node->vm_next = new_node->vm_next->vm_next;
                    }
                    temp = vm_head;
                    int vm_count = 0;
                    while (temp != NULL)
                    {

                        temp = temp->vm_next;
                        vm_count++;
                    }
                    stats->num_vm_area += vm_count;
                    return answer;
                }
            }
            else
            { // temp->vm_next == NULL
                temp = vm_head;
                while (temp->vm_next != NULL)
                {
                    temp = temp->vm_next;
                }
                if (temp->access_flags == prot)
                { // left-mergeable with the last node
                    answer = temp->vm_end;
                    temp->vm_end += length;
                    temp = vm_head;
                    int vm_count = 0;
                    while (temp != NULL)
                    {

                        temp = temp->vm_next;
                        vm_count++;
                    }
                    stats->num_vm_area += vm_count;
                    return answer;
                }
                else
                { // not left-mergeable with the last node, no question abt right-mergeable
                    answer = temp->vm_end;
                    struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                    new_node->vm_next = temp->vm_next;
                    temp->vm_next = new_node;
                    new_node->vm_start = temp->vm_end;
                    new_node->vm_end = temp->vm_end + length;
                    new_node->access_flags = prot;
                    temp = vm_head;
                    int vm_count = 0;
                    while (temp != NULL)
                    {

                        temp = temp->vm_next;
                        vm_count++;
                    }
                    stats->num_vm_area += vm_count;
                    return answer;
                }
            }
        }
    }
    stats->num_vm_area += sub_count;
    return -EINVAL;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{

    if (length % 4 * KB != 0)
    {
        length = (length / 4 * KB + 1) * 4 * KB;
    }
    struct vm_area *vm_head = current->vm_area;
    struct vm_area *tt = vm_head;
    int sub_count = 0;
    while (tt != NULL)
    {
        sub_count++;
        tt = tt->vm_next;
    }
    stats->num_vm_area -= sub_count;
    if (vm_head == NULL)
    {
        vm_head = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        vm_head->vm_start = MMAP_AREA_START;
        vm_head->vm_end = MMAP_AREA_START + 4 * KB;
        vm_head->access_flags = 0;
        vm_head->vm_next = NULL;
        current->vm_area = vm_head;
        stats->num_vm_area++;
    }

    while (length > 0)
    {
        struct vm_area *temp = vm_head;
        int found = 0;
        u64 offset = 0;
        while (temp != NULL)
        {
            if (temp->vm_start <= addr && temp->vm_end >= addr + 4 * KB)
            {
                found = 1;
                break;
            }
            temp = temp->vm_next;
        }
        if (found)
        {
            if (temp->vm_start == addr)
            { // area to unmap is the left-most
                temp->vm_start += 4 * KB;
            }
            else
            {
                struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
                new_node->vm_next = temp->vm_next;
                temp->vm_next = new_node;
                new_node->vm_start = addr + 4 * KB;
                new_node->vm_end = temp->vm_end;
                temp->vm_end = addr;
                new_node->access_flags = temp->access_flags;
            }
            u64 *base_addr = osmap(current->pgd);
            u64 entry;
            u64 tempo_addr = addr << 16;
            for (int i = 0; i < 4; ++i)
            {
                u64 offset = tempo_addr >> 55;
                tempo_addr = tempo_addr << 9;
                entry = *(u64 *)(base_addr + offset);
                if (entry % 2 == 0)
                {
                    break;
                }
                else
                {
                    if (i == 3)
                    {

                        s8 ref_count = get_pfn_refcount(entry >> 12);

                        if (ref_count == 1)
                        {

                            put_pfn(entry >> 12);

                            os_pfn_free(USER_REG, (entry >> 12));
                        }
                        else
                        {

                            put_pfn(entry >> 12);
                        }

                        *(u64 *)(base_addr + offset) = 0;
                    }
                    base_addr = osmap((entry >> 12));
                }
            }
        }
        temp = vm_head;

        while (temp->vm_next != NULL)
        {
            if (temp->vm_next->vm_start == temp->vm_next->vm_end)
            {
                temp->vm_next = temp->vm_next->vm_next;
            }

            temp = temp->vm_next;
        }

        addr += 4 * KB;
        length -= 4 * KB;
    }
    struct vm_area *tip = vm_head;
    int vm_count = 0;
    while (tip != NULL)
    {

        vm_count++;
        tip = tip->vm_next;
    }
    stats->num_vm_area += vm_count;
    asm volatile("mov %cr3, %rax");
    asm volatile("mov %rax, %cr3");
    return 0;
}

long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    if (length % 4 * KB != 0)
    {
        length = (length / 4 * KB + 1) * 4 * KB;
    }
    addr -= addr % 4 * KB;
    struct vm_area *vm_head = current->vm_area;
    if (vm_head == NULL)
    {
        vm_head = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        vm_head->vm_start = MMAP_AREA_START;
        vm_head->vm_end = MMAP_AREA_START + 4 * KB;
        vm_head->access_flags = 0;
        vm_head->vm_next = NULL;
        current->vm_area = vm_head;
    }
    while (length > 0)
    {
        struct vm_area *temp = vm_head;
        int found = 0;
        while (temp != NULL)
        {
            if (temp->vm_start <= addr && temp->vm_end >= addr + 4 * KB)
            {
                found = 1;
                break;
            }
            temp = temp->vm_next;
        }
        if (found)
        {
            vm_areafree(current, addr, 4 * KB);
            vm_area_map(current, addr, 4 * KB, prot, MAP_FIXED);
            u64 *base_addr = osmap(current->pgd);
            u64 entry;
            u64 tempo_addr = addr << 16;
            for (int i = 0; i < 4; ++i)
            {
                u64 offset = tempo_addr >> 55;
                tempo_addr = tempo_addr << 9;
                entry = *(u64 *)(base_addr + offset);
                if (entry % 2 == 0)
                {
                    break;
                }
                else
                {
                    if (i == 3)
                    {
                        if (prot == (PROT_READ | PROT_WRITE))
                        {

                            entry = entry | (1 << 3);
                        }
                        else
                        {
                            if (entry & (1 << 3))
                            {

                                entry = entry ^ (1 << 3);
                            }
                        }
                        *(u64 *)(base_addr + offset) = entry;
                    }
                    base_addr = osmap((entry >> 12));
                }
            }
        }

        addr += 4 * KB;
        length -= 4 * KB;
    }
    asm volatile("mov %cr3, %rax");
    asm volatile("mov %rax, %cr3");
    return 0;
}
/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{

    struct vm_area *vm_head = current->vm_area;
    struct vm_area *temp = vm_head;
    while (temp != NULL)
    {
        if (temp->vm_start <= addr && temp->vm_end > addr)
        {
            break;
        }
        temp = temp->vm_next;
    }

    if (temp == NULL)
    {
        return -EINVAL;
    }
    if (error_code == 7)
    {

        if (temp->access_flags == (PROT_READ | PROT_WRITE))
        {

            handle_cow_fault(current, addr, temp->access_flags);
        }
        else
        {

            return -EINVAL;
        }

        return 1;
    }
    if (error_code == 6 && temp->access_flags == PROT_READ)
    {

        return -EINVAL;
    }

    if (current->pgd == NULL)
    {
        return -EINVAL;
    }
    u64 *pgd_base = osmap(current->pgd);

    u64 tempo_addr = addr;
    tempo_addr = tempo_addr << 16;
    u64 offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;
    u64 pud_entry = *(u64 *)(pgd_base + offset);

    if (pud_entry % 2 == 0)
    {
        u64 new_pfn = os_pfn_alloc(OS_PT_REG);

        if (get_pfn_refcount(new_pfn) == 0)
        {
            get_pfn(new_pfn);
        }
        pud_entry = new_pfn << 12;
        pud_entry = pud_entry | 1;
        pud_entry = pud_entry | (1 << 4);
        pud_entry = pud_entry | (1 << 3);
        *(u64 *)(pgd_base + offset) = pud_entry;
    }
    u64 *pud_base = osmap(pud_entry >> 12);

    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;

    u64 pmd_entry = *(u64 *)(pud_base + offset);
    if (pmd_entry % 2 == 0)
    {
        u64 new_pfn = os_pfn_alloc(OS_PT_REG);

        if (get_pfn_refcount(new_pfn) == 0)
        {
            get_pfn(new_pfn);
        }
        pmd_entry = new_pfn << 12;
        pmd_entry = pmd_entry | 1;
        pmd_entry = pmd_entry | (1 << 4);
        pmd_entry = pmd_entry | (1 << 3);
        *(u64 *)(pud_base + offset) = pmd_entry;
    }
    u64 *pmd_base = osmap(pmd_entry >> 12);
    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;

    u64 pte_entry = *(u64 *)(pmd_base + offset);

    if (pte_entry % 2 == 0)
    {
        u64 new_pfn = os_pfn_alloc(OS_PT_REG);

        if (get_pfn_refcount(new_pfn) == 0)
        {
            get_pfn(new_pfn);
        }
        pte_entry = new_pfn << 12;
        pte_entry = pte_entry | 1;
        pte_entry = pte_entry | (1 << 4);
        pte_entry = pte_entry | (1 << 3);
        *(u64 *)(pmd_base + offset) = pte_entry;
    }
    u64 *pte_base = osmap(pte_entry >> 12);
    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;
    u64 pfn_entry = *(u64 *)(pte_base + offset);
    if (pfn_entry % 2 == 0)
    {
        u64 new_pfn = os_pfn_alloc(USER_REG);

        if (get_pfn_refcount(new_pfn) == 0)
        {
            get_pfn(new_pfn);
        }
        pfn_entry = new_pfn << 12;
        pfn_entry = pfn_entry | 1;
        pfn_entry = pfn_entry | (1 << 4);
        if (temp->access_flags == (PROT_READ | PROT_WRITE))
        {
            pfn_entry = pfn_entry | (1 << 3);
        }
        *(u64 *)(pte_base + offset) = pfn_entry;
    }

    asm volatile("mov %cr3, %rax");
    asm volatile("mov %rax, %cr3");
    return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the
 * end of this function (e.g., setup_child_context etc.)
 */
u64 get_translation(struct exec_context *current, u64 addr)
{

    u64 *pgd_base = osmap(current->pgd);

    u64 tempo_addr = addr;
    tempo_addr = tempo_addr << 16;
    u64 offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;
    u64 pud_entry = *(u64 *)(pgd_base + offset);

    if (pud_entry % 2 == 0)
    {
        return 0;
    }
    u64 *pud_base = osmap(pud_entry >> 12);

    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;

    u64 pmd_entry = *(u64 *)(pud_base + offset);
    if (pmd_entry % 2 == 0)
    {
        return 0;
    }
    u64 *pmd_base = osmap(pmd_entry >> 12);
    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;

    u64 pte_entry = *(u64 *)(pmd_base + offset);

    if (pte_entry % 2 == 0)
    {
        return 0;
    }
    u64 *pte_base = osmap(pte_entry >> 12);
    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;
    u64 pfn_entry = *(u64 *)(pte_base + offset);
    if (pfn_entry & (1 << 3))
    {
        pfn_entry = pfn_entry ^ (1 << 3);
    }
    *(u64 *)(pte_base + offset) = pfn_entry;
    return pfn_entry;
}

long do_cfork()
{

    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
    /* Do not modify above lines
     *
     * */
    u32 cpid = new_ctx->pid;
    memcpy(new_ctx, ctx, sizeof(*ctx));
    new_ctx->pid = cpid;
    u64 new_ctx_pgdbase = os_pfn_alloc(OS_PT_REG);
    if (get_pfn_refcount(new_ctx_pgdbase) == 0)
    {
        get_pfn(new_ctx_pgdbase);
    }

    new_ctx->pgd = new_ctx_pgdbase;

    struct vm_area *vm_head = ctx->vm_area;
    if (vm_head != NULL)
    {

        struct vm_area *our_temp = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        memcpy(our_temp, vm_head, sizeof(*vm_head));
        struct vm_area *temp = vm_head->vm_next;
        while (temp != NULL)
        {
            struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
            memcpy(new_node, temp, sizeof(*temp));
            our_temp->vm_next = new_node;
            our_temp = new_node;
            temp = temp->vm_next;
        }
    }
    else
    {

        struct vm_area *new_node = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        new_node->vm_start = MMAP_AREA_START;
        new_node->vm_end = MMAP_AREA_START + 4 * KB;
        new_node->access_flags = 0;
        new_node->vm_next = NULL;
        ctx->vm_area = new_node;

        struct vm_area *new_node1 = (struct vm_area *)os_alloc(sizeof(struct vm_area));
        new_node1->vm_start = MMAP_AREA_START;
        new_node1->vm_end = MMAP_AREA_START + 4 * KB;
        new_node1->access_flags = 0;
        new_node1->vm_next = NULL;
        new_ctx->vm_area = new_node1;
    }

    struct vm_area *temp = ctx->vm_area;

    for (int i = 0; i < 4 || temp != NULL; i++)
    {

        u64 length, addr;
        if (i == 0)
        {
            length = ctx->mms[MM_SEG_CODE].next_free - ctx->mms[MM_SEG_CODE].start;
            addr = ctx->mms[MM_SEG_CODE].start;
        }
        if (i == 1)
        {
            length = ctx->mms[MM_SEG_RODATA].next_free - ctx->mms[MM_SEG_RODATA].start;
            addr = ctx->mms[MM_SEG_RODATA].start;
        }
        if (i == 2)
        {
            length = ctx->mms[MM_SEG_DATA].next_free - ctx->mms[MM_SEG_DATA].start;
            addr = ctx->mms[MM_SEG_DATA].start;
        }
        if (i == 3)
        {
            length = ctx->mms[MM_SEG_STACK].end - ctx->mms[MM_SEG_STACK].start;
            addr = ctx->mms[MM_SEG_STACK].start;
        }
        if (i > 3)
        {
            length = temp->vm_end - temp->vm_start;
            addr = temp->vm_start;
        }
        while (length > 0)
        {

            u64 *pgd_base = osmap(new_ctx->pgd);
            u64 *parent_pgd_base = osmap(ctx->pgd);

            u64 tempo_addr = addr;
            tempo_addr = tempo_addr << 16;
            u64 offset = tempo_addr >> 55;
            tempo_addr = tempo_addr << 9;
            u64 pud_entry = *(u64 *)(pgd_base + offset);
            u64 ppud_entry = *(u64 *)(parent_pgd_base + offset);

            if (ppud_entry % 2 == 1 && pud_entry == 0)
            {
                u64 new_pfn = os_pfn_alloc(OS_PT_REG);
                if (get_pfn_refcount(new_pfn) == 0)
                {
                    get_pfn(new_pfn);
                }
                pud_entry = new_pfn << 12;
                pud_entry = pud_entry | 1;
                pud_entry = pud_entry | (1 << 4);
                pud_entry = pud_entry | (1 << 3);
                *(u64 *)(pgd_base + offset) = pud_entry;
            }
            else if (ppud_entry % 2 == 1 && pud_entry % 2 == 1)
            {
            }
            else
            {
                addr += 4 * KB;
                length -= 4 * KB;
                continue;
            }
            u64 *pud_base = osmap(pud_entry >> 12);
            u64 *ppud_base = osmap(ppud_entry >> 12);

            offset = tempo_addr >> 55;
            tempo_addr = tempo_addr << 9;

            u64 pmd_entry = *(u64 *)(pud_base + offset);
            u64 ppmd_entry = *(u64 *)(ppud_base + offset);
            if (ppmd_entry % 2 == 1 && pmd_entry == 0)
            {
                u64 new_pfn = os_pfn_alloc(OS_PT_REG);
                if (get_pfn_refcount(new_pfn) == 0)
                {
                    get_pfn(new_pfn);
                }
                pmd_entry = new_pfn << 12;
                pmd_entry = pmd_entry | 1;
                pmd_entry = pmd_entry | (1 << 4);
                pmd_entry = pmd_entry | (1 << 3);
                *(u64 *)(pud_base + offset) = pmd_entry;
            }
            else if (ppmd_entry % 2 == 1 && pmd_entry % 2 == 1)
            {
            }
            else
            {
                addr += 4 * KB;
                length -= 4 * KB;
                continue;
            }
            u64 *pmd_base = osmap(pmd_entry >> 12);
            u64 *ppmd_base = osmap(ppmd_entry >> 12);
            offset = tempo_addr >> 55;
            tempo_addr = tempo_addr << 9;

            u64 pte_entry = *(u64 *)(pmd_base + offset);
            u64 ppte_entry = *(u64 *)(ppmd_base + offset);
            if (ppte_entry % 2 == 1 && pte_entry == 0)
            {
                u64 new_pfn = os_pfn_alloc(OS_PT_REG);
                if (get_pfn_refcount(new_pfn) == 0)
                {
                    get_pfn(new_pfn);
                }
                pte_entry = new_pfn << 12;
                pte_entry = pte_entry | 1;
                pte_entry = pte_entry | (1 << 4);
                pte_entry = pte_entry | (1 << 3);
                *(u64 *)(pmd_base + offset) = pte_entry;
            }
            else if (ppte_entry % 2 == 1 && pte_entry % 2 == 1)
            {
            }
            else
            {
                addr += 4 * KB;
                length -= 4 * KB;
                continue;
            }
            u64 *pte_base = osmap(pte_entry >> 12);

            offset = tempo_addr >> 55;
            tempo_addr = tempo_addr << 9;

            u64 new_pfn = get_translation(ctx, addr);

            if (new_pfn % 2 != 0)
            {
                get_pfn(new_pfn >> 12);
            }
            *(u64 *)(pte_base + offset) = new_pfn;

            addr += 4 * KB;
            length -= 4 * KB;
        }

        if (i > 3)
        {
            temp = temp->vm_next;
        }
    }
    new_ctx->ppid = ctx->pid;
    struct vm_area *tip = vm_head;
    int vm_count = 0;
    while (tip != NULL)
    {

        vm_count++;
        tip = tip->vm_next;
    }
    stats->num_vm_area += vm_count;
    /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);

    return cpid;
}

/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data)
 * it is called when there is a CoW violation in these areas.
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{

    u64 *pgd_base = osmap(current->pgd);

    u64 tempo_addr = vaddr;
    tempo_addr = tempo_addr << 16;
    u64 offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;
    u64 pud_entry = *(u64 *)(pgd_base + offset);

    u64 *pud_base = osmap(pud_entry >> 12);

    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;

    u64 pmd_entry = *(u64 *)(pud_base + offset);

    u64 *pmd_base = osmap(pmd_entry >> 12);
    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;

    u64 pte_entry = *(u64 *)(pmd_base + offset);

    u64 *pte_base = osmap(pte_entry >> 12);
    offset = tempo_addr >> 55;
    tempo_addr = tempo_addr << 9;
    u64 pfn_entry = *(u64 *)(pte_base + offset);
    u64 old_pfn_addr = (pfn_entry >> 12) << 12;
    put_pfn(pfn_entry >> 12);
    u64 new_pfn = os_pfn_alloc(USER_REG);
    if (get_pfn_refcount(new_pfn) == 0)
    {
        get_pfn(new_pfn);
    }

    pfn_entry = new_pfn << 12;
    pfn_entry = pfn_entry | (1);

    if (access_flags == (PROT_READ | PROT_WRITE))
    {
        pfn_entry = pfn_entry | (1 << 3);
    }
    pfn_entry = pfn_entry | (1 << 4);
    *(u64 *)(pte_base + offset) = pfn_entry;

    memcpy((char *)((pfn_entry >> 12) << 12), (char *)old_pfn_addr, 4 * KB);

    return 1;
}