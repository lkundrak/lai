
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018 by Omar Mohammad
 */

#include "lai.h"

/* ACPI Control Method Execution */
/* Type2Opcode := DefAcquire | DefAdd | DefAnd | DefBuffer | DefConcat |
   DefConcatRes | DefCondRefOf | DefCopyObject | DefDecrement |
   DefDerefOf | DefDivide | DefFindSetLeftBit | DefFindSetRightBit |
   DefFromBCD | DefIncrement | DefIndex | DefLAnd | DefLEqual |
   DefLGreater | DefLGreaterEqual | DefLLess | DefLLessEqual | DefMid |
   DefLNot | DefLNotEqual | DefLoadTable | DefLOr | DefMatch | DefMod |
   DefMultiply | DefNAnd | DefNOr | DefNot | DefObjectType | DefOr |
   DefPackage | DefVarPackage | DefRefOf | DefShiftLeft | DefShiftRight |
   DefSizeOf | DefStore | DefSubtract | DefTimer | DefToBCD | DefToBuffer |
   DefToDecimalString | DefToHexString | DefToInteger | DefToString |
   DefWait | DefXOr | UserTermObj */

// acpi_exec_resolve(): Resolves a name during control method execution
// Param:	char *path - 4-char object name or full path
// Return:	acpi_handle_t * - pointer to namespace object, NULL on error

acpi_handle_t *acpi_exec_resolve(char *path)
{
	acpi_handle_t *object;
	object = acpins_resolve(path);
	if(!object)
	{
		if(acpi_strlen(path) <= 6)
			return NULL;

		// because method scopes include the method and its parent's too
		acpi_memmove(path + acpi_strlen(path) - 9, path + acpi_strlen(path) - 4, 5);
		object = acpins_resolve(path);
		if(!object)
		{
			acpi_memmove(path + 2, path + acpi_strlen(path) - 4, 5);
			object = acpins_resolve(path);
			if(!object)
				return NULL;
		}
	}

	// resolve Aliases too
	while(object->type == ACPI_NAMESPACE_ALIAS)
	{
		object = acpins_resolve(object->alias);
		if(!object)
			return NULL;
	}

	return object;
}

// acpi_copy_object(): Copies an object
// Param:	acpi_object_t *destination - destination
// Param:	acpi_object_t *source - source
// Return:	Nothing

void acpi_copy_object(acpi_object_t *destination, acpi_object_t *source)
{
	acpi_memcpy(destination, source, sizeof(acpi_object_t));
}

// acpi_write_object(): Writes to an object
// Param:	void *data - destination to be parsed
// Param:	acpi_object_t *source - source object
// Param:	acpi_state_t *state - state of the AML VM
// Return:	size_t - size in bytes for skipping

size_t acpi_write_object(void *data, acpi_object_t *source, acpi_state_t *state, ...)
{
	uint8_t *dest = (uint8_t*)data;

	acpi_object_t *dest_reg;

	// try register
	if(dest[0] >= LOCAL0_OP && dest[0] <= LOCAL7_OP)
	{
		switch(dest[0])
		{
		case LOCAL0_OP:
			dest_reg = &state->local[0];
			break;
		case LOCAL1_OP:
			dest_reg = &state->local[1];
			break;
		case LOCAL2_OP:
			dest_reg = &state->local[2];
			break;
		case LOCAL3_OP:
			dest_reg = &state->local[3];
			break;
		case LOCAL4_OP:
			dest_reg = &state->local[4];
			break;
		case LOCAL5_OP:
			dest_reg = &state->local[5];
			break;
		case LOCAL6_OP:
			dest_reg = &state->local[6];
			break;
		case LOCAL7_OP:
			dest_reg = &state->local[7];
			break;
		}

		acpi_copy_object(dest_reg, source);
		return 1;
	} else if(dest[0] >= ARG0_OP && dest[0] <= ARG6_OP)
	{
		switch(dest[0])
		{
		case ARG0_OP:
			dest_reg = &state->arg[0];
			break;
		case ARG1_OP:
			dest_reg = &state->arg[1];
			break;
		case ARG2_OP:
			dest_reg = &state->arg[2];
			break;
		case ARG3_OP:
			dest_reg = &state->arg[3];
			break;
		case ARG4_OP:
			dest_reg = &state->arg[4];
			break;
		case ARG5_OP:
			dest_reg = &state->arg[5];
			break;
		case ARG6_OP:
			dest_reg = &state->arg[6];
			break;
		}

		acpi_copy_object(dest_reg, source);
		return 1;
	}

	// try integer
	// in which case we use va_list to store the destination in its own object
	uint64_t integer;
	size_t integer_size = acpi_eval_integer(dest, &integer);
	if(integer_size != 0)
	{
		va_list params;
		va_start(params, state);

		acpi_object_t *tmp = va_arg(params, acpi_object_t*);
		acpi_copy_object(tmp, source);

		va_end(params);
		return integer_size;
	}

	// try name spec
	// here, the name may only be an object or a field, it cannot be a MethodInvokation
	if(acpi_is_name(dest[0]))
	{
		char name[512];
		size_t name_size;
		name_size = acpins_resolve_path(name, dest);
		acpi_handle_t *handle = acpi_exec_resolve(name);
		if(!handle)
		{
			acpi_printf("acpi: undefined reference %s\n", name);
			while(1);
		}

		if(handle->type == ACPI_NAMESPACE_NAME)
			acpi_copy_object(&handle->object, source);
		else if(handle->type == ACPI_NAMESPACE_FIELD || handle->type == ACPI_NAMESPACE_INDEXFIELD)
			acpi_write_opregion(handle, source);
		else
		{
			acpi_printf("acpi: NameSpec destination is not name or field.\n");
			while(1);
		}

		return name_size;
	}

	// TO-DO: Implement IndexOf() opcode here, to treat packages like arrays
	kprintf("acpi: undefined opcode, sequence %xb %xb %xb %xb\n", dest[0], dest[1], dest[2], dest[3]);
	while(1);
}

