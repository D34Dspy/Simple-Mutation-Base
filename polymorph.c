#include "polymorph.h"

#include <corecrt_malloc.h>
#include <memory.h>
#include <stdlib.h>
#include "ld32.h"

#define _my_rnd_min 0x50
#define _my_rnd_max 0x100
#define _my_junk_min 0
#define _my_junk_max 15

typedef struct _instruction
{
	uint8_t length;
	uint8_t* data;
	uint8_t* orig_addr;
	uint8_t* new_addr;
}instruction_t;

uint32_t _my_random (uint32_t begin, uint32_t end)
{
	uint32_t range = (end - begin) + 1;
	uint32_t limit = (RAND_MAX + 1) - ((RAND_MAX + 1) % range);

	uint32_t randVal = rand ();
	while (randVal >= limit) randVal = rand ();

	return (randVal % range) + begin;
}

uint8_t* _my_malloc (size_t size)
{
	uint8_t* ret = malloc (size + 10);
	memset (ret, 0, size);
	//printf_s ("_my_malloc\t%p (%d bytes)\n", ret, size);
	return ret;
}

void _my_free(uint8_t* addr)
{
	//printf_s ("_my_free\t%p\n", addr);
	free (addr);
}

// ma (dis)assembler
typedef enum _registerbase
{
	rb_eax,
	rb_ecx,
	rb_edx,
	rb_ebx,
	rb_esp,
	rb_ebp,
	rb_esi,
	rb_edi,
}regbase_t;
typedef enum _modtype
{
	mt_to_mem,
	mt_to_reg
}modtype_t;
typedef enum _valsize
{
	vs_byte,
	vs_dword
}valsize_t;
typedef struct _opcode
{
	union
	{
		struct
		{
			/*
			struct
			{
				// Optional Scaled Indexed Byte
				// if the instruction uses a scaled
				// indexed memory addressing mode
				uint8_t sib : 1;

				// This byte is only required 
				// if the instruction supports
				// register or memory operands
				// 1 = add r/m to reg | 0 = add reg to r/m
				uint8_t rm : 1;

				// One or two byte instruction 
				// opcode (two bytes if the special
				// 0x0F opcode expansion is present
				uint8_t opc : 2;

				// Prefix bytes, zero to four special
				// prefix values that affect the
				// operation of the instruction
				//uint8_t pb_sp : 1;// Segment override Prefix
				//uint8_t pb_op : 1;// Operand-size Prefix
				//uint8_t pb_ap : 1;// Address-size Prefix
				//uint8_t pb_ip : 1;// Instruction-size Prefix
				uint8_t pb;
			}primary;
			struct
			{
				// "Right Operand Indicator"
				uint8_t rm_rop : 3;

				// "Left Operand Indicator"
				uint8_t lop : 3;

				// Immediate (constant) data.
				// This is a zero, one, two, or
				// four byte constant value if the
				// instruction has an immediate operand
				uint8_t imm : 1;

				// Displacement. This is a zero 
				// or one, two or four byte value
				// that specifies a memory address
				// displacement for the instruction
				uint8_t disp : 1;
			}secondary;
			*/

			// sub al 00101100
			// add al 00000100 

			struct
			{
				uint8_t s : 1;
				uint8_t d : 1;
				uint8_t sib : 2;
				//uint8_t prefix : 4;
				uint8_t p4 : 1;
				uint8_t p3 : 1;
				uint8_t p2 : 1;
				uint8_t p1 : 1;
			}primary;

			struct
			{
				uint8_t rm : 3;
				uint8_t reg : 3;
				uint8_t mod : 2;
			}secondary;
		};

		struct
		{
			uint8_t encoded_primary;
			uint8_t encoded_secondary;
		};
	};
}opcode_t;
typedef enum _jccbase
{
	jb_jo,
	jb_jno,
	jb_jb,
	jb_jnb,
	jb_jz,
	jb_jnz,
	jb_jbe,
	jb_jna,
	jb_ja,
	jb_js,
	jb_jns,
	jb_jp,
	jb_jnp,
	jb_jl,
	jb_jle,
	jb_jg
}jccbase_t;

