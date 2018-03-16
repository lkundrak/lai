
/*
 * Lux ACPI Implementation
 * Copyright (C) 2018 by Omar Mohammad
 */

/* ACPI OperationRegion Implementation */
/* OperationRegions allow ACPI's AML to access I/O ports, system memory, system
 * CMOS, PCI config, and other hardware used for I/O with the chipset. */

/* TO-DO: I should implement the embedded controller, because some real HW use it */

#include "lai.h"

void acpi_read_field(acpi_object_t *, acpi_handle_t *);
void acpi_write_field(acpi_handle_t *, acpi_object_t *);
void acpi_read_indexfield(acpi_object_t *, acpi_handle_t *);
void acpi_write_indexfield(acpi_handle_t *, acpi_object_t *);

// acpi_read_opregion(): Reads from an OpRegion Field or IndexField
// Param:	acpi_object_t *destination - where to read data
// Param:	acpi_handle_t *field - field or index field
// Return:	Nothing

void acpi_read_opregion(acpi_object_t *destination, acpi_handle_t *field)
{
	if(field->type == ACPI_NAMESPACE_FIELD)
		return acpi_read_field(destination, field);

	else if(field->type == ACPI_NAMESPACE_INDEXFIELD)
		return acpi_read_indexfield(destination, field);
}

// acpi_write_opregion(): Writes to a OpRegion Field or IndexField
// Param:	acpi_handle_t *field - field or index field
// Param:	acpi_object_t *source - data to write
// Return:	Nothing

void acpi_write_opregion(acpi_handle_t *field, acpi_object_t *source)
{
	if(field->type == ACPI_NAMESPACE_FIELD)
		return acpi_write_field(field, source);

	else if(field->type == ACPI_NAMESPACE_INDEXFIELD)
		return acpi_write_indexfield(field, source);
}

// acpi_read_field(): Reads from a normal field
// Param:	acpi_object_t *destination - where to read data
// Param:	acpi_handle_t *field - field
// Return:	Nothing

