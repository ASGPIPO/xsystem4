/* Regression against the installed translated AIN and actual sound archive.
 * Link with the engine objects and run without executing the game VM.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ffi.h>

#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/buffer.h"
#include "system4/hashtable.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "xsystem4.h"
#include "../src/hll/hll.h"

extern struct static_library lib_KiwiSoundEngine;
void apply_game_specific_hacks(struct ain *ain);

static int32_t operand(struct ain *a, size_t address)
{
	struct buffer in;
	buffer_init(&in, a->code, a->code_size);
	buffer_seek(&in, address);
	return buffer_read_int32(&in);
}

static bool kiwi_exists(struct string *name)
{
	void *fn = NULL;
	for (int i = 0; lib_KiwiSoundEngine.functions[i].name; i++) {
		if (!strcmp(lib_KiwiSoundEngine.functions[i].name, "IsExistFile"))
			fn = lib_KiwiSoundEngine.functions[i].fun;
	}
	assert(fn);
	unsigned int refs = name->ref;
	ffi_cif cif;
	ffi_type *types[] = { &ffi_type_pointer };
	void *args[] = { &name };
	ffi_arg result = 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_sint32, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(fn), &result, args);
	assert(name->ref == refs);
	return result != 0;
}

static struct string *basename_for(struct afa_archive *ar, int id)
{
	assert(id < (int)ar->nr_files);
	const struct string *name = ar->files[id].name;
	assert(name->size > 4 && !strcmp(name->text + name->size - 4, ".ogg"));
	return make_string(name->text, name->size - 4);
}

static void assert_unchanged(struct ain *a, const uint8_t *before, const char *label)
{
	apply_game_specific_hacks(a);
	assert(!memcmp(before, a->code, a->code_size));
	printf("PASS guard %s leaves all bytecode unchanged\n", label);
}

static void test_guards(struct ain *a, uint8_t *before, int fno)
{
	char *game_name = config.game_name;
	config.game_name = "Other game";
	assert_unchanged(a, before, "other game");
	config.game_name = game_name;
	sjis_set_encoding(SJIS_ENCODING_CP932);
	assert_unchanged(a, before, "CP932");
	sjis_set_encoding(SJIS_ENCODING_CP936);

	int version = a->version;
	a->version = 5;
	assert_unchanged(a, before, "other AIN version");
	a->version = version;

	int nr_strings = a->nr_strings;
	a->nr_strings = 1252;
	assert_unchanged(a, before, "prefix missing");
	a->nr_strings = nr_strings;

	struct string *prefix = a->strings[1252];
	a->strings[1252] = make_string("another/", 8);
	assert_unchanged(a, before, "different prefix bytes");
	free_string(a->strings[1252]);
	a->strings[1252] = make_string(prefix->text, prefix->size + 1);
	assert_unchanged(a, before, "different prefix byte length");
	free_string(a->strings[1252]);
	a->strings[1252] = prefix;

	struct hash_table *functions = a->_func_ht;
	a->_func_ht = ht_create(16);
	assert_unchanged(a, before, "function missing");
	ht_free(a->_func_ht);
	a->_func_ht = functions;

	size_t address = a->functions[fno].address;
	a->code[address + 0x0c] ^= 1;
	memcpy(before, a->code, a->code_size);
	assert_unchanged(a, before, "changed opcode in full pattern");
	a->code[address + 0x0c] ^= 1;
	memcpy(before, a->code, a->code_size);

	int32_t next = a->functions[fno + 1].address;
	a->functions[fno + 1].address = address + 44;
	assert_unchanged(a, before, "short function boundary");
	a->functions[fno + 1].address = next;

	a->functions[fno].address = a->code_size - 10;
	assert_unchanged(a, before, "code range too short");
	a->functions[fno].address = address;
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	config.game_name = utf2sjis("兰斯03", 0);
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain && ain->version == 6);
	int fno = ain_get_function(ain, "DamageEffect@playSound");
	assert(fno >= 0 && fno + 1 < ain->nr_functions);
	size_t address = ain->functions[fno].address;
	size_t index_operand = address + 0x16;
	size_t length_operand = address + 0x26;
	assert(operand(ain, index_operand) == 8 && operand(ain, length_operand) == 8);
	assert(sjis_count_char(ain->strings[1252]->text) == 5);
	assert(ain->strings[1252]->size == 9);
	unsigned int prefix_refs = ain->strings[1252]->ref;
	uint8_t *before = malloc(ain->code_size);
	assert(before);
	memcpy(before, ain->code, ain->code_size);
	test_guards(ain, before, fno);

	struct afa_archive *ar = afa_open("/root/rance03/Rance03Sound.afa", ARCHIVE_MMAP, &error);
	assert(ar);
	asset_manager_init();
	int libno = ain_get_library(ain, "KiwiSoundEngine");
	int fn = ain_get_library_function(ain, libno, "IsExistFile");
	assert(fn >= 0);
	assert(ain->libraries[libno].functions[fn].nr_arguments == 1);
	assert(ain->libraries[libno].functions[fn].arguments[0].type.data == AIN_STRING);
	assert(ain->libraries[libno].functions[fn].return_type.data == AIN_BOOL);
	struct string *full[2], *expected[2];
	int ids[] = { 14, 32 };
	for (int i = 0; i < 2; i++) {
		expected[i] = basename_for(ar, ids[i]);
		full[i] = string_concatenate(ain->strings[1252], expected[i]);
		if (!i)
			assert(!strcmp(full[i]->text, ain->strings[2035]->text));
		unsigned int refs = full[i]->ref;
		int length = sjis_count_char(full[i]->text);
		struct string *old = string_copy(full[i], operand(ain, index_operand),
				length - operand(ain, length_operand));
		assert(full[i]->ref == refs);
		printf("BEFORE AFA entry %d: %s; Kiwi exists=%d\n",
				ids[i], display_sjis0(old->text), kiwi_exists(old));
		assert(!kiwi_exists(old));
		free_string(old);
	}

	apply_game_specific_hacks(ain);
	int prefix_chars = sjis_count_char(ain->strings[1252]->text);
	for (int i = 0; i < 2; i++) {
		unsigned int refs = full[i]->ref;
		struct string *name = string_copy(full[i], operand(ain, index_operand),
				sjis_count_char(full[i]->text) - operand(ain, length_operand));
		int actual_id = -1;
		bool found = asset_exists_by_name(ASSET_SOUND, name->text, &actual_id);
		printf("AFTER AFA entry %d: %s; archive exists=%d/id=%d; Kiwi exists=%d\n",
				ids[i], display_sjis0(name->text), found, actual_id, kiwi_exists(name));
		assert(!strcmp(name->text, expected[i]->text));
		// The engine asset API exposes AFA entry numbers with one-based IDs.
		assert(found && actual_id == ar->files[ids[i]].no + 1 && kiwi_exists(name));
		assert(full[i]->ref == refs);
		free_string(name);
		free_string(full[i]);
		free_string(expected[i]);
	}
	assert(operand(ain, index_operand) == prefix_chars);
	assert(operand(ain, length_operand) == prefix_chars);
	assert(sjis_get_encoding() == SJIS_ENCODING_CP936);
	assert(ain->strings[1252]->size == 9 && ain->strings[1252]->ref == prefix_refs);
	size_t differences = 0;
	for (size_t i = 0; i < ain->code_size; i++) {
		if (ain->code[i] == before[i])
			continue;
		assert((i >= index_operand && i < index_operand + 4)
				|| (i >= length_operand && i < length_operand + 4));
		differences++;
	}
	assert(differences == 2);
	memcpy(before, ain->code, ain->code_size);
	assert_unchanged(ain, before, "repeated apply");
	printf("PASS real AIN+real AFA entries14/32+Kiwi ffi, prefix chars=%d, only two operand bytes changed; CP936/refcounts retained\n", prefix_chars);
	archive_free(&ar->ar);
	free(before);
	ain_free(ain);
	free(config.game_name);
	return 0;
}