void add_instruction (mutation_context_t* context, const instruction_t* instruction)
{
	memcpy ((instruction_t*)context->instruction_cache.data + context->instruction_cache.count, instruction, sizeof (instruction_t));
	context->instruction_cache.count++;
}

void get_registers(uint8_t pack, regbase_t* left, regbase_t* right)
{
	opcode_t opc;
	memset (&opc, 0, sizeof (opcode_t));
	opc.encoded_secondary = pack;
	if (left)
		*left = opc.secondary.reg;
	if (right)
		*right = opc.secondary.rm;
	return opc.encoded_secondary;
}
uint8_t encode_registers(regbase_t left, regbase_t right)
{
	opcode_t opc;
	memset (&opc, 0, sizeof (opcode_t));
	opc.secondary.mod = 0xFF;
	opc.secondary.reg = left;
	opc.secondary.rm = right;
	return opc.encoded_secondary;
}
uint8_t encode_regdisp (regbase_t reg)
{
	opcode_t op;
	op.secondary.mod = 0xFF;
	op.secondary.reg = reg;
	op.secondary.rm = 5; // 1 0 1
	return op.encoded_secondary;
}

uint32_t jmp_get_offset (uint32_t from, uint32_t to)
{
	return to - from - 5;
}
uint32_t jmp_get_dst (uint32_t origin, uint32_t offset)
{
	return origin + offset + 5;
}

