#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>

#define M_B 1048576 // Define a constant for megabytes

void *head = NULL;

void *memalloc(unsigned long size)
{
	if (size == 0)
		return NULL;

	if (size < 16)
		size = 16; // To make the minimumn size allocated = 24 Byte (including the size)
	else
		size = ((size + 7) / 8) * 8; // padding

	size = size + 8; // +8 to store the memory size

	if (head == NULL)
	{
		head = mmap(NULL, 4 * M_B, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); /*gcc shutup*/

		if (head == MAP_FAILED) // check if mmap failed
		{
			perror("mmap failed");
			return NULL;
		}
		*(unsigned long *)head = 4 * M_B;
		*(void **)(head + 8) = NULL;  // next
		*(void **)(head + 16) = NULL; // prev
	}

	void *temp = head;
	while (temp != NULL && *(unsigned long *)temp < size)
	{
		temp = *(void **)(temp + 8);
	}

	if (temp == NULL)
	{
		unsigned long count = size / (4 * M_B) + 1;
		void *newmem = mmap(NULL, count * (4 * M_B), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); /*gcc shutup*/

		if (newmem == MAP_FAILED) // check if mmap failed
		{
			perror("mmap failed");
			return NULL;
		}

		if (count * (4 * M_B) - size < 24)
		{
			*(long unsigned *)newmem = count * (4 * M_B);
		}
		else
		{
			*(long unsigned *)newmem = size;
			void *newtemp = (void *)(newmem + size);
			*(unsigned long *)newtemp = count * (4 * M_B) - size;

			// Add the newtemp at the start of the free memory list
			*(void **)(newtemp + 16) = NULL; // newtemp->prev
			*(void **)(newtemp + 8) = head;	 // newtemp->next
			*(void **)(head + 16) = newtemp; // temp->prev
			head = newtemp;
		}
		return (void *)(newmem + 8);
	}
	else
	{
		void *tempprev = *(void **)(temp + 16);
		void *tempnext = *(void **)(temp + 8);

		if (*(unsigned long *)temp - size < 24) // completely allocate the free memory block
		{
			// size is already stored
			if (tempprev == NULL)
			{
				head = tempnext;
				if (tempnext != NULL)
					*(void **)(tempnext + 16) = NULL; // tempnext->prev
			}
			else
			{
				*(void **)(tempprev + 8) = tempnext; // tempprev->next
				if (tempnext != NULL)
					*(void **)(tempnext + 16) = tempprev; // tempnext->prev
			}
			return (void *)(temp + 8);
		}
		else
		{
			void *newtemp = (void *)(temp + size);
			*(unsigned long *)newtemp = *(unsigned long *)temp - size;
			*(unsigned long *)temp = size;

			// newtemp added to the front of the free list
			if (tempprev == NULL)
			{
				*(void **)(newtemp + 16) = NULL;	// newtemp->prev
				*(void **)(newtemp + 8) = tempnext; // newtemp->next
				head = newtemp;

				if (tempnext != NULL)
					*(void **)(tempnext + 16) = newtemp; // tempnext->prev
			}
			else
			{
				*(void **)(tempprev + 8) = tempnext; // tempprev->next
				*(void **)(newtemp + 8) = head;		 // newtemp->next
				*(void **)(newtemp + 16) = NULL;	 // newtemp->prev
				*(void **)(head + 16) = newtemp;	 // head->prev
				head = newtemp;

				if (tempnext != NULL)
					*(void **)(tempnext + 16) = tempprev; // tempnext->prev
			}
			return (void *)(temp + 8);
		}
	}
	return NULL;
}

int memfree(void *ptr)
{
	if (ptr == NULL || ptr - 8 == NULL)
		return -1;

	ptr = ptr - 8;

	void *temp_left = head;
	while (temp_left != NULL && (temp_left + *(unsigned long *)(temp_left)) != ptr)
	{
		temp_left = *(void **)(temp_left + 8); // temp_left = temp_left->next
	}

	void *temp_right = head;
	while (temp_right != NULL && (ptr + *(unsigned long *)(ptr)) != temp_right)
	{
		temp_right = *(void **)(temp_right + 8); // temp_right->next
	}

	if (temp_left != NULL)
	{
		void *left = *(void **)(temp_left + 16); // temp_left->prev
		void *right = *(void **)(temp_left + 8); // temp_left->next
		*(unsigned long *)(temp_left) += (*(unsigned long *)(ptr));
		ptr = temp_left;
		if (left != NULL)
		{
			*(void **)(left + 8) = right;
		}
		if (right != NULL)
		{
			*(void **)(right + 16) = left;
		}
	}
	if (temp_right != NULL)
	{
		void *left = *(void **)(temp_right + 16); // temp_right->prev
		void *right = *(void **)(temp_right + 8); // temp_right->next
		*(unsigned long *)(head) += (*(unsigned long *)(temp_right));
		if (left != NULL)
		{
			*(void **)(left + 8) = right;
		}
		if (right != NULL)
		{
			*(void **)(right + 16) = left;
		}
	}

	*(void **)(ptr + 8) = head;
	*(void **)(ptr + 16) = NULL;
	head = ptr;

	return 0;
}