// acpi_exec_store(): Executes a Store() opcode
// Param:	void *data - pointer to opcode
// Param:	acpi_state_t *state - ACPI AML state
// Return:	size_t - size in bytes for skipping

size_t acpi_exec_store(void *data, acpi_state_t *state)
{
	size_t return_size = 1;
	uint8_t *store = (uint8_t*)data;
	store++;		// skip STORE_OP

	size_t dest_size, source_size;
	acpi_object_t source;

	// determine the source project
	source_size = acpi_eval_object(&source, state, &store[0]);
	return_size += source_size;
	store += source_size;

	// now work on the destination
	// destination may be name or variable
	dest_size = acpi_write_object(&store[0], &source, state);

	return_size += dest_size;
	return return_size;
}

// acpi_exec_add(): Executes an Add() opcode
// Param:	void *data - pointer to opcode
// Param:	acpi_state_t *state - ACPI AML state
// Return:	size_t - size in bytes for skipping

size_t acpi_exec_add(void *data, acpi_state_t *state)
{
	size_t return_size = 1;
	uint8_t *opcode = (uint8_t*)data;
	opcode++;

	acpi_object_t n1, n2;
	size_t size;

	size = acpi_eval_object(&n1, state, &opcode[0]);
	opcode += size;
	return_size += size;

	size = acpi_eval_object(&n2, state, &opcode[0]);
	opcode += size;
	return_size += size;

	n1.integer += n2.integer;

	size = acpi_write_object(&opcode[0], &n1, state);
	return_size += size;

	return return_size;
}

// acpi_exec_increment(): Executes Increment() instruction
// Param:	void *data - pointer to opcode
// Param:	acpi_state_t *state - ACPI AML state
// Return:	size_t - size in bytes for skipping

size_t acpi_exec_increment(void *data, acpi_state_t *state)
{
	size_t return_size = 1;
	uint8_t *opcode = (uint8_t*)data;
	opcode++;

	acpi_object_t n;
	size_t size;

	size = acpi_eval_object(&n, state, &opcode[0]);
	n.integer++;
	acpi_write_object(&opcode[0], &n, state);

	return_size += size;
	return return_size;
}

// acpi_exec_decrement(): Executes Decrement() instruction
// Param:	void *data - pointer to opcode
// Param:	acpi_state_t *state - ACPI AML state
// Return:	size_t - size in bytes for skipping

size_t acpi_exec_decrement(void *data, acpi_state_t *state)
{
	size_t return_size = 1;
	uint8_t *opcode = (uint8_t*)data;
	opcode++;

	acpi_object_t n;
	size_t size;

	size = acpi_eval_object(&n, state, &opcode[0]);
	n.integer--;
	acpi_write_object(&opcode[0], &n, state);

	return_size += size;
	return return_size;
}




