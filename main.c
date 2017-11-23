#include <windows.h>
#include <stdio.h>

#include "polymorph.h"

uintptr_t _my_virtual_alloc(size_t length)
{
	return (uintptr_t)VirtualAlloc (NULL, length, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}
void _my_virtual_free(uintptr_t addr, size_t len)
{
	VirtualFree ((LPVOID)addr, len, 0);
}
void _my_set_is_writable(uintptr_t addr, size_t length, uint32_t* old)
{
	VirtualProtect ((LPVOID)addr, length, GENERIC_ALL, (PDWORD)old);
}
void _my_restore_mprot(uintptr_t addr, size_t length, uint32_t flags)
{
	uint32_t old;
	VirtualProtect ((LPVOID)addr, length, flags, (PDWORD)&old);
}

typedef void (*set_is_writable_t) (uintptr_t address, size_t length, uint32_t* old_flags);
typedef void (*restore_mprot_t) (uintptr_t address, size_t length, uint32_t flags);

typedef uint32_t (*show_message_t)(const char* nigga);
uint32_t _my_show_message (const char* nigga)
{
	MessageBoxA (NULL, nigga, nigga, 0);
	return 0;
}
typedef uint32_t (*print_message_t)(const char* nigga);
uint32_t _my_print_message (const char* nigga)
{
	printf_s (nigga);
	return 0;
}
typedef void (*initialize_t)(struct _function_table* table);
void _my_initialize (struct _function_table* table);

typedef struct _function_table
{
	initialize_t initialize;
	show_message_t show_message;
	print_message_t print_message;
}function_table_t;

void _my_initialize (struct _function_table* table)
{
	table->print_message ("nigga\n");

	// test if statement
	if(0 == 1)
	{
		table->print_message ("something went wrong!\n");
	}

	if(1 != 0)
	{
		table->print_message ("test jcc success!\n");
	}
	if (1 == 1)
	{
		table->print_message ("test jcc success!\n");
	}
	if (1 >= 1)
	{
		table->print_message ("test jcc success!\n");
	}
	if (1 <= 1)
	{
		table->print_message ("test jcc success!\n");
	}
	if (1 > 0)
	{
		table->print_message ("test jcc success!\n");
	}
	if (0 < 1)
	{
		table->print_message ("test jcc success!\n");
	}
}

LARGE_INTEGER _frequency;
void test_table (struct _function_table* table, const char* tbl)
{
	LARGE_INTEGER from, to;

	QueryPerformanceCounter (&from);
	table->initialize (table);
	QueryPerformanceCounter (&to);

	double dur = (double)(to.QuadPart - from.QuadPart) * 1000.0 / (double)_frequency.QuadPart;

	printf_s ("table \"%s\" took %.2fms\n", tbl, dur);
}

int main ()
{
	srand (GetTickCount ());
	QueryPerformanceFrequency (&_frequency);

	mutation_context_t* ctx_initialize = create_morpher (0x1000);
	ctx_initialize->original.address = (uintptr_t)_my_initialize;
	mutation_context_t* ctx_show_message = create_morpher (0x1000);
	ctx_show_message->original.address = (uintptr_t)_my_show_message;
	mutation_context_t* ctx_print_message = create_morpher (0x1000);
	ctx_print_message->original.address = (uintptr_t)_my_print_message;

	size_t size_initialize = prepare_mutations (ctx_initialize);
	size_t size_show_message = prepare_mutations (ctx_show_message);
	size_t size_print_message = prepare_mutations (ctx_print_message);

	size_t needed_size = 
		size_initialize +
		size_show_message +
		size_print_message + 0x30;

	uintptr_t map_address = _my_virtual_alloc (needed_size);
	memset ((void*)map_address, 0x90, needed_size);

	printf_s ("sizeof _my_initialize %d bytes\n", size_initialize);
	printf_s ("sizeof _my_show_message %d bytes\n", size_show_message);
	printf_s ("sizeof _my_print_message %d bytes\n", size_print_message);
	printf_s ("code will be mapped at %p ( %d bytes )\n", (void*)map_address, needed_size);

	function_table_t mutated_table;
	mutated_table.initialize = post_mutations (ctx_initialize, map_address + 0x1);
	mutated_table.show_message = post_mutations (ctx_show_message, map_address + 0x1 + size_initialize);
	mutated_table.print_message = post_mutations (ctx_print_message, map_address + 0x1 + size_initialize + size_show_message);
	printf_s ("_my_initialize got mapped at %p\n", mutated_table.initialize);
	printf_s ("_my_show_message got mapped at %p\n", mutated_table.show_message);
	printf_s ("_my_print_message got mapped at %p\n", mutated_table.print_message);

	function_table_t orig_table;
	orig_table.initialize = _my_initialize;
	orig_table.show_message = _my_show_message;
	orig_table.print_message = _my_print_message;

	test_table (&orig_table, "original");
	test_table (&mutated_table, "mutated");

	dispose_morpher (ctx_initialize);
	dispose_morpher (ctx_show_message);
	dispose_morpher (ctx_print_message);

	_my_virtual_free (map_address, needed_size);

	while (1);

	return 0;
}