// assembler functions
void pushr (mutation_context_t* context, regbase_t regbase)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (1);
	instr.length = 1;
	*instr.data = 0x50 + regbase;
	add_instruction (context, &instr);
}
void popr (mutation_context_t* context, regbase_t regbase)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (1);
	*instr.data = 0x58 + regbase;
	instr.length = 1;
	add_instruction (context, &instr);
}
void push32 (mutation_context_t* context, uint32_t value)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (5);
	instr.length = 5;
	*instr.data = 0x68;
	*(uint32_t*)(instr.data + 1) = value;
	add_instruction (context, &instr);
}
void jmp8 (mutation_context_t* context, uint8_t offset)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (2);
	instr.length = 2;
	*instr.data = 0xEB;
	*(instr.data + 1) = offset;
	add_instruction (context, &instr);
}
void jmp32 (mutation_context_t* context, uint8_t* orig, uint32_t offset)
{
	instruction_t instr;
	instr.orig_addr = orig;
	instr.data = _my_malloc (5);
	instr.length = 5;
	*instr.data = 0xE9;
	*(uint32_t*)(instr.data + 1) = offset;
	add_instruction (context, &instr);
}
void call32 (mutation_context_t* context, uint8_t* orig, uint32_t offset)
{
	instruction_t instr;
	instr.orig_addr = orig;
	instr.data = _my_malloc (5);
	instr.length = 5;
	*instr.data = 0xE8;
	*(uint32_t*)(instr.data + 1) = offset;
	add_instruction (context, &instr);
}
void insert_random_data (mutation_context_t* context, uint8_t length)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (length);
	instr.length = length;
	for (uint32_t i = 0; i < length; i++)
		*(instr.data + i) = _my_random (0, 255);
	add_instruction (context, &instr);
}
void jcc8 (mutation_context_t* context, uint8_t* orig, jccbase_t jccbase, uint8_t offset)
{
	instruction_t instr;
	instr.orig_addr = orig;
	instr.data = _my_malloc (2);
	instr.length = 2;
	*instr.data = 0x70 + jccbase;
	*(instr.data + 1) = offset;
	add_instruction (context, &instr);
}
void jcc32 (mutation_context_t* context, uint8_t* orig, jccbase_t jccbase, uint32_t offset)
{
	instruction_t instr;
	instr.orig_addr = orig;
	instr.data = _my_malloc (2);
	instr.length = 2;
	*(instr.data) = 0x0F;
	*(instr.data + 1) = 0x70 + 0x10 + jccbase;
	*(uint32_t *)(instr.data + 2) = offset;
	add_instruction (context, &instr);
}
void xorr (mutation_context_t* context, regbase_t left, regbase_t right)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (2);
	*instr.data = 0x33;
	*(instr.data + 1) = encode_registers (left, right);
	instr.length = 2;
	add_instruction (context, &instr);
}
void andrd (mutation_context_t* context, regbase_t reg, uint32_t value)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (5);
	*instr.data = 0x25;
	*(instr.data + 1) = encode_regdisp (reg);
	*(uint32_t*)(instr.data + 2) = value;
	instr.length = 5;
	add_instruction (context, &instr);
}
void copy_instr (mutation_context_t* context, uint32_t length, uint8_t* data)
{
	instruction_t instr;
	instr.orig_addr = data;
	instr.data = _my_malloc (length);
	memcpy (instr.data, data, length);
	instr.length = length;
	add_instruction (context, &instr);
}
void pushfd (mutation_context_t* context)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (1);
	instr.length = 1;
	*instr.data = 0x9C;
	add_instruction (context, &instr);
}
void popfd (mutation_context_t* context)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (1);
	instr.length = 1;
	*instr.data = 0x9D;
	add_instruction (context, &instr);
}
void incr (mutation_context_t* context, regbase_t reg)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (1);
	*instr.data = 0x40 + reg;
	instr.length = 1;
	add_instruction (context, &instr);
}
void decr (mutation_context_t* context, regbase_t reg)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (1);
	*instr.data = 0x48 + reg;
	instr.length = 1;
	add_instruction (context, &instr);
}
void testrr (mutation_context_t* context, valsize_t valsize, regbase_t dst, regbase_t src)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (2);
	opcode_t* opc = (opcode_t*)instr.data;
	opc->encoded_primary = 0x85;
	opc->primary.s = (uint8_t)valsize;
	opc->primary.d = 0;
	opc->secondary.mod = 0xFF;
	opc->secondary.rm = dst;
	opc->secondary.reg = src;

	instr.length = 2;
	add_instruction (context, &instr);
}
void addrr (mutation_context_t* context, valsize_t valsize, regbase_t dst, regbase_t src)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (2);

	opcode_t* opc = (opcode_t*)instr.data;
	opc->encoded_primary = 0x00;
	opc->primary.s = (uint8_t)valsize;
	opc->primary.d = 0;
	opc->secondary.mod = 0xFF;
	opc->secondary.rm = dst;
	opc->secondary.reg = src;

	instr.length = 2;
	add_instruction (context, &instr);
}
void addrd (mutation_context_t* context, valsize_t valsize, regbase_t reg, uint32_t disp)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.length = valsize == vs_byte ? 3 : 6;
	instr.data = _my_malloc (instr.length);

	opcode_t* opc = (opcode_t*)instr.data;
	opc->encoded_primary = 0;
	opc->primary.s = (uint8_t)valsize;
	opc->primary.d = 0;
	opc->primary.p1 = 1;

	opc->secondary.mod = 0xFF;
	opc->secondary.reg = 0;
	opc->secondary.rm = reg;

	if (valsize == vs_byte)
		*(uint8_t*)(instr.data + 2) = (uint8_t)disp;
	else *(uint32_t*)(instr.data + 2) = (uint32_t)disp;

	add_instruction (context, &instr);
}
void subrr (mutation_context_t* context, valsize_t valsize, regbase_t dst, regbase_t src)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.data = _my_malloc (2);

	opcode_t* opc = (opcode_t*)instr.data;
	opc->encoded_primary = 0x29;
	opc->primary.s = (uint8_t)valsize;
	opc->primary.d = 0;
	opc->secondary.mod = 0xFF;
	opc->secondary.rm = dst;
	opc->secondary.reg = src;

	instr.length = 2;
	add_instruction (context, &instr);
}
void subrd (mutation_context_t* context, valsize_t valsize, regbase_t reg, uint32_t disp)
{
	instruction_t instr;
	instr.orig_addr = 0;
	instr.length = valsize == vs_byte ? 3 : 6;
	instr.data = _my_malloc (instr.length);

	opcode_t* opc = (opcode_t*)instr.data;
	opc->encoded_primary = 0;
	opc->primary.s = (uint8_t)valsize;
	opc->primary.d = 0;
	opc->primary.p1 = 1;

	opc->secondary.mod = 0xFF;
	opc->secondary.reg = 5;
	opc->secondary.rm = reg;

	// sub EA 11101010
	// add C2 11000010

	if (valsize == vs_byte)
		*(uint8_t*)(instr.data + 2) = (uint8_t)disp;
	else *(uint32_t*)(instr.data + 2) = (uint32_t)disp;

	add_instruction (context, &instr);
}

