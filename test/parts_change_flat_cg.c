/* Real AIN ABI and actual FLAT/CG replacement regression.
 * Run on a private Xvfb display; no game VM is executed.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ffi.h>
#include "system4/ain.h"
#include "system4/cg.h"
#include "system4/flat.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "parts.h"
#include "scene.h"
#include "vm/heap.h"
#include "xsystem4.h"
#include "../src/parts/parts_internal.h"
#include "../src/hll/hll.h"

extern struct static_library lib_PartsEngine;
static void *change_cg;

static uint64_t pixel_hash(const void *data, size_t size)
{
	const uint8_t *p = data;
	uint64_t h = 1469598103934665603ULL;
	for (size_t i = 0; i < size; i++)
		h = (h ^ p[i]) * 1099511628211ULL;
	return h;
}

static uint8_t *texture_pixels(Texture *t)
{
	size_t size = (size_t)t->w * t->h * 4;
	uint8_t *pixels = malloc(size);
	assert(pixels && t->handle && glIsTexture(t->handle));
	GLuint fbo = gfx_set_framebuffer(GL_FRAMEBUFFER, t, 0, 0, t->w, t->h);
	glReadPixels(0, 0, t->w, t->h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	gfx_reset_framebuffer(GL_FRAMEBUFFER, fbo);
	assert(glGetError() == GL_NO_ERROR);
	return pixels;
}

static void replace(int number, struct string *old, struct string *new_name, int state)
{
	assert(change_cg);
	ffi_cif cif;
	ffi_type *types[] = { &ffi_type_sint32, &ffi_type_pointer,
		&ffi_type_pointer, &ffi_type_sint32 };
	void *args[] = { &number, &old, &new_name, &state };
	unsigned int old_refs = old ? old->ref : 0;
	unsigned int new_refs = new_name ? new_name->ref : 0;
	assert(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 4, &ffi_type_void, types) == FFI_OK);
	ffi_call(&cif, FFI_FN(change_cg), NULL, args);
	assert(!old || old->ref == old_refs);
	assert(!new_name || new_name->ref == new_refs);
}

static void verify_texture(Texture *t, struct cg *cg)
{
	assert(t->w == cg->metrics.w && t->h == cg->metrics.h);
	uint8_t *pixels = texture_pixels(t);
	assert(!memcmp(pixels, cg->pixels, (size_t)t->w * t->h * 4));
	printf("PASS texture %dx%d equals actual CG RGBA hash=%016llx\n", t->w, t->h,
			(unsigned long long)pixel_hash(pixels, (size_t)t->w * t->h * 4));
	free(pixels);
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	initialize_instructions(ain->version);
	heap_init();
	config.game_name = utf2sjis("兰斯03", 0);
	config.ain_filename = "Rance03.ain";
	config.game_dir = "/root/rance03";
	config.view_width = 1024;
	config.view_height = 768;
	config.default_volume = 100;
	char *channels[] = { "Master", "BGM", "SE", "Voice" };
	int volumes[] = { 100, 100, 100, 100 };
	config.mixer_nr_channels = 4;
	config.mixer_channels = channels;
	config.mixer_volumes = volumes;
	asset_manager_init();
	assert(PE_Init());

	int libno = ain_get_library(ain, "PartsEngine");
	int fn = ain_get_library_function(ain, libno, "Parts_ChangeFlatCG");
	assert(fn >= 0);
	struct ain_hll_function *signature = &ain->libraries[libno].functions[fn];
	assert(signature->return_type.data == AIN_VOID && signature->nr_arguments == 4);
	assert(signature->arguments[0].type.data == AIN_INT
			&& signature->arguments[1].type.data == AIN_STRING
			&& signature->arguments[2].type.data == AIN_STRING
			&& signature->arguments[3].type.data == AIN_INT);
	for (int i = 0; lib_PartsEngine.functions[i].name; i++)
		if (!strcmp(lib_PartsEngine.functions[i].name, "Parts_ChangeFlatCG"))
			change_cg = lib_PartsEngine.functions[i].fun;

	int number = 72000;
	struct string *filename = ain->strings[1053];
	struct string *old = ain->strings[1054];
	struct string *new_name = ain->strings[1093];
	assert(PE_SetPartsFlatAndStop(number, filename, 1));
	struct parts *parts = parts_try_get(number);
	assert(parts && parts->states[0].type == PARTS_FLAT);
	struct parts_flat *f = &parts->states[0].flat;
	int index = parts_flat_find_library(f->flat, old->text);
	assert(index >= 0 && f->flat->libraries[index].type == FLAT_LIB_CG);
	printf("REAL FLAT %s: %zu libraries, old %s index%d texture=%dx%d\n",
			display_sjis0(filename->text), f->nr_libraries,
			display_sjis1(old->text), index, f->textures[index].w, f->textures[index].h);
	struct cg *cg = asset_cg_load_by_name(new_name->text, NULL);
	assert(cg);
	printf("REAL replacement %s: CG=%dx%d; real AIN void(int,string VALUE,string VALUE,int), binding=%s\n",
			display_sjis0(new_name->text), cg->metrics.w, cg->metrics.h,
			change_cg ? "present" : "MISSING");
	assert(change_cg);

	struct parts_flat before = *f;
	struct flat_layer_state layer_before = *f->root_state;
	Texture *textures = malloc(sizeof(Texture) * f->nr_libraries);
	assert(textures);
	memcpy(textures, f->textures, sizeof(Texture) * f->nr_libraries);
	uint8_t *old_pixels = texture_pixels(&f->textures[index]);
	uint64_t old_hash = pixel_hash(old_pixels,
			(size_t)f->textures[index].w * f->textures[index].h * 4);
	free(old_pixels);
	GLuint old_handle = f->textures[index].handle;
	parts_engine_clean();
	scene_is_dirty = false;
	replace(number, old, new_name, 1);
	assert(!memcmp(&before, f, sizeof(*f)));
	assert(!memcmp(&layer_before, f->root_state, sizeof(layer_before)));
	assert(!glIsTexture(old_handle));
	verify_texture(&f->textures[index], cg);
	assert(old_hash != pixel_hash(cg->pixels, (size_t)cg->metrics.w * cg->metrics.h * 4));
	for (size_t i = 0; i < f->nr_libraries; i++)
		if ((int)i != index)
			assert(!memcmp(&textures[i], &f->textures[i], sizeof(Texture)));
	parts_render_update();
	assert(scene_is_dirty);
	puts("PASS replaced only target CG, retained libraries/flat frame/timer/common/state, marked scene dirty");

	GLuint replaced_handle = f->textures[index].handle;
	replace(number, old, new_name, 1);
	assert(!glIsTexture(replaced_handle));
	verify_texture(&f->textures[index], cg);

	int second_name = 1067; /* actual DialogInfomation trap picture */
	struct cg *second_cg = asset_cg_load_by_name(ain->strings[second_name]->text, NULL);
	assert(second_cg);
	replace(number, old, ain->strings[second_name], 1);
	verify_texture(&f->textures[index], second_cg);
	assert(!memcmp(&before, f, sizeof(*f)));
	assert(!memcmp(&layer_before, f->root_state, sizeof(layer_before)));

	struct string *missing = make_string("__missing_flat_cg__", 19);
	Texture final = f->textures[index];
	replace(number, missing, new_name, 1);
	replace(number, old, missing, 1);
	replace(number, NULL, new_name, 1);
	replace(number, old, NULL, 1);
	replace(number, old, new_name, 0);
	replace(number, old, new_name, 4);
	replace(number, old, new_name, INT_MIN);
	replace(number + 1, old, new_name, 1);
	assert(!parts_try_get(number + 1));
	replace(number, old, new_name, 2);
	assert(parts->states[1].type == PARTS_UNINITIALIZED);
	enum flat_library_type type = f->flat->libraries[index].type;
	f->flat->libraries[index].type = FLAT_LIB_MEMORY;
	replace(number, old, new_name, 1);
	f->flat->libraries[index].type = type;
	assert(!memcmp(&final, &f->textures[index], sizeof(Texture)));
	assert(!memcmp(&before, f, sizeof(*f)));
	puts("PASS invalid state/part/component/name/asset/type safely leaves target unchanged; value string refs retained");

	free_string(missing);
	cg_free(cg);
	cg_free(second_cg);
	free(textures);
	GLuint last_handle = f->textures[index].handle;
	PE_ReleaseParts(number);
	assert(!glIsTexture(last_handle) && !parts_try_get(number));
	assert(glGetError() == GL_NO_ERROR);
	PE_Reset();
	ain_free(ain);
	free(config.game_name);
	puts("PASS final release destroys replacement GL texture; real AIN libffi FLAT CG regression complete");
	return 0;
}
