/* Runs the actual opcode handler and heap/destructor code, without SDL startup. */
#include <stdio.h>
#include "../src/vm.c"

static void release_with_opcode(int opcode, int slot)
{
	stack_push(0x13579);
	stack_push(slot);
	instr_ptr = 32;
	execute_instruction(opcode);
	assert(stack_ptr == 1 && stack_pop().i == 0x13579);
}

int main(void)
{
#ifdef DEBUGGER_ENABLED
	dbg_enabled = false;
#endif
	uint8_t code[64] = {0};
	LittleEndian_putW(code, 0, PUSHGLOBALPAGE);
	LittleEndian_putW(code, 2, PUSH);
	LittleEndian_putDW(code, 4, 0);
	LittleEndian_putW(code, 8, INC);
	LittleEndian_putW(code, 10, RETURN);
	LittleEndian_putW(code, 32, CHECKUDO);
	struct ain_variable global = { .type = AIN_MKTYPE(AIN_INT, -1, 0) };
	struct ain_function funcs[2] = { {0}, { .name = "count_destructor", .address = 0,
		.return_type = AIN_VOID_TYPE, .struct_type = 0 } };
	struct ain_struct type = { .name = "Tracked", .constructor = -1, .destructor = 1 };
	struct ain program = { .version = 6, .minor_version = 1, .code = code,
		.code_size = sizeof(code), .nr_globals = 1, .globals = &global,
		.nr_functions = 2, .functions = funcs, .nr_structures = 1, .structures = &type };
	ain = &program;
	initialize_instructions(6);
	stack = xcalloc(INITIAL_STACK_SIZE, sizeof(*stack));
	stack_size = INITIAL_STACK_SIZE;
	heap_init();
	heap[0].ref = 1;
	heap[0].type = VM_PAGE;
	heap_set_page(0, alloc_page(GLOBAL_PAGE, 0, 1));

	for (int i = 0; i < 2; i++) {
		int op = i ? DELETE : CHECKUDO;
		int before = heap[0].page->values[0].i;
		release_with_opcode(op, -1);
		assert(heap[0].page->values[0].i == before);
		union vm_value value;
		create_struct(0, &value);
		release_with_opcode(op, value.i);
		assert(heap[value.i].ref == 0);
		assert(heap[0].page->values[0].i == before + 1);
		create_struct(0, &value);
		heap_ref(value.i);
		release_with_opcode(op, value.i);
		assert(heap[value.i].ref == 1);
		assert(heap[0].page->values[0].i == before + 1);
		release_with_opcode(op, value.i);
		assert(heap[value.i].ref == 0);
		assert(heap[0].page->values[0].i == before + 2);
		assert(call_stack_ptr == 0 && stack_ptr == 0);
		printf("PASS %s: null, unique destructor once, shared ref preserved, stack pop one\n",
			i ? "DELETE control" : "CHECKUDO");
	}
	return 0;
}