uint8_t* get_function_end(uint8_t* start_address, size_t* instruction_count)
{
	*instruction_count = 0;
	uint8_t* highest_code_point = start_address;
	while (1)
	{
		size_t length = get_instruction_length (start_address);

		if (highest_code_point > start_address)
			highest_code_point = start_address;  

		uint8_t* new_code_point = 0;
		if (*start_address >= 0x70 && *start_address <= 0x7F) // convert it to 32 bit jump
		{
			int8_t offset = *(start_address + 1);
			new_code_point = get_function_end (start_address + offset, instruction_count);
		}
		else if (*start_address == 0xEB)
		{
			int8_t offset = *(start_address + 1);
			new_code_point = get_function_end (start_address + offset, instruction_count);
		}
		else if (*start_address == 0xE9)
		{
			uint32_t offset = *(start_address + 2);
			new_code_point = get_function_end ((uint8_t*)jmp_get_dst ((uint32_t)start_address + 1, offset), instruction_count);
		}
		else if (*start_address == 0x0F && *(start_address + 1) >= 0x80 && *(start_address + 1) <= 0x8F)
		{
			uint32_t offset = *(start_address + 2);
			new_code_point = get_function_end ((uint8_t*)jmp_get_dst ((uint32_t)start_address + 1, offset), instruction_count);
		}

		if (new_code_point > highest_code_point)
			highest_code_point = new_code_point;

		*(instruction_count) = *(instruction_count)+1;

		if(*start_address == 0xC2 || 
			*start_address == 0xC3 || 
			*start_address == 0xCA || 
			*start_address == 0xCB)
		{
			if (start_address + length > highest_code_point)
				highest_code_point = start_address + length;
			break;
		}

		start_address += length;
	}
	return highest_code_point;
}

