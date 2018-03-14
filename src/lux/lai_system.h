
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018 by Omar Mohammad
 */

/* lux-specific OS functions */

#pragma once

#include <stdint.h>
#include <lock.h>
#include <kprintf.h>

#define acpi_printf kprintf

typedef lock_t acpi_lock_t;