void acpi_read_field(acpi_object_t *destination, acpi_handle_t *field)
{
	acpi_handle_t *opregion;
	opregion = acpins_resolve(field->field_opregion);
	if(!opregion)
	{
		acpi_printf("acpi: OpRegion %s doesn't exist.\n", field->field_opregion);
		while(1);
	}

	uint64_t offset, value, mask;
	size_t bit_offset;

	mask = ((uint64_t)1 << field->field_size);
	mask--;
	offset = field->field_offset / 8;
	void *mmio;

	// these are for PCI
	char name[512];
	acpi_object_t bus_number, address_number;
	int eval_status;
	size_t pci_byte_offset;

	if(opregion->op_address_space != OPREGION_PCI)
	{
		switch(field->field_flags & 0x0F)
		{
		case FIELD_BYTE_ACCESS:
			bit_offset = field->field_offset % 8;
			break;

		case FIELD_WORD_ACCESS:
			bit_offset = field->field_offset % 16;
			break;

		case FIELD_DWORD_ACCESS:
		case FIELD_ANY_ACCESS:
			bit_offset = field->field_offset % 32;
			break;

		case FIELD_QWORD_ACCESS:
			bit_offset = field->field_offset % 64;
			break;

		default:
			acpi_printf("acpi: undefined field flags 0x%xb: %s\n", field->field_flags, field->path);
			while(1);
		}
	} else
	{
		bit_offset = field->field_offset % 32;
		pci_byte_offset = field->field_offset % 4;
	}

	// now read from either I/O ports, MMIO, or PCI config
	if(opregion->op_address_space == OPREGION_IO)
	{
		// I/O port
		switch(field->field_flags & 0x0F)
		{
		case FIELD_BYTE_ACCESS:
			value = (uint64_t)acpi_inb(opregion->op_base + offset) >> bit_offset;
			//acpi_printf("acpi: read 0x%xb from I/O port 0x%xw\n", (uint8_t)value, opregion->op_base + offset);
			break;
		case FIELD_WORD_ACCESS:
			value = (uint64_t)acpi_inw(opregion->op_base + offset) >> bit_offset;
			//acpi_printf("acpi: read 0x%xw from I/O port 0x%xw\n", (uint16_t)value, opregion->op_base + offset);
			break;
		case FIELD_DWORD_ACCESS:
		case FIELD_ANY_ACCESS:
			value = (uint64_t)acpi_ind(opregion->op_base + offset) >> bit_offset;
			//acpi_printf("acpi: read 0x%xd from I/O port 0x%xw\n", (uint32_t)value, opregion->op_base + offset);
			break;
		default:
			acpi_printf("acpi: undefined field flags 0x%xb: %s\n", field->field_flags, field->path);
			while(1);
		}
	} else if(opregion->op_address_space == OPREGION_MEMORY)
	{
		// Memory-mapped I/O
		mmio = acpi_map(opregion->op_base + offset, 8);
		uint8_t *mmio_byte;
		uint16_t *mmio_word;
		uint32_t *mmio_dword;
		uint64_t *mmio_qword;

		switch(field->field_flags & 0x0F)
		{
		case FIELD_BYTE_ACCESS:
			mmio_byte = (uint8_t*)mmio;
			value = (uint64_t)mmio_byte[0] >> bit_offset;
			break;
		case FIELD_WORD_ACCESS:
			mmio_word = (uint16_t*)mmio;
			value = (uint64_t)mmio_word[0] >> bit_offset;
			break;
		case FIELD_DWORD_ACCESS:
		case FIELD_ANY_ACCESS:
			mmio_dword = (uint32_t*)mmio;
			value = (uint64_t)mmio_dword[0] >> bit_offset;
			break;
		case FIELD_QWORD_ACCESS:
			mmio_qword = (uint64_t*)mmio;
			value = mmio_qword[0] >> bit_offset;
			break;
		default:
			acpi_printf("acpi: undefined field flags 0x%xb: %s\n", field->field_flags, field->path);
			while(1);
		}
	} else if(opregion->op_address_space == OPREGION_PCI)
	{
		// PCI bus number is in the _BBN object
		acpi_strcpy(name, opregion->path);
		acpi_strcpy(name + acpi_strlen(name) - 4, "_BBN");
		eval_status = acpi_eval(&bus_number, name);

		// when the _BBN object is not present, we assume PCI bus 0
		if(eval_status != 0)
		{
			bus_number.type = ACPI_INTEGER;
			bus_number.integer = 0;
		}

		// device slot/function is in the _ADR object
		acpi_strcpy(name, opregion->path);
		acpi_strcpy(name + acpi_strlen(name) - 4, "_ADR");
		eval_status = acpi_eval(&address_number, name);

		// when this is not present, again default to zero
		if(eval_status != 0)
		{
			address_number.type = ACPI_INTEGER;
			address_number.type = 0;
		}

		value = acpi_pci_read((uint8_t)bus_number.integer, (uint8_t)(address_number.integer >> 16) & 0xFF, (uint8_t)(address_number.integer & 0xFF), (offset & 0xFFFC) + opregion->op_base);

		//acpi_printf("acpi: read 0x%xd from PCI config 0x%xw, %xb:%xb:%xb\n", value, (uint16_t)(offset & 0xFFFC) + opregion->op_base, (uint8_t)bus_number.integer, (uint8_t)(address_number.integer >> 16) & 0xFF, (uint8_t)address_number.integer & 0xFF);
		value >>= bit_offset;
	} else
	{
		acpi_printf("acpi: undefined opregion address space: %d\n", opregion->op_address_space);
		while(1);
	}

	destination->type = ACPI_INTEGER;
	destination->integer = value & mask;
}

// acpi_write_field(): Writes to a normal field
// Param:	acpi_handle_t *field - field
// Param:	acpi_object_t *source - data to write
// Return:	Nothing