// mutation and that stuff..
regbase_t random_reg (void)
{
	regbase_t ret = _my_random (0, 7);
	while(ret == rb_ebp || ret == rb_esp)
		ret = _my_random (0, 7);
	return ret;
}
void random_set_reg (mutation_context_t* context, regbase_t reg, uint32_t value)
{
	uint8_t type = _my_random (0, 3);

	push32 (context, value);
	popr (context, reg);
	return;
	if(type == 0)
	{
		// wooo spammin like shit :)
		regbase_t rl;
		if (reg == rb_eax)
			rl = rb_ecx;
		else rl = reg - 1;
		pushr (context, rl);

		uint32_t xw = _my_random (_my_rnd_min, _my_rnd_max);
		push32 (context, value ^ xw);
		popr (context, reg);

		pushr (context, xw);
		popr (context, rl);

		xorr (context, rl, reg);
		pushr (context, rl);
		popr (context, reg);

		popr (context, rl);
	}
	else if(type == 1)
	{
		push32 (context, value);
		popr (context, reg);
	}
	else
	{
		// wooo spammin like shit :)
		regbase_t rl;
		if (reg == rb_eax)
			rl = rb_ecx;
		else rl = reg - 1;
		pushr (context, rl);

		uint32_t xw = _my_random (_my_rnd_min, _my_rnd_max);
		push32 (context, value ^ xw);
		popr (context, reg);

		pushr (context, xw);
		popr (context, rl);

		xorr (context, reg, rl);

		popr (context, rl);
	}
}
void random_add_reg (mutation_context_t* context, regbase_t reg, uint32_t value, uint8_t runs)
{
	uint32_t added = 0;
	uint32_t step_total = _my_random (1, value + 0x50);
	for (size_t i = 0; i < runs; i++)
	{
		uint8_t add = step_total / runs;
		addrd (context, vs_dword, reg, add);
		added += add;
	}
	if(added > value)
	{
		subrd (context, vs_dword, reg, added - value);
	}
	else
	{
		addrd (context, vs_dword, reg, value - added);
	}
}
void random_sub_reg (mutation_context_t* context, regbase_t reg, uint32_t value, uint8_t runs)
{
	uint32_t subtracted = 0;
	uint32_t step_total = _my_random (1, value + 0x50);
	for (size_t i = 0; i < runs; i++)
	{
		uint8_t add = step_total / runs;
		subrd (context, vs_dword, reg, add);
		subtracted += add;
	}
	if (subtracted > value)
	{
		addrd (context, vs_dword, reg, subtracted - value);
	}
	else
	{
		subrd (context, vs_dword, reg, value - subtracted);
	}
}

typedef struct _rjjc8r_t
{
	mutation_context_t* ctx;
	uint8_t num_regs;
	regbase_t* regs;
}rjjc8r_t;
void ref_reg (rjjc8r_t* ref, regbase_t reg)
{
	if(ref->regs == 0)
		ref->regs = (regbase_t*)_my_malloc (sizeof(regbase_t) * 10);
	ref->num_regs++;
	*(ref->regs + ref->num_regs - 1) = reg;
	pushr (ref->ctx, reg);
}
void begin_random_jcc8 (uint8_t offset, rjjc8r_t* storage)
{
	jccbase_t type = jb_jl;// _my_random (0, jb_jg);

	uint32_t value;
	regbase_t left;
	regbase_t right;

	switch (type)
	{
	default:
		// 2lazy2implement
		jmp8 (storage->ctx, offset);
		break;

	case jb_jl:
		left = random_reg ();
		right = random_reg ();
		while (right == left)
			right = random_reg ();
		ref_reg (storage, left);
		ref_reg (storage, right);
		value = _my_random (_my_rnd_min, _my_rnd_max);
		random_set_reg (storage->ctx, left, value);
		random_set_reg (storage->ctx, right, value + _my_random (1, 0x40));
		testrr (storage->ctx, vs_dword, left, right);
		jcc8 (storage->ctx, 0, jb_jl, offset);
		break;

	case jb_jnb:
		left = random_reg ();
		right = random_reg ();
		while (right == left)
			right = random_reg ();
		ref_reg (storage, left);
		ref_reg (storage, right);
		value = _my_random (_my_rnd_min, _my_rnd_max);
		random_set_reg (storage->ctx, left, value);
		random_set_reg (storage->ctx, right, value - _my_random (1, 0x40));
		testrr (storage->ctx, vs_dword, left, right);
		jcc8 (storage->ctx, 0, jb_jnb, offset);
		break;

	case jb_jz:
		left = random_reg ();
		right = random_reg ();
		ref_reg (storage, left);
		ref_reg (storage, right);
		while (right == left)
			right = random_reg ();
		value = _my_random (_my_rnd_min, _my_rnd_max);
		random_set_reg (storage->ctx, left, value);
		random_set_reg (storage->ctx, right, value);
		testrr (storage->ctx, vs_dword, left, right);
		jcc8 (storage->ctx, 0, jb_jz, offset);
		break;
	case jb_jnz:
		left = random_reg ();
		right = random_reg ();
		while (right == left)
			right = random_reg ();
		ref_reg (storage, left);
		ref_reg (storage, right);
		value = _my_random (_my_rnd_min, _my_rnd_max);
		random_set_reg (storage->ctx, left, value);
		random_set_reg (storage->ctx, right, !value);
		testrr (storage->ctx, vs_dword, left, right);
		jcc8 (storage->ctx, 0, jb_jnz, offset);
		break;
	case jb_jna:
	case jb_jbe:
		left = random_reg ();
		right = random_reg ();
		while (right == left)
			right = random_reg ();
		ref_reg (storage, left);
		ref_reg (storage, right);
		value = _my_random (_my_rnd_min, _my_rnd_max);
		random_set_reg (storage->ctx, left, value);
		random_set_reg (storage->ctx, right, value - _my_random (0, 0x40));
		testrr (storage->ctx, vs_dword, left, right);
		jcc8 (storage->ctx, 0, jb_jbe, offset);
		break;
	case jb_ja:
		left = random_reg ();
		right = random_reg ();
		while (right == left)
			right = random_reg ();
		ref_reg (storage, left);
		ref_reg (storage, right);
		value = _my_random (_my_rnd_min, _my_rnd_max);
		random_set_reg (storage->ctx, left, value);
		random_set_reg (storage->ctx, right, value - _my_random (1, 0x40));
		testrr (storage->ctx, vs_dword, left, right);
		jcc8 (storage->ctx, 0, jb_ja, offset);
		break;
	}
}
void end_random_jcc8 (rjjc8r_t* storage)
{
	if(storage->num_regs > 0)
	{
		for (size_t n = 0; n < storage->num_regs; n++)
		{
			size_t inv = (storage->num_regs - 1) - n;
			popr (storage->ctx, *(storage->regs + inv));
		}
		_my_free ((uint8_t*)storage->regs);
	}
}

