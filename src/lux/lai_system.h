
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018 by Omar Mohammad
 */

/* lux-specific OS functions */

#pragma once

#include <stdint.h>
#include <lock.h>
#include <kprintf.h>
#include <va_list.h>
#include <tty.h>

#define acpi_printf		kprintf

#define acpi_panic(...)		tty_switch(0); \
				debug_mode = 1; \
				kprintf(__VA_ARGS__); \
				while(1);

typedef lock_t acpi_lock_t;