void acpi_write_field(acpi_handle_t *field, acpi_object_t *source)
{
	// determine the flags we need in order to write
	acpi_handle_t *opregion;
	opregion = acpins_resolve(field->field_opregion);
	if(!opregion)
	{
		acpi_printf("acpi: OpRegion %s doesn't exist.\n", field->field_opregion);
		while(1);
	}

	uint64_t offset, value, mask;
	size_t bit_offset;

	mask = ((uint64_t)1 << field->field_size);
	mask--;
	offset = field->field_offset / 8;
	void *mmio;

	switch(field->field_flags & 0x0F)
	{
	case FIELD_BYTE_ACCESS:
		bit_offset = field->field_offset % 8;
		break;

	case FIELD_WORD_ACCESS:
		bit_offset = field->field_offset % 16;
		break;

	case FIELD_DWORD_ACCESS:
	case FIELD_ANY_ACCESS:
		bit_offset = field->field_offset % 32;
		break;

	case FIELD_QWORD_ACCESS:
		bit_offset = field->field_offset % 64;
		break;

	default:
		acpi_printf("acpi: undefined field flags 0x%xb: %s\n", field->field_flags, field->path);
		while(1);
	}

	// read from the field
	if(opregion->op_address_space == OPREGION_IO)
	{
		// I/O port
		switch(field->field_flags & 0x0F)
		{
		case FIELD_BYTE_ACCESS:
			value = (uint64_t)acpi_inb(opregion->op_base + offset);
			break;
		case FIELD_WORD_ACCESS:
			value = (uint64_t)acpi_inw(opregion->op_base + offset);
			break;
		case FIELD_DWORD_ACCESS:
		case FIELD_ANY_ACCESS:
			value = (uint64_t)acpi_ind(opregion->op_base + offset);
			break;
		default:
			acpi_printf("acpi: undefined field flags 0x%xb: %s\n", field->field_flags, field->path);
			while(1);
		}
	} else if(opregion->op_address_space == OPREGION_MEMORY)
	{
		// Memory-mapped I/O
		mmio = acpi_map(opregion->op_base + offset, 8);
		uint8_t *mmio_byte;
		uint16_t *mmio_word;
		uint32_t *mmio_dword;
		uint64_t *mmio_qword;

		switch(field->field_flags & 0x0F)
		{
		case FIELD_BYTE_ACCESS:
			mmio_byte = (uint8_t*)mmio;
			value = (uint64_t)mmio_byte[0];
			break;
		case FIELD_WORD_ACCESS:
			mmio_word = (uint16_t*)mmio;
			value = (uint64_t)mmio_word[0];
			break;
		case FIELD_DWORD_ACCESS:
		case FIELD_ANY_ACCESS:
			mmio_dword = (uint32_t*)mmio;
			value = (uint64_t)mmio_dword[0];
			break;
		case FIELD_QWORD_ACCESS:
			mmio_qword = (uint64_t*)mmio;
			value = mmio_qword[0];
			break;
		default:
			acpi_printf("acpi: undefined field flags 0x%xb: %s\n", field->field_flags, field->path);
			while(1);
		}
	} else
	{
		acpi_printf("acpi: undefined opregion address space: %d\n", opregion->op_address_space);
		while(1);
	}

	// now determine how we need to write to the field
	if(((field->field_flags >> 5) & 0x0F) == FIELD_PRESERVE)
	{
		value &= ~(mask << bit_offset);
		value |= (source->integer << bit_offset);
	} else if(((field->field_flags >> 5) & 0x0F) == FIELD_WRITE_ONES)
	{
		value = 0xFFFFFFFFFFFFFFFF;
		value &= ~(mask << bit_offset);
		value |= (source->integer << bit_offset);
	} else
	{
		value = 0;
		value |= (source->integer << bit_offset);
	}

	// finally, write to the field
	if(opregion->op_address_space == OPREGION_IO)
	{
		// I/O port
		switch(field->field_flags & 0x0F)
		{
		case FIELD_BYTE_ACCESS:
			acpi_outb(opregion->op_base + offset, (uint8_t)value);
			//acpi_printf("acpi: wrote 0x%xb to I/O port 0x%xw\n", (uint8_t)value, opregion->op_base + offset);
			break;
		case FIELD_WORD_ACCESS:
			acpi_outw(opregion->op_base + offset, (uint16_t)value);
			//acpi_printf("acpi: wrote 0x%xw to I/O port 0x%xw\n", (uint16_t)value, opregion->op_base + offset);
			break;
		case FIELD_DWORD_ACCESS:
		case FIELD_ANY_ACCESS:
			acpi_outd(opregion->op_base + offset, (uint32_t)value);
			//acpi_printf("acpi: wrote 0x%xd to I/O port 0x%xw\n", (uint32_t)value, opregion->op_base + offset);
			break;
		default:
			acpi_printf("acpi: undefined field flags 0x%xb: %s\n", field->field_flags, field->path);
			while(1);
		}

		// iowait() equivalent
		acpi_outb(0x80, 0x00);
		acpi_outb(0x80, 0x00);
	} else if(opregion->op_address_space == OPREGION_MEMORY)
	{
		// Memory-mapped I/O
		mmio = acpi_map(opregion->op_base + offset, 8);
		uint8_t *mmio_byte;
		uint16_t *mmio_word;
		uint32_t *mmio_dword;
		uint64_t *mmio_qword;

		switch(field->field_flags & 0x0F)
		{
		case FIELD_BYTE_ACCESS:
			mmio_byte = (uint8_t*)mmio;
			mmio_byte[0] = (uint8_t)value;
			break;
		case FIELD_WORD_ACCESS:
			mmio_word = (uint16_t*)mmio;
			mmio_word[0] = (uint16_t)value;
			break;
		case FIELD_DWORD_ACCESS:
		case FIELD_ANY_ACCESS:
			mmio_dword = (uint32_t*)mmio;
			mmio_dword[0] = (uint32_t)value;
			break;
		case FIELD_QWORD_ACCESS:
			mmio_qword = (uint64_t*)mmio;
			mmio_qword[0] = value;
			break;
		default:
			acpi_printf("acpi: undefined field flags 0x%xb\n", field->field_flags);
			while(1);
		}
	} else
	{
		acpi_printf("acpi: undefined opregion address space: %d\n", opregion->op_address_space);
		while(1);
	}

	// Finished...

}

