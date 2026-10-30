/* Cropped sprite frames must keep their anchor when mirrored. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/instructions.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "parts.h"
#include "xsystem4.h"
#include "vm/heap.h"
#include "vm.h"
#include "../src/parts/parts_internal.h"

static unsigned char *render(int number)
{
	Texture target = {0};
	gfx_init_texture_rgba(&target, 1024, 768, (SDL_Color){0,0,0,0});
	GLuint fbo = gfx_set_framebuffer(GL_FRAMEBUFFER, &target, 0, 0, 1024, 768);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
	parts_render(parts_try_get(number));
	gfx_reset_framebuffer(GL_FRAMEBUFFER, fbo);
	unsigned char *pixels = gfx_get_pixels(&target);
	gfx_delete_texture(&target);
	assert(pixels && glGetError() == GL_NO_ERROR);
	return pixels;
}

static void bounds(const unsigned char *pixels, int *left, int *right)
{
	*left = 1024;
	*right = -1;
	for (int y = 0; y < 768; y++)
		for (int x = 0; x < 1024; x++) {
			const unsigned char *p = pixels + (y * 1024 + x) * 4;
			if (p[0] || p[1] || p[2]) {
				if (x < *left) *left = x;
				if (x > *right) *right = x;
			}
		}
}

static int synthetic_crops(void)
{
	const int no = 72001, parent = 72002;
	unsigned char pattern[32 * 24 * 4];
	for (int y = 0; y < 24; y++)
		for (int x = 0; x < 32; x++) {
			unsigned char *pixel = pattern + (y * 32 + x) * 4;
			pixel[0] = 10 + x * 3;
			pixel[1] = 10 + y * 5;
			pixel[2] = 99;
			pixel[3] = 255;
		}
	struct parts *p = parts_get(no);
	struct parts_common *common = &parts_get_cg(p, 0)->common;
	gfx_init_texture_with_pixels(&common->texture, 32, 24, pattern);
	parts_set_dims(p, common, 32, 24);
	PE_SetParentPartsNumber(no, parent);
	PE_SetPos(parent, 30, 40);
	PE_SetPos(no, 120, 100);
	PE_SetShow(no, true);
	int failures = 0;
	for (int cropped = 0; cropped <= 1; cropped++)
		for (int scale = 1; scale <= 2; scale++)
			for (int origin = 1; origin <= 9; origin++) {
				int w = cropped ? 7 : 32, h = cropped ? 9 : 24;
				int ox[] = {0, -w/2, -w};
				int oy[] = {0, -h/2, -h};
				int left = 150 + ox[(origin - 1) % 3] * scale;
				int top = 140 + oy[(origin - 1) / 3] * scale;
				PE_SetPartsCGSurfaceArea(no, cropped ? 5 : 0,
						cropped ? 3 : 0, cropped ? w : 0, cropped ? h : 0, 1);
				PE_SetPartsOriginPosMode(no, origin);
				PE_SetPartsMagX(no, scale);
				PE_SetPartsMagY(no, scale);
				p->sprite_deform = 0;
				PE_UpdateComponent(0);
				unsigned char *reference = render(no);
				for (int deform = 0; deform <= 2; deform++) {
					p->sprite_deform = deform;
					unsigned char *pixels = render(no);
					int differences = 0;
					for (int y = 0; y < 768; y++)
						for (int x = 0; x < 1024; x++) {
							int rx = deform == 1 ? 2 * left + w * scale - 1 - x : x;
							int ry = deform == 2 ? 2 * top + h * scale - 1 - y : y;
							unsigned char zero[4] = {0};
							const unsigned char *expected = rx >= 0 && rx < 1024 && ry >= 0 && ry < 768
								? reference + (ry * 1024 + rx) * 4 : zero;
							if (memcmp(pixels + (y * 1024 + x) * 4, expected, 4))
								differences++;
						}
					printf("SYNTH crop=%d scale=%d origin=%d mirror=%d mismatch=%d\n",
							cropped, scale, origin, deform, differences);
					failures += differences != 0;
					free(pixels);
				}
				free(reference);
			}
	PE_ReleaseParts(no);
	PE_ReleaseParts(parent);
	return failures;
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
	config.game_name = "Map sprite crop regression";
	config.ain_filename = "Rance03.ain";
	config.game_dir = "/root/rance03";
	config.view_width = 1024;
	config.view_height = 768;
	config.default_volume = 100;
	char *channels[] = {"Master", "BGM", "SE", "Voice"};
	int volumes[] = {100,100,100,100};
	config.mixer_nr_channels = 4;
	config.mixer_channels = channels;
	config.mixer_volumes = volumes;
	asset_manager_init();
	assert(PE_Init());
	int failures = 0;
	for (int asset = 1160; asset <= 1161; asset++) {
		const int no = 72000;
		assert(PE_SetPartsCG(no, ain->strings[asset], 0, 1));
		struct parts *p = parts_try_get(no);
		int width = p->states[0].common.w;
		int height = p->states[0].common.h;
		int frame_width = width / 4;
		assert(width % 4 == 0 && frame_width > 0);
		PE_SetPartsOriginPosMode(no, 2);
		PE_SetPos(no, 512, 300);
		PE_SetShow(no, true);
		PE_UpdateComponent(0);
		printf("ASSET %d sheet=%dx%d frame=%dx%d\n",
				asset, width, height, frame_width, height);
		for (int frame = 0; frame < 4; frame++) {
			assert(PE_SetPartsCG(no, ain->strings[asset], 0, 1));
			assert(PE_SetPartsCGSurfaceArea(no, frame * frame_width, 0,
					frame_width, height, 1));
			unsigned char *reference = render(no);
			for (int deform = 0; deform <= 2; deform++) {
				assert(PE_SetPartsCG(no, ain->strings[asset], deform, 1));
				assert(PE_SetPartsCGSurfaceArea(no, frame * frame_width, 0,
						frame_width, height, 1));
				unsigned char *pixels = render(no);
				int differences = 0;
				int left = 512 - frame_width / 2;
				for (int y = 0; y < 768; y++)
					for (int x = 0; x < 1024; x++) {
						int rx = deform == 1 ? 2 * left + frame_width - 1 - x : x;
						int ry = deform == 2 ? 600 + height - 1 - y : y;
						unsigned char zero[4] = {0};
						const unsigned char *expected = rx >= 0 && rx < 1024 && ry >= 0 && ry < 768
							? reference + (ry * 1024 + rx) * 4 : zero;
						if (memcmp(pixels + (y * 1024 + x) * 4, expected, 4))
							differences++;
					}
				int l, r;
				bounds(pixels, &l, &r);
				printf("FRAME asset=%d frame=%d mirror=%d logical=(%d,%d) visible_x=%d..%d mismatch=%d\n",
						asset, frame, deform, p->local.pos.x, p->local.pos.y, l, r, differences);
				failures += differences != 0;
				free(pixels);
			}
			free(reference);
		}
		PE_ReleaseParts(no);
	}
	failures += synthetic_crops();
	printf("RESULT failures=%d\n", failures);
	return failures ? 1 : 0;
}
