
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018 by Omar Mohammad
 */

#include "lai.h"

// acpi_is_name(): Evaluates a name character
// Param:	char character - character from name
// Return:	int - 1 if it's a name, 0 if it's not

int acpi_is_name(char character)
{
	if((character >= '0' && character <= 'Z') || character == '_' || character == ROOT_CHAR || character == PARENT_CHAR || character == MULTI_PREFIX || character == DUAL_PREFIX)
		return 1;

	else
		return 0;
}

// acpi_eval_integer(): Evaluates an integer object
// Param:	uint8_t *object - pointer to object
// Param:	uint64_t *integer - destination
// Return:	size_t - size of object in bytes, 0 if it's not an integer

size_t acpi_eval_integer(uint8_t *object, uint64_t *integer)
{
	uint8_t *byte = (uint8_t*)(object + 1);
	uint16_t *word = (uint16_t*)(object + 1);
	uint32_t *dword = (uint32_t*)(object + 1);
	uint64_t *qword = (uint64_t*)(object + 1);

	switch(object[0])
	{
	case ZERO_OP:
		integer[0] = 0;
		return 1;
	case ONE_OP:
		integer[0] = 1;
		return 1;
	case ONES_OP:
		integer[0] = 0xFFFFFFFFFFFFFFFF;
		return 1;
	case BYTEPREFIX:
		integer[0] = (uint64_t)byte[0];
		return 2;
	case WORDPREFIX:
		integer[0] = (uint64_t)word[0];
		return 3;
	case DWORDPREFIX:
		integer[0] = (uint64_t)dword[0];
		return 5;
	case QWORDPREFIX:
		integer[0] = qword[0];
		return 9;
	default:
		return 0;
	}
}

// acpi_parse_pkgsize(): Parses package size
// Param:	uint8_t *data - pointer to package size data
// Param:	size_t *destination - destination to store package size
// Return:	size_t - size of package size encoding

size_t acpi_parse_pkgsize(uint8_t *data, size_t *destination)
{
	destination[0] = 0;

	uint8_t bytecount = (data[0] >> 6) & 3;
	if(bytecount == 0)
		destination[0] = (size_t)(data[0] & 0x3F);
	else if(bytecount == 1)
	{
		destination[0] = (size_t)(data[0] & 0x0F);
		destination[0] |= (size_t)(data[1] << 4);
	} else if(bytecount == 2)
	{
		destination[0] = (size_t)(data[0] & 0x0F);
		destination[0] |= (size_t)(data[1] << 4);
		destination[0] |= (size_t)(data[2] << 12);
	} else if(bytecount == 3)
	{
		destination[0] = (size_t)(data[0] & 0x0F);
		destination[0] |= (size_t)(data[1] << 4);
		destination[0] |= (size_t)(data[2] << 12);
		destination[0] |= (size_t)(data[3] << 20);
	}

	return (size_t)(bytecount + 1);
}

// acpi_eval_package(): Evaluates a package
// Param:	acpi_object_t *package - pointer to package object
// Param:	size_t index - index to evaluate
// Param:	acpi_object_t *destination - where to store value
// Return:	int - 0 on success

int acpi_eval_package(acpi_object_t *package, size_t index, acpi_object_t *destination)
{
	if(index >= package->package_size)
		return 1;

	acpi_memcpy(destination, &package->package[index], sizeof(acpi_object_t));
	return 0;
}




