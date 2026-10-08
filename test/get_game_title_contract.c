/* Exercise the real HLL binding and the startup checks decoded from Rance03.ain. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void *selected_title;
void record_replacement(const char *name, void *fun);
#define static_library_replace(lib, name, fun) record_replacement(name, fun)
#include "../src/hll/SystemService.c"
#undef static_library_replace

struct config config;
struct ain *ain;
bool vm_reset_once;

void record_replacement(const char *name, void *fun)
{
	if (!strcmp(name, "GetGameTitle"))
		selected_title = fun;
}

static void check_part(struct string *s, int start, int len, const char *expected)
{
	struct string *part = string_copy(s, start, len);
	assert(!strcmp(part->text, expected));
	free_string(part);
}

int main(void)
{
	struct string *marker = cstr_to_string("oi@64sd54di8zd5");
	struct ain_hll_argument arg = { .name = "Name", .type = AIN_MKTYPE(AIN_REF_STRING, -1, 0) };
	struct ain_hll_function fun = { .name = "GetGameTitle", .return_type = AIN_VOID_TYPE,
		.nr_arguments = 1, .arguments = &arg };
	struct ain_library lib = { .name = "SystemService", .nr_functions = 1, .functions = &fun };
	struct ain program = { .version = 6, .game_version = 100, .nr_strings = 1,
		.strings = &marker, .nr_libraries = 1, .libraries = &lib };
	ain = &program;
	config.game_name = "Chinese display title";
	selected_title = SystemService_GetGameName;
	SystemService_PreLink();
	struct string *title = cstr_to_string("previous output");
	struct string *old = string_ref(title);
	((void (*)(struct string **))selected_title)(&title);
	assert(old->ref == 1);
	free_string(old);
	check_part(title, 4, 7, "oi@64sd");
	check_part(title, 14, 5, "54di8");
	check_part(title, 32, 3, "zd5");
	struct string *a = string_copy(title, 7, 1);
	struct string *b = string_copy(title, 31, 1);
	assert(string_to_integer(a) == string_to_integer(b));
	free_string(a); free_string(b);
	a = string_copy(title, 13, 1); b = string_copy(title, 19, 1);
	assert(string_to_integer(a) * string_to_integer(b) == 21);
	free_string(a); free_string(b); free_string(title);
	puts("PASS Rance03 main: substring token, digit equality, product 21, ref release");

	for (int i = 0; i < 4; i++) {
		program.version = i == 0 ? 7 : 6;
		program.game_version = i == 1 ? 101 : 100;
		fun.return_type.data = i == 2 ? AIN_INT : AIN_VOID;
		arg.type.data = i == 3 ? AIN_STRING : AIN_REF_STRING;
		selected_title = SystemService_GetGameName;
		SystemService_PreLink();
		assert(selected_title == SystemService_GetGameName);
	}
	puts("PASS unrelated version, game version and incompatible HLL signatures unchanged");
	program.version = 6; program.game_version = 100;
	fun.return_type.data = AIN_VOID; arg.type.data = AIN_REF_STRING;
	struct ain no_marker = program;
	no_marker._string_ht = NULL; no_marker.nr_strings = 0;
	ain = &no_marker; selected_title = SystemService_GetGameName;
	SystemService_PreLink();
	assert(selected_title == SystemService_GetGameName);
	puts("PASS game without Rance03 protocol marker unchanged");
	return 0;
}
