/* Real settings PACT load, navigation ABI and alternate-state regression. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "system4/ain.h"
#include "system4/archive.h"
#include "system4/ex.h"
#include "system4/utfsjis.h"
#include "system4/string.h"
#include "asset_manager.h"
#include "parts.h"
#include "xsystem4.h"
#include "vm.h"
#include "../src/parts/parts_internal.h"
#include "../src/hll/hll.h"
extern struct static_library lib_PartsEngine;

static void *bound(const char *name)
{
	int lib = ain_get_library(ain, "PartsEngine");
	assert(lib >= 0 && ain_get_library_function(ain, lib, name) >= 0);
	for (int i = 0; lib_PartsEngine.functions[i].name; i++)
		if (!strcmp(lib_PartsEngine.functions[i].name, name))
			return lib_PartsEngine.functions[i].fun;
	fprintf(stderr, "Missing required PartsEngine.%s\n", name);
	return NULL;
}

static struct ex_tree *find_child(struct ex_tree *t, const char *name)
{
	if (!t || t->is_leaf) return NULL;
	for (unsigned i = 0; i < t->nr_children; i++) {
		struct ex_tree *c = &t->children[i];
		if (!strcmp(c->name->text, name)) return c;
		struct ex_tree *r = find_child(c, name);
		if (r) return r;
	}
	return NULL;
}

/* First 迁移先 string of the named part in a page EX tree. */
static struct string *destination_of(struct ex *ex, const char *part_name)
{
	struct ex_tree *part = find_child(ex_get_tree(ex, "\xbb\xee\xb6\xaf" /* 活动 */), part_name);
	assert(part);
	struct ex_tree *legacy = find_child(part, "\xa5\xa4\xa5\xf3\xa5\xc6\xa5\xf3\xa5\xc8\xc7\xe9\x88\xf3" /* インテント情報 */);
	struct ex_tree *dest = find_child(part, "\xdf\x77\xd2\xc6\xcf\xc8" /* 迁移先 */);
	if (!dest) {
		assert(legacy);
		dest = find_child(legacy, "\xdf\x77\xd2\xc6\xcf\xc8" /* 迁移先 */);
	}
	assert(dest && dest->is_leaf && dest->leaf.value.type == EX_LIST);
	struct ex_value *v = ex_list_get(dest->leaf.value.list, 0);
	assert(v && v->type == EX_STRING && v->s && v->s->size);
	return string_ref(v->s);
}

