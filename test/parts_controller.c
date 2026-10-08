/* Exercise explicit Rance03 top IDs against the actual PartsEngine implementation. */
#define VM_PRIVATE
#include <stdio.h>
#include "system4/instructions.h"
#include "../src/parts/parts.c"
#include "debugger.h"
#include "vm/heap.h"

int main(void)
{
#ifdef DEBUGGER_ENABLED
	dbg_enabled = false;
#endif
	uint8_t code[2] = {0};
	struct ain program = { .version = 6, .minor_version = 1, .code = code, .code_size = 2 };
	ain = &program;
	initialize_instructions(6);
	heap_init();
	parts_table = ht_create(1024);
	ctrl_stack_init();
	assert(PE_get_active_controller() == 0 && PE_get_nr_controllers() == 1);
	parts_get(100)->delegate_index = 10;
	assert(PE_AddController(0) == 1);
	parts_get(101)->delegate_index = 11;
	PE_set_active_controller(0);
	assert(PE_AddController(1) == 2); // explicit top independent of current active
	parts_get(102)->delegate_index = 12;
	struct page *erased = NULL;
	PE_RemoveController(&erased, 2);
	assert(erased && erased->nr_vars == 1 && erased->values[0].i == 12);
	assert(!PE_IsExist(102) && PE_IsExist(100) && PE_IsExist(101));
	assert(PE_get_active_controller() == 1 && PE_get_nr_controllers() == 2);
	PE_RemoveController(&erased, -1);
	assert(erased->nr_vars == 2 && erased->values[1].i == 11);
	assert(!PE_IsExist(101) && PE_IsExist(100));
	assert(PE_get_active_controller() == 0 && PE_get_nr_controllers() == 1);
	PE_RemoveController(&erased, 0);
	assert(!PE_IsExist(100));
	assert(PE_get_active_controller() == 0 && PE_get_nr_controllers() == 1);
	assert(PE_AddController(-1) == 1);
	puts("PASS explicit top insertion with lower active controller, explicit and -1 removal");
	puts("PASS removed controller releases its parts, preserves other parts, returns delegates");
	puts("PASS removing final controller restores a default controller");
	return 0;
}