void insert_junkcode(mutation_context_t* context)
{
	// insert some junk
	uint32_t junk = _my_random (_my_junk_min, _my_junk_max);
	if (junk > 0)
	{
		pushfd (context);
		rjjc8r_t random_jump;
		memset (&random_jump, 0, sizeof (rjjc8r_t));
		random_jump.ctx = context;
		if(_my_random(0,1) == 1)
		{
			uint8_t* opcodes = _my_malloc(junk);
			uint32_t* lengths = _my_malloc(sizeof(uint32_t)*junk);

			size_t length = 0;

			size_t n;
			for (n = 0; n < junk; n++)
			{
				opcode_t opc;
				opc.encoded_primary = _my_random (0, 255);
				opc.encoded_secondary = 0;
				opc.primary.d = mt_to_reg;
				opc.primary.sib = 0;
				opc.primary.s = vs_byte;

				*(opcodes + n) = opc.encoded_primary;
				
				uint32_t len = get_instruction_length (&opc);
				*(lengths + n) = len;
				length += len;
			}

			begin_random_jcc8 ((uint8_t)length, &random_jump);
			for (n = 0; n < junk; n++)
			{
				instruction_t instr;

				instr.length = *(lengths + n);

				instr.data = _my_malloc (instr.length);
				memset (instr.data, 0, instr.length);
				*instr.data = *(opcodes + n);

				add_instruction (context, &instr);
			}
			end_random_jcc8 (&random_jump);

			_my_free (opcodes);
			_my_free ((uint8_t*)lengths);
		}
		else
		{
			begin_random_jcc8 (junk, &random_jump);
			instruction_t instr;
			instr.length = junk;
			instr.data = _my_malloc (junk);
			for (size_t n = 0; n < junk; n++)
				*(instr.data + n) = _my_random (0, 255);
			add_instruction (context, &instr);
			end_random_jcc8 (&random_jump);
		}
		popfd (context);
	}
}

