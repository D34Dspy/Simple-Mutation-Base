#ifndef MYPOLYMORPHER__POLYMORPH_H
#define MYPOLYMORPHER__POLYMORPH_H

#include <stdint.h>

typedef struct _mutation_context
{
	struct
	{
		uintptr_t address;
		size_t length;
	}original;

	struct
	{
		void* data;
		size_t count;
	}instruction_cache;

}mutation_context_t;

typedef void (*set_is_writable_t) (uintptr_t address, size_t length, uint32_t* old_flags);
typedef void (*restore_mprot_t) (uintptr_t address, size_t length, uint32_t flags);

//size_t morph_function (uintptr_t original, size_t instructions, uintptr_t map_address);

size_t prepare_mutations (mutation_context_t* context);
uintptr_t post_mutations (mutation_context_t * context, uintptr_t map_address);
void dispose_original (mutation_context_t* context, set_is_writable_t set_is_writable, restore_mprot_t restore_mprot);

mutation_context_t* create_morpher (uint32_t max_instructions);
void dispose_morpher (mutation_context_t* context);

#endif // !MYPOLYMORPHER__POLYMORPH_H