int main(void)
{
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	config.game_name = "Ordinary settings pages regression";
	config.view_width = 1024;
	config.view_height = 768;
	asset_manager_init();
	PE_Init();
	const char *required[] = {"GetActivityPartsName","IsExistActivityPartsByNumber","NumofActivityIntentDataDestination","GetActivityIntentDataDestination","GetActivityIntentDataType"};
	for (unsigned i=0; i<sizeof(required)/sizeof(required[0]); i++) assert(bound(required[i]));
	struct string *name = cstr_to_string("settings-pages");
	struct string *file = cstr_to_string("\xc5\xe4\xd6\xc3\xa3\xaf\xa3\xb0\xa3\xb1\xa3\xaf\xb6\xd4\xb0\xd7\xcb\xd9\xb6\xc8\xd2\xb3.pact");
	/* Button on page i navigates to page i+1. */
	const char *buttons[] = {
		"\xb6\xd4\xb0\xd7\xcb\xd9\xb6\xc8\xb0\xb4\xc5\xa5:0",
		"\xb6\xd4\xb0\xd7\xb0\xb4\xc5\xa5",
		"\xb6\xd4\xb0\xd7\xb4\xb0\xbf\xda\xb0\xb4\xc5\xa5",
		"\xc9\xf9\xd2\xf4\xb0\xb4\xc5\xa5",
		"\xca\xe4\xc8\xeb\xb0\xb4\xc5\xa5",
		"\xb4\xb0\xbf\xda\xb0\xb4\xc5\xa5",
		"\xcd\xf8\xc2\xe7\xb0\xb4\xc5\xa5"
	};
	struct string *current = string_ref(file);
	int failures = 0;
	for (int tab = 1; tab <= 7; tab++) {
		/* Parse the EX for the next destination before loading. */
		struct archive_data *d = asset_get_by_name(ASSET_PACT, current->text, NULL);
		assert(d);
		struct ex *ex = ex_read(d->data, d->size);
		archive_free_data(d);
		assert(ex);
		struct string *next = tab < 7 ? destination_of(ex, buttons[tab]) : NULL;
		ex_free(ex);
		bool loaded = PE_ReadActivityFile(name, current);
		printf("READ tab=%d result=%d\n", tab, loaded);
		assert(loaded);
		int count = PE_NumofActivityParts(name);
		printf("PARTS tab=%d count=%d\n", tab, count);
		assert(count > 0);
		if (tab == 1)
			assert(count == 36);
		if (tab == 2) {
			struct string *label = cstr_to_string("\x53\x59\x53\x5f\xb5\xe3\xbb\xf7\xbd\xe2\xb3\xfd\xcc\xf8\xb9\xfd\xc4\xa3\xca\xbd");
			int n = PE_GetActivityPartsNumber(name,label);
			int width = PE_GetPartsWidth(n,1);
			printf("CHECKBOX labelled width=%d\n",width);
			if (width <= 50) failures++;
			Texture *texture = &parts_try_get(n)->states[0].common.texture;
			uint8_t *pixels = gfx_get_pixels(texture);
			assert(pixels);
			int ink = 0;
			for (int y=0; y<texture->h; y++) for (int x=58; x<texture->w; x++)
				if (pixels[(y*texture->w+x)*4+3]) ink++;
			free(pixels);
			printf("CHECKBOX label visible pixels=%d\n",ink);
			if (!ink) failures++;
			free_string(label);
		}
		if (tab == 4) {
			struct string *bar = cstr_to_string("\xd6\xf7\xbb\xac\xbf\xe9");
			int n = PE_GetActivityPartsNumber(name,bar);
			PE_SetHScrollbarScrollRate(n,1.0f);
			float rate = PE_GetHScrollbarScrollRate(n);
			printf("IMAGELESS hscroll set=1 get=%g\n",rate);
			if (rate != 1.0f) failures++;
			struct string *box = cstr_to_string("\xb2\xbc\xbe\xd6\xbf\xf2\x5f\x30\x30\x30");
			n = PE_GetActivityPartsNumber(name,box);
			int w=PE_GetPartsWidth(n,1),h=PE_GetPartsHeight(n,1);
			printf("FREE layout width=%d height=%d\n",w,h);
			if (w <= 0 || h <= 0) failures++;
			struct string *visible = cstr_to_string("SYS_\xd6\xf7\xbb\xac\xbf\xe9");
			n = PE_GetActivityPartsNumber(name,visible);
			float x=PE_parts_get_absolute_x(n),y=PE_parts_get_absolute_y(n);
			printf("SOUND visible slider position=%g,%g\n",x,y);
			assert(x >= 0 && x+PE_GetPartsWidth(n,1) <= 1024 && y >= 0 && y+PE_GetPartsHeight(n,1) <= 768);
			free_string(visible);
			free_string(bar);free_string(box);
		}
		if (tab == 1) {
			struct string *missing = cstr_to_string("SYS_\xd6\xf7\xbb\xac\xbf\xe9");
			int absent=PE_GetActivityPartsNumber(name,missing);
			printf("MISSING part number=%d expected=0\n",absent);
			if (absent != 0) failures++;
			free_string(missing);
			struct string *background = cstr_to_string("\xb1\xb3\xbe\xb0");
			int bg = PE_GetActivityPartsNumber(name, background);
			assert(bg >= 0);
			PE_SetInputState(bg, 2);
			printf("BACKGROUND hover state=%d\n", PE_GetInputState(bg));
			assert(PE_GetInputState(bg) == 1);
			PE_SetInputState(bg, 3);
			assert(PE_GetInputState(bg) == 1);
			free_string(background);
		}
		if (next) {
			struct string *button = cstr_to_string(buttons[tab]);
			assert(PE_IsExistActivityIntentData(name, button));
			int (*count)(struct string*,struct string*) = bound("NumofActivityIntentDataDestination");
			int (*type)(struct string*,struct string*) = bound("GetActivityIntentDataType");
			void (*destination)(struct string**,int,struct string*,struct string*) = bound("GetActivityIntentDataDestination");
			void (*part_name)(struct string**,struct string*,int) = bound("GetActivityPartsName");
			bool (*exists)(struct string*,int) = bound("IsExistActivityPartsByNumber");
			int number = PE_GetActivityPartsNumber(name,button);
			struct string *out = cstr_to_string("");
			assert(number >= 0 && exists(name,number) && !exists(name,-1));
			part_name(&out,name,number); assert(!strcmp(out->text,button->text));
			assert(count(name,button)==1 && type(name,button)==1);
			destination(&out,0,name,button); assert(!strcmp(out->text,next->text));
			destination(&out,-1,name,button); assert(out->size==0);
			destination(&out,1,name,button); assert(out->size==0);
			part_name(&out,name,-1); assert(out->size==0);
			free_string(out);free_string(button);
		}
		assert(PE_ReleaseActivity(name, NULL));
		assert(!PE_IsExistActivity(name));
		free_string(current);
		current = next;
	}
	free_string(name);
	PE_Reset();
	ain_free(ain);
	assert(failures == 0);
	puts("PASS real seven ordinary settings pages load and release");
	return 0;
}