size_t prepare_mutations (mutation_context_t * context)
{
	size_t pos;
	size_t instructions;
	uint8_t* data = (uint8_t*)context->original.address;

	if (context->original.length == 0)
		context->original.length = (uint32_t)(get_function_end (data, &instructions) - data);

	for (pos = 0; pos < instructions; pos++)
	{
		uint32_t length = get_instruction_length (data);

		// insert some junk
		insert_junkcode (context);

		if (*data == 0x33) // mutate xor eax, eax
		{
			regbase_t left;
			regbase_t right;
			get_registers (*(data + 1), &left, &right);
			if (left == right) // the register is being set to 0, lets change it to a basically equavilent operation
				random_set_reg (context, left, 0);
			else copy_instr (context, length, data);
		}
		else if (*data >= 0x70 && *data <= 0x7F) // convert it to 32 bit jump, store the destination in the offset bytes
		{
			int8_t offset = *(data + 1);
			jcc32 (context, data, *data - 0x70, data + offset);
		}
		else if (*data == 0xEB)  // convert it to 32 bit jump, store the destination in the offset bytes
		{
			int8_t offset = *(data + 1);
			jmp32 (context, data, (uint32_t)(data + offset));
		}
		else if (*data == 0xE8) // prepare jump for fix, store the destination in the offset bytes
		{
			uint32_t offset = *(uint32_t*)(data + 1);
			call32 (context, data, jmp_get_dst (data, offset));
		}
		else if (*data == 0xE9) // prepare jump for fix, store the destination in the offset bytes
		{
			uint32_t offset = *(uint32_t*)(data + 1);
			jmp32 (context, data, jmp_get_dst (data, offset));
		}
		else if (*data == 0x0F && *(data + 1) >= 0x80 && *(data + 1) <= 0x8F) // prepare jump for fix, store the destination in the offset bytes
		{
			uint32_t offset = *(uint32_t*)(data + 1);
			jcc32 (context, data, *(data + 1) - 0x80, jmp_get_dst (data + 1, offset));
		}
		else copy_instr (context, length, data);

		data += length;
	}

	size_t size = 0;
	for (pos = 0; pos < context->instruction_cache.count; pos++)
		size += ((instruction_t*)context->instruction_cache.data + pos)->length;

	return size;
}

uintptr_t post_mutations (mutation_context_t * context, uintptr_t map_address)
{
	size_t pos;

	uint8_t* data = (uint8_t*)map_address;

	// insert the mutated instructions
	for (pos = 0; pos < context->instruction_cache.count; pos++)
	{
		instruction_t* instr = ((instruction_t*)context->instruction_cache.data + pos);
		instr->new_addr = data;
		memcpy (data, instr->data, instr->length);
		data += instr->length;
	}

	// fix all mutated instructions
	for (pos = 0; pos < context->instruction_cache.count; pos++)
	{
		instruction_t* instr = ((instruction_t*)context->instruction_cache.data + pos);

		uint8_t opc = *instr->data;
		uint8_t opc2 = *(instr->data + 1);

		if (opc == 0xE8 || // call
			opc == 0xE9 || // jump
			(opc == 0x0F && opc2 >= 0x80 && opc2 <= 0x8F) // conditional jump
			)
		{
			uint32_t* offset = instr->new_addr + 1;

			if (opc == 0x0F)
				offset = (uint8_t*)offset + 1;

			*offset = jmp_get_offset (instr->new_addr, *offset);
		}

		_my_free (instr->data);

		data += instr->length;
	}

	return map_address;
}

void dispose_original (mutation_context_t * context, set_is_writable_t set_is_writable, restore_mprot_t restore_mprot)
{
	uint32_t flags;
	set_is_writable (context->original.address, context->original.length, &flags);
	memset (context->original.address, 0x90, context->original.length);
	restore_mprot (context->original.address, context->original.length, flags);
}

mutation_context_t* create_morpher (uint32_t max_instructions)
{
	mutation_context_t* ctx = _my_malloc (sizeof (mutation_context_t));
	ctx->instruction_cache.data = _my_malloc (sizeof (instruction_t) * max_instructions);
	ctx->instruction_cache.count = 0;
	return ctx;
}

void dispose_morpher (mutation_context_t * context)
{
	_my_free (context->instruction_cache.data);
	_my_free (context);
}
