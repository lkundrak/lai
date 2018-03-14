
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

// acpi_set_object(): Copies an object
// Param:	acpi_object_t *destination - destination
// Param:	acpi_object_t *source - source
// Return:	Nothing

void acpi_set_object(acpi_object_t *destination, acpi_object_t *source)
{
	acpi_memcpy(destination, source, sizeof(acpi_object_t));
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

	// source object may be integer, string, name or variable
	size_t integer_size, dest_size, source_size;
	uint64_t integer;
	acpi_object_t source;
	char source_name[512];
	acpi_handle_t *object;
	acpi_object_t *destination;

	// try integer
	integer_size = acpi_eval_integer(store, &integer);
	if(integer_size != 0)
	{
		source.type = ACPI_INTEGER;
		source.integer = integer;
		return_size += integer_size;
		store += integer_size;
	} else if(store[0] == STRINGPREFIX)
	{
		// try string
		source.type = ACPI_STRING;
		source.string = (char*)&store[1];
		return_size += acpi_strlen((char*)store) + 1;
		store += acpi_strlen((char*)store) + 1;
	} else if(acpi_is_name(store[0]))
	{
		// try name
		source_size = acpins_resolve_path(source_name, store);
		object = acpi_exec_resolve(source_name);
		if(!object)
		{
			acpi_printf("acpi: undefined reference %s\n", source_name);
			while(1);
		}

		acpi_set_object(&source, &object->object);
		return_size += source_size;
		store += source_size;
	} else if(store[0] >= LOCAL0_OP && store[0] <= LOCAL7_OP)
	{
		switch(store[0])
		{
		case LOCAL0_OP:
			acpi_set_object(&source, &state->local0);
			break;
		case LOCAL1_OP:
			acpi_set_object(&source, &state->local1);
			break;
		case LOCAL2_OP:
			acpi_set_object(&source, &state->local2);
			break;
		case LOCAL3_OP:
			acpi_set_object(&source, &state->local3);
			break;
		case LOCAL4_OP:
			acpi_set_object(&source, &state->local4);
			break;
		case LOCAL5_OP:
			acpi_set_object(&source, &state->local5);
			break;
		case LOCAL6_OP:
			acpi_set_object(&source, &state->local6);
			break;
		case LOCAL7_OP:
			acpi_set_object(&source, &state->local7);
			break;
		}

		return_size++;
		store++;
	} else if(store[0] >= ARG0_OP || store[0] <= ARG6_OP)
	{
		switch(store[0])
		{
		case ARG0_OP:
			acpi_set_object(&source, &state->arg0);
			break;
		case ARG1_OP:
			acpi_set_object(&source, &state->arg1);
			break;
		case ARG2_OP:
			acpi_set_object(&source, &state->arg2);
			break;
		case ARG3_OP:
			acpi_set_object(&source, &state->arg3);
			break;
		case ARG4_OP:
			acpi_set_object(&source, &state->arg4);
			break;
		case ARG5_OP:
			acpi_set_object(&source, &state->arg5);
			break;
		case ARG6_OP:
			acpi_set_object(&source, &state->arg6);
			break;
		}

		return_size++;
		store++;
	} else
	{
		acpi_printf("TO-DO: Non-integer store() values\n");
		while(1);
	}

	// now work on the destination
	// destination may be name or variable
	char destination_name[512];
	if(acpi_is_name(store[0]))
	{
		dest_size = acpins_resolve_path(destination_name, store);
		object = acpi_exec_resolve(destination_name);
		if(!object)
		{
			acpi_printf("acpi: undefined reference %s\n", destination_name);
			while(1);
		}

		if(object->type == ACPI_NAMESPACE_NAME)
			acpi_set_object(&object->object, &source);
		else
		{
			acpi_printf("TO-DO: Non-Name Store() destination\n");
			while(1);
		}
	} else if(store[0] >= LOCAL0_OP && store[0] <= LOCAL7_OP)
	{
		dest_size = 1;
		switch(store[0])
		{
		case LOCAL0_OP:
			destination = &state->local0;
			break;
		case LOCAL1_OP:
			destination = &state->local1;
			break;
		case LOCAL2_OP:
			destination = &state->local2;
			break;
		case LOCAL3_OP:
			destination = &state->local3;
			break;
		case LOCAL4_OP:
			destination = &state->local4;
			break;
		case LOCAL5_OP:
			destination = &state->local5;
			break;
		case LOCAL6_OP:
			destination = &state->local6;
			break;
		case LOCAL7_OP:
			destination = &state->local7;
			break;
		}

		acpi_set_object(destination, &source);
	} else
	{
		acpi_printf("acpi: undefined opcode in control method %s, sequence %xb %xb %xb %xb\n", state->name, store[0], store[1], store[2], store[3]);
		while(1);
	}

	return_size += dest_size;
	return return_size;
}




