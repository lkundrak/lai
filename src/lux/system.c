
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018 by Omar Mohammad
 */

/* lux-specific OS functions */

#include <stdint.h>		// lux kernel headers
#include <string.h>
#include <kprintf.h>
#include <mm.h>
#include <io.h>
#include <lock.h>
#include <pci.h>
#include <timer.h>

// Any OS using lai must provide implementations of the following functions

void *acpi_memcpy(void *dest, const void *src, size_t count)
{
	return memcpy(dest, src, count);
}

void *acpi_memmove(void *dest, const void *src, size_t count)
{
	return memmove(dest, src, count);
}

char *acpi_strcpy(char *dest, const char *src)
{
	return strcpy(dest, src);
}

void *acpi_malloc(size_t count)
{
	return kmalloc(count);
}

void *acpi_realloc(void *ptr, size_t count)
{
	return krealloc(ptr, count);
}

void *acpi_calloc(size_t n, size_t size)
{
	return kcalloc(n, size);
}

void acpi_free(void *ptr)
{
	kfree(ptr);
}

void *acpi_map(size_t physical, size_t count)
{
	count += PAGE_SIZE - 1;
	count >>= PAGE_SIZE_SHIFT;
	return (void*)vmm_request_map(physical, count, PAGE_PRESENT | PAGE_RW | PAGE_UNCACHEABLE);
}

size_t acpi_strlen(const char *string)
{
	return strlen(string);
}

void *acpi_memset(void *dest, int val, size_t count)
{
	return memset(dest, val, count);
}

int acpi_memcmp(const void *m1, const void *m2, size_t count)
{	
	return memcmp(m1, m2, count);
}

int acpi_strcmp(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}

void acpi_outb(uint16_t port, uint8_t data)
{
	outb(port, data);
}

void acpi_outw(uint16_t port, uint16_t data)
{
	outw(port, data);
}

void acpi_outd(uint16_t port, uint32_t data)
{
	outd(port, data);
}

uint8_t acpi_inb(uint16_t port)
{
	return inb(port);
}

uint16_t acpi_inw(uint16_t port)
{
	return inw(port);
}

uint32_t acpi_ind(uint16_t port)
{
	return ind(port);
}

void acpi_pci_write(uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset, uint32_t data)
{
	pci_device_t device;
	device.bus = bus;
	device.slot = slot;
	device.function = function;

	pci_write(&device, offset, data);
}

uint32_t acpi_pci_read(uint8_t bus, uint8_t slot, uint8_t function, uint16_t offset)
{
	pci_device_t device;
	device.bus = bus;
	device.slot = slot;
	device.function = function;

	return pci_read(&device, offset);
}

void acpi_sleep(uint64_t time)
{
	timer_sleep(time);
}