// acpi_read_indexfield(): Reads from an IndexField
// Param:	acpi_object_t *destination - destination to read into
// Param:	acpi_handle_t *indexfield - index field
// Return:	Nothing

void acpi_read_indexfield(acpi_object_t *destination, acpi_handle_t *indexfield)
{
	acpi_handle_t *field;
	field = acpins_resolve(indexfield->indexfield_index);
	if(!field)
	{
		acpi_printf("acpi: undefined reference %s\n", indexfield->indexfield_index);
		while(1);
	}

	acpi_object_t index;
	index.type = ACPI_INTEGER;
	index.integer = indexfield->indexfield_offset / 8;	// always byte-aligned

	acpi_write_field(field, &index);	// the index register

	field = acpins_resolve(indexfield->indexfield_data);
	if(!field)
	{
		acpi_printf("acpi: undefined reference %s\n", indexfield->indexfield_data);
		while(1);
	}

	acpi_read_field(destination, field);	// the data register
}

// acpi_write_indexfield(): Writes to an IndexField
// Param:	acpi_handle_t *indexfield - index field
// Param:	acpi_object_t *source - data to write
// Return:	Nothing

void acpi_write_indexfield(acpi_handle_t *indexfield, acpi_object_t *source)
{
	acpi_handle_t *field;
	field = acpins_resolve(indexfield->indexfield_index);
	if(!field)
	{
		acpi_printf("acpi: undefined reference %s\n", indexfield->indexfield_index);
		while(1);
	}

	acpi_object_t index;
	index.type = ACPI_INTEGER;
	index.integer = indexfield->indexfield_offset / 8;	// always byte-aligned

	acpi_write_field(field, &index);	// the index register

	field = acpins_resolve(indexfield->indexfield_data);
	if(!field)
	{
		acpi_printf("acpi: undefined reference %s\n", indexfield->indexfield_data);
		while(1);
	}

	acpi_write_field(field, source);	// the data register
}





