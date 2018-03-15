
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018 by Omar Mohammad
 */

#include "lai.h"

/* ACPI Control Method Execution */
/* Type1Opcode := DefBreak | DefBreakPoint | DefContinue | DefFatal | DefIfElse |
   DefLoad | DefNoop | DefNotify | DefRelease | DefReset | DefReturn |
   DefSignal | DefSleep | DefStall | DefUnload | DefWhile */

int acpi_exec(uint8_t *, size_t, acpi_state_t *, acpi_object_t *);

// acpi_exec_method(): Finds and executes a control method
// Param:	acpi_state_t *state - method name and arguments
// Param:	acpi_object_t *method_return - return value of method
// Return:	int - 0 on success

int acpi_exec_method(acpi_state_t *state, acpi_object_t *method_return)
{
	acpi_handle_t *method;
	method = acpins_resolve(state->name);
	if(!method)
		return -1;

	//acpi_printf("acpi: execute control method %s\n", state->name);

	int status = acpi_exec(method->pointer, method->size, state, method_return);

	//acpi_printf("acpi: %s finished, ", state->name);

	/*if(method_return->type == ACPI_INTEGER)
		acpi_printf("return value is integer: %d\n", method_return->integer);
	else if(method_return->type == ACPI_STRING)
		acpi_printf("return value is string: '%s'\n", method_return->string);
	else if(method_return->type == ACPI_PACKAGE)
		acpi_printf("return value is package\n");
	else if(method_return->type == ACPI_BUFFER)
		acpi_printf("return value is buffer\n");*/

	return status;
}

// acpi_exec(): Internal function, executes actual AML opcodes
// Param:	uint8_t *method - pointer to method opcodes
// Param:	size_t size - size of method of bytes
// Param:	acpi_state_t *state - machine state
// Param:	acpi_object_t *method_return - return value of method
// Return:	int - 0 on success

int acpi_exec(uint8_t *method, size_t size, acpi_state_t *state, acpi_object_t *method_return)
{
	if(!size)
	{
		method_return->type = ACPI_INTEGER;
		method_return->integer = 0;
		return 0;
	}

	acpi_strcpy(acpins_path, state->name);

	size_t i = 0;
	acpi_object_t invoke_return;
	state->status = ACPI_STATUS_NORMAL;

	while(i <= size)
	{
		if(state->status != ACPI_STATUS_WHILE && i >= size)
			break;

		if(state->status == ACPI_STATUS_WHILE && i >= state->condition_end)
			i = state->condition_start;

		// Method Invokation?
		if(acpi_is_name(method[i]))
			i += acpi_methodinvoke(&method[i], state, &invoke_return);

		if(i >= size)
			goto return_zero;

		if(acpi_is_name(method[i]))
			continue;

		switch(method[i])
		{
		case ZERO_OP:
		case ONE_OP:
		case ONES_OP:
		case NOP_OP:
			i++;
			break;

		/* A control method can return literally any object */
		/* So we need to take this into consideration */
		case RETURN_OP:
			i++;
			acpi_eval_object(method_return, state, &method[i]);
			return 0;

		case WHILE_OP:
			state->condition_start = i;
			i++;
			state->condition_end = i;
			state->status = ACPI_STATUS_WHILE;
			i += acpi_parse_pkgsize(&method[i], &state->condition_pkgsize);
			state->condition_end += state->condition_pkgsize;

			// evaluate the predicate
			state->predicate_size = acpi_eval_object(&state->predicate, state, &method[i]);
			if(state->predicate.integer == 0)
			{
				state->status = ACPI_STATUS_NORMAL;
				i = state->condition_end;
			} else
			{
				i += state->predicate_size;
			}

			break;

		/* Most of the type 2 opcodes are implemented in exec2.c */
		case NAME_OP:
			i += acpi_exec_name(&method[i], state);
			break;
		case STORE_OP:
			i += acpi_exec_store(&method[i], state);
			break;
		case ADD_OP:
			i += acpi_exec_add(&method[i], state);
			break;
		case INCREMENT_OP:
			i += acpi_exec_increment(&method[i], state);
			break;
		case DECREMENT_OP:
			i += acpi_exec_decrement(&method[i], state);
			break;

		default:
			acpi_printf("acpi: undefined opcode in control method %s, sequence %xb %xb %xb %xb\n", state->name, method[i], method[i+1], method[i+2], method[i+3]);
			while(1);
		}
	}

return_zero:
	// when it returns nothing, assume Return (0)
	method_return->type = ACPI_INTEGER;
	method_return->integer = 0;
	return 0;
}

// acpi_methodinvoke(): Executes a MethodInvokation
// Param:	void *data - pointer to MethodInvokation
// Param:	acpi_state_t *old_state - state of currently executing method
// Param:	acpi_object_t *method_return - object to store return value
// Return:	size_t - size in bytes for skipping

size_t acpi_methodinvoke(void *data, acpi_state_t *old_state, acpi_object_t *method_return)
{
	uint8_t *methodinvokation = (uint8_t*)data;

	// save the state of the currently executing method
	char path_save[512];
	acpi_strcpy(path_save, acpins_path);

	size_t return_size = 0;

	// determine the name of the method
	acpi_state_t *state = acpi_malloc(sizeof(acpi_state_t));
	size_t name_size = acpins_resolve_path(state->name, methodinvokation);
	return_size += name_size;
	methodinvokation += name_size;

	acpi_handle_t *method;
	method = acpi_exec_resolve(state->name);
	if(!method)
	{
		acpi_printf("acpi: undefined MethodInvokation %s\n", state->name);
		while(1);
	}

	uint8_t argc = method->method_flags & METHOD_ARGC_MASK;
	uint8_t current_argc = 0;
	size_t arg_size;
	if(argc != 0)
	{
		// parse method arguments here
		while(current_argc < argc)
		{
			arg_size = acpi_eval_object(&state->arg[current_argc], old_state, methodinvokation);
			methodinvokation += arg_size;
			return_size += arg_size;

			current_argc++;
		}
	}

	// execute
	acpi_exec_method(state, method_return);

	// restore state
	acpi_strcpy(acpins_path, path_save);
	return return_size;
}





