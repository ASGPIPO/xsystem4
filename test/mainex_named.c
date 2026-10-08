/* Exercise both MainEX selector ABIs through libffi and real Rance03 data. */
#include <assert.h>
#include <stdio.h>
#include <ffi.h>
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/little_endian.h"
#include "../src/hll/MainEXFile.c"

struct config config;
struct ain *ain;

const char *display_utf0(const char *text) { return text; }

void static_library_replace(struct static_library *lib, const char *name, void *fun)
{
	for (int i = 0; lib->functions[i].name; i++) {
		if (!strcmp(lib->functions[i].name, name)) {
			lib->functions[i].fun = fun;
			return;
		}
	}
	assert(0);
}

static void *exported(const char *name)
{
	for (int i = 0; lib_MainEXFile.functions[i].name; i++) {
		if (!strcmp(lib_MainEXFile.functions[i].name, name))
			return lib_MainEXFile.functions[i].fun;
	}
	return NULL;
}

static int libno;
static bool legacy;

static int invoke(const char *name, void **args)
{
	int fno = ain_get_library_function(ain, libno, name);
	assert(fno >= 0);
	struct ain_hll_function *f = &ain->libraries[libno].functions[fno];
	assert(f->nr_arguments <= 5);
	ffi_type *types[5];
	for (int i = 0; i < f->nr_arguments; i++) {
		types[i] = f->arguments[i].type.data == AIN_INT
			? &ffi_type_sint32 : &ffi_type_pointer;
	}
	ffi_cif cif;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, f->nr_arguments,
			&ffi_type_sint32, types) == FFI_OK);
	ffi_arg result = 0;
	void *fn = exported(name);
	assert(fn);
	ffi_call(&cif, FFI_FN(fn), &result, args);
	return (int)result;
}

static void *selector(struct string **name, int *handle)
{
	*handle = MainEXFile_Handle(*name);
	return legacy ? (void *)handle : (void *)name;
}

/* Decode the real script wrapper's argument loads before its CALLHLL.
 * HLL0 labels claim Col,Row, but EX_A2String emits Name,LineY,LineX,Data. */
static void script_a2string_order(int order[4])
{
	int fno = ain_get_function(ain, "EX_A2String");
	assert(fno >= 0);
	initialize_instructions(ain->version);
	unsigned p = ain->functions[fno].address;
	int hll_fno = ain_get_library_function(ain, libno, "A2String");
	int loads[16], nr_loads = 0;
	while (p + 12 <= ain->code_size) {
		int op = LittleEndian_getW(ain->code, p);
		assert(op < NR_OPCODES);
		if (op == CALLHLL && LittleEndian_getDW(ain->code, p + 2) == libno
				&& LittleEndian_getDW(ain->code, p + 6) == hll_fno) {
			assert(nr_loads >= 4);
			for (int i = 0; i < 4; i++)
				order[i] = loads[nr_loads - 4 + i];
			assert(order[0] == 0 && order[1] == 2
					&& order[2] == 1 && order[3] == 4);
			puts("PASS real EX_A2String bytecode emits Name,LineY(row),LineX(col),Data");
			fflush(stdout);
			return;
		}
		if (op == PUSHLOCALPAGE && LittleEndian_getW(ain->code, p + 2) == PUSH
				&& (LittleEndian_getW(ain->code, p + 8) == REF
					|| LittleEndian_getW(ain->code, p + 8) == S_REF)) {
			assert(nr_loads < 16);
			loads[nr_loads++] = LittleEndian_getDW(ain->code, p + 4);
		}
		p += instruction_width(op);
	}
	assert(0);
}

