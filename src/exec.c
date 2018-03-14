
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

	acpi_printf("acpi: execute control method %s\n", state->name);

	int status = acpi_exec(method->pointer, method->size, state, method_return);

	acpi_printf("acpi: %s finished, ", state->name);

	if(method_return->type == ACPI_INTEGER)
		acpi_printf("return value is integer, %d\n", method_return->integer);
	else if(method_return->type == ACPI_STRING)
		acpi_printf("return value is string, %s\n", method_return->string);
	else if(method_return->type == ACPI_PACKAGE)
		acpi_printf("return value is package\n");

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
	size_t tmp_size;
	uint64_t integer;
	char path[512];
	acpi_handle_t *handle;
	acpi_object_t *return_register;

	acpi_state_t *invoke_state;
	acpi_object_t *invoke_return;
	acpi_handle_t *invoke_method;
	uint8_t invoke_argc;

	while(i < size)
	{
		// Method Invokation?
		if(acpi_is_name(method[i]))
		{
			acpi_printf("method invokation start\n");

			invoke_state = acpi_malloc(sizeof(acpi_state_t));
			invoke_return = acpi_malloc(sizeof(acpi_object_t));

			i += acpins_resolve_path(invoke_state->name, &method[i]);

			// determine how many arguments are needed
			invoke_method = acpi_exec_resolve(invoke_state->name);
			if(!invoke_method)
			{
				acpi_printf("acpi: attempted to invoke undefined method %s\n", invoke_state->name);
				acpi_free(invoke_state);
				acpi_free(invoke_return);
				goto return_zero;
			}

			invoke_argc = invoke_method->method_flags & METHOD_ARGC_MASK;

			uint8_t current_argc = 0;
			if(invoke_argc != 0)
			{
				while(current_argc < invoke_argc)
				{
					acpi_printf("TO-DO: Parse args here\n");
					while(1);
				}
			}

			// execute the method
			acpi_exec_method(invoke_state, invoke_return);

			acpi_free(invoke_state);
			acpi_free(invoke_return);

			acpi_printf("method invokation end\n");
		}

		if(i >= size)
			goto return_zero;

		switch(method[i])
		{
		case ZERO_OP:
		case ONE_OP:
		case ONES_OP:
		case NOP_OP:
			i++;
			break;

		/* Type 2 opcodes are implemented in exec2.c */
		case STORE_OP:
			i += acpi_exec_store(&method[i], state);
			break;

		/* A control method can return a package, integer, string, or a name reference */
		/* We need to take all these into consideration */
		case RETURN_OP:
			i++;
			if(method[i] == PACKAGE_OP)
			{
				acpins_create_package(method_return, &method[i]);
				return 0;
			}

			if(method[i] >= LOCAL0_OP && method[i] <= LOCAL7_OP)
			{
				switch(method[i])
				{
				case LOCAL0_OP:
					return_register = &state->local0;
					break;
				case LOCAL1_OP:
					return_register = &state->local1;
					break;
				case LOCAL2_OP:
					return_register = &state->local2;
					break;
				case LOCAL3_OP:
					return_register = &state->local3;
					break;
				case LOCAL4_OP:
					return_register = &state->local4;
					break;
				case LOCAL5_OP:
					return_register = &state->local5;
					break;
				case LOCAL6_OP:
					return_register = &state->local6;
					break;
				case LOCAL7_OP:
					return_register = &state->local7;
					break;
				}

				acpi_set_object(method_return, return_register);
				return 0;
			}

			if(method[i] >= ARG0_OP && method[i] <= ARG6_OP)
			{
				switch(method[i])
				{
				case ARG0_OP:
					return_register = &state->arg0;
					break;
				case ARG1_OP:
					return_register = &state->arg1;
					break;
				case ARG2_OP:
					return_register = &state->arg2;
					break;
				case ARG3_OP:
					return_register = &state->arg3;
					break;
				case ARG4_OP:
					return_register = &state->arg4;
					break;
				case ARG5_OP:
					return_register = &state->arg5;
					break;
				case ARG6_OP:
					return_register = &state->arg6;
					break;
				}

				acpi_set_object(method_return, return_register);
				return 0;
			}

			tmp_size = acpi_eval_integer(&method[i], &integer);
			if(tmp_size != 0)
			{
				method_return->type = ACPI_INTEGER;
				method_return->integer = integer;
				return 0;
			} else if(method[i] == STRINGPREFIX)
			{
				method_return->type = ACPI_STRING;
				method_return->string = (char*)&method[i+1];
				return 0;
			} else if(acpi_is_name(method[i]) || method[i] == DUAL_PREFIX || method[i] == ROOT_CHAR || method[i] == PARENT_CHAR || method[i] == MULTI_PREFIX)
			{
				// determine name of object
				acpins_resolve_path(path, &method[i]);
				handle = acpins_resolve(path);
				if(!handle)
				{
					acpi_memmove(path + acpi_strlen(path) - 9, path + acpi_strlen(path) - 4, 5);
					handle = acpins_resolve(path);
				}

				if(!handle)
				{
					acpi_printf("acpi: undefined reference in method %s: %s\n", state->name, path);
					while(1);
				}

				while(handle->type == ACPI_NAMESPACE_ALIAS)
				{
					acpi_strcpy(path, handle->alias);
					handle = acpins_resolve(path);
					if(!handle)
					{
						acpi_printf("acpi: undefined reference in method %s: %s\n", state->name, path);
						while(1);
					}
				}

				if(handle->type == ACPI_NAMESPACE_NAME)
				{
					acpi_set_object(method_return, &handle->object);
					return 0;
				}

				acpi_printf("TO-DO: More name returns\n");
				while(1);
			} else
			{
				// NOT INTEGER
				acpi_printf("acpi: undefined opcode in control method %s, sequence %xb %xb %xb %xb\n", state->name, method[i], method[i+1], method[i+2], method[i+3]);
				while(1);
			}

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