int main(int argc, char **argv)
{
	assert(argc == 5);
	legacy = !strcmp(argv[1], "legacy");
	int error = 0;
	ain = ain_open(argv[2], &error);
	assert(ain);
	libno = ain_get_library(ain, "MainEXFile");
	assert(libno >= 0);

	/* Give every formerly handle-based function a legacy AIN signature. */
	if (legacy) {
		for (int i = 0; i < ain->libraries[libno].nr_functions; i++) {
			struct ain_hll_function *f = &ain->libraries[libno].functions[i];
			if (!strcmp(f->name, "Row") || !strcmp(f->name, "Col")
					|| !strcmp(f->name, "Type") || !strcmp(f->name, "Exists")
					|| !strcmp(f->name, "Int") || !strcmp(f->name, "Float")
					|| !strcmp(f->name, "String") || f->name[0] == 'A'
					|| !strncmp(f->name, "IA2", 3) || !strncmp(f->name, "SA2", 3)
					|| !strncmp(f->name, "RA2", 3)
					|| !strncmp(f->name, "GetRowAt", 8)) {
				if (strcmp(f->name, "AddEXText"))
					f->arguments[0].type.data = AIN_INT;
			}
		}
	}
	void *old_row = exported("Row");
	void (*prelink)(void) = exported("_PreLink");
	if (prelink)
		prelink();
	if (legacy)
		assert(exported("Row") == old_row);

	config.ex_path = argv[3];
	MainEXFile_ModuleInit();

	/* String 1808 is the exact player-table key passed by loadPlayer. */
	int string_index = atoi(argv[4]);
	assert(string_index >= 0 && string_index < ain->nr_strings);
	struct string *name = ain->strings[string_index];
	struct ex_table *table = ex_get_table(ex, name->text);
	assert(table && table->nr_rows > 1 && table->nr_columns > 1);
	int handle;
	void *args[5] = { selector(&name, &handle) };
	int rows = invoke("Row", args);
	printf("%s real player table expected=%u actual=%d columns=%u\n",
			argv[1], table->nr_rows, rows, table->nr_columns);
	fflush(stdout);
	assert(rows == (int)table->nr_rows);
	assert(invoke("Col", args) == (int)table->nr_columns);
	assert(invoke("Type", args) == EX_TABLE);
	assert(invoke("Exists", args));

	/* Evaluate the script wrapper's actual argument loads. Expected IDs come
	 * from each real row, independently of the C adapter's coordinates. */
	int order[4];
	script_a2string_order(order);
	for (int row = 0; row < rows; row++) {
		int col = 0;
		struct string *out = NULL;
		struct string **ref = &out;
		void *locals[] = { selector(&name, &handle), &col, &row, NULL, &ref };
		for (int i = 0; i < 4; i++)
			args[i] = locals[order[i]];
		assert(invoke("A2Type", args) == table->rows[row][col].type);
		assert(invoke("A2Exists", args));
		assert(invoke("A2String", args));
		if (strcmp(out->text, table->rows[row][col].s->text))
			fprintf(stderr, "script route row=%d col=%d expected ID=[%s] actual=[%s]\n",
					row, col, table->rows[row][col].s->text, out->text);
		assert(!strcmp(out->text, table->rows[row][col].s->text));
		free_string(out);
	}
	printf("PASS %s all %d player IDs and A2 coordinate ordering\n", argv[1], rows);

	int numeric_cells = 0;
	for (unsigned row = 0; row < table->nr_rows; row++) {
		for (unsigned col = 0; col < table->nr_columns; col++) {
			struct ex_value *v = &table->rows[row][col];
			args[1] = &row;
			args[2] = &col;
			if (v->type == EX_INT) {
				int out = -1;
				int *ref = &out;
				args[3] = &ref;
				assert(invoke("A2Int", args));
				assert(out == v->i);
				numeric_cells++;
			} else if (v->type == EX_FLOAT) {
				float out = -1;
				float *ref = &out;
				args[3] = &ref;
				assert(invoke("A2Float", args));
				assert(out == v->f);
				numeric_cells++;
			}
		}
	}
	assert(numeric_cells > 0);
	printf("PASS %s %d real numeric cells\n", argv[1], numeric_cells);

	/* Reproduce Player@loadParam's keyed skill lookup and loadZeroThreeSkill's
	 * positional ID read. Every player skill must resolve to the real table. */
	struct string *skill_name = ain->strings[1838];
	struct ex_table *skills = ex_get_table(ex, skill_name->text);
	struct string *format = cstr_to_string("zeroThreeSkillId");
	int skill_col = ex_col_from_name(table, format->text);
	assert(skill_col >= 0 && skills);
	for (unsigned row = 0; row < table->nr_rows; row++) {
		struct string *id = table->rows[row][0].s;
		struct string *out = NULL;
		struct string **ref = &out;
		args[0] = selector(&name, &handle);
		args[1] = &id;
		args[2] = &format;
		args[3] = &ref;
		assert(invoke("SA2String", args));
		assert(table->rows[row][skill_col].type == EX_STRING);
		assert(!strcmp(out->text, table->rows[row][skill_col].s->text));
		int skill_row = ex_row_at_string_key(skills, out->text);
		assert(skill_row >= 0);
		free_string(out);
		out = NULL;
		int col = 0;
		void *locals[] = { selector(&skill_name, &handle),
				&col, &skill_row, NULL, &ref };
		for (int i = 0; i < 4; i++)
			args[i] = locals[order[i]];
		assert(invoke("A2String", args));
		assert(!strcmp(out->text, skills->rows[skill_row][0].s->text));
		free_string(out);
	}
	free_string(format);
	printf("PASS %s all 23 player skill references resolve through script-order skill IDs\n", argv[1]);

	/* Every selector export must safely reject an unknown table/value name. */
	struct string *missing = cstr_to_string("__missing_MainEX_value__");
	int bad_handle = 0, index = -1, data = 123;
	float fdata = 123;
	struct string *sdata = NULL;
	struct string **sref = &sdata;
	int *iref = &data;
	float *fref = &fdata;
	int tested = 0;
	for (int i = 0; i < ain->libraries[libno].nr_functions; i++) {
		struct ain_hll_function *f = &ain->libraries[libno].functions[i];
		if (f->nr_arguments == 0 || !strcmp(f->name, "AddEXText")
				|| !strncmp(f->name, "GetNode", 7) || !strncmp(f->name, "GetEX", 5))
			continue;
		args[0] = legacy ? (void *)&bad_handle : (void *)&missing;
		for (int j = 1; j < f->nr_arguments; j++) {
			switch (f->arguments[j].type.data) {
			case AIN_INT: args[j] = &index; break;
			case AIN_STRING: args[j] = &missing; break;
			case AIN_REF_INT: args[j] = &iref; break;
			case AIN_REF_FLOAT: args[j] = &fref; break;
			case AIN_REF_STRING: args[j] = &sref; break;
			default: assert(0);
			}
		}
		int result = invoke(f->name, args);
		assert(result == (!strncmp(f->name, "GetRowAt", 8) ? -1 : 0));
		tested++;
	}
	assert(tested == 34);
	assert(data == 123 && fdata == 123 && sdata == NULL);
	printf("PASS %s %d missing-name/invalid-handle contracts\n", argv[1], tested);
	free_string(missing);

	MainEXFile_ModuleFini();
	ain_free(ain);
	return 0;
}
