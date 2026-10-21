/* GPU regressions for backlog fixes. Uses synthetic pixels and text only. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "parts.h"
#include "vm.h"
#include "xsystem4.h"
#include "../src/parts/parts_internal.h"

static void canvas(int no, int w, int h)
{
	assert(PE_AddCreateToPartsConstructionProcess(no, w, h, 1));
	assert(PE_AddFillWithAlphaToPartsConstructionProcess(no, 0, 0, w, h,
			255, 255, 255, 255, 1));
	assert(PE_BuildPartsConstructionProcess(no, 1));
}
static int draw_pixel(int no, int x, int y)
{
	Texture target = {0};
	gfx_init_texture_rgba(&target, 320, 240, (SDL_Color){0,0,0,0});
	GLuint fbo = gfx_set_framebuffer(GL_FRAMEBUFFER, &target, 0, 0, 320, 240);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
	parts_render(parts_try_get(no));
	gfx_reset_framebuffer(GL_FRAMEBUFFER, fbo);
	SDL_Color color = gfx_get_pixel(&target, x, y);
	gfx_delete_texture(&target);
	assert(glGetError() == GL_NO_ERROR);
	return color.r;
}
static int layout(void)
{
	int box = PE_GetFreeNumber(), child = PE_GetFreeNumber();
	PE_SetLayoutBoxLayoutType(box, 1);
	PE_SetLayoutBoxAlign(box, 1);
	PE_SetPos(box, 10, 10);
	canvas(child, 20, 20);
	PE_AddChild(box, child);
	PE_UpdateComponent(0);
	assert(draw_pixel(child, 15, 15) == 255);
	PE_SetPos(box, 100, 80);
	PE_UpdateComponent(0);
	int pixel = draw_pixel(child, 105, 85);
	printf("moved layout child pixel=%d expected=255\n", pixel);
	if (pixel != 255) return 1;
	/* Explicit surface cropping must still apply after a parent moves. */
	assert(PE_SetPartsConstructionSurfaceArea(child, 0, 0, 8, 8, 1));
	assert(draw_pixel(child, 105, 85) == 255);
	assert(draw_pixel(child, 112, 85) == 0);
	puts("PASS moving layout and explicit surface crop");
	return 0;
}
static int messages(void)
{
	int no = PE_GetFreeNumber();
	struct parts *p = parts_get(no);
	/* No skin needed: the test exercises the real queue and scroll API. */
	p->vscrollbar = calloc(1, sizeof(*p->vscrollbar));
	assert(p->vscrollbar);
	p->vscrollbar->cg_name = cstr_to_string("__missing_test_skin__");
	p->vscrollbar->total = 13;
	p->vscrollbar->view = 22;
	PE_ReleaseMessage();
	PE_SetVScrollbarScrollPos(no, 13);
	assert(PE_GetVScrollbarScrollPos(no) == 0);
	assert(PE_GetMessageType() == PARTS_MSG_SCROLL);
	assert(PE_GetMessageVariableInt(0) == 0);
	assert(PE_GetMessageVariableInt(1) == 13);
	assert(PE_GetMessageDelegateIndex() == -1);
	PE_SetDelegateIndex(no, 42);
	assert(PE_GetMessageDelegateIndex() == 42);
	PE_ReleaseMessage();
	PE_SetVScrollbarScrollPos(no, 0);
	assert(PE_GetMessageType() == PARTS_MSG_SCROLL);
	PE_ReleaseMessage();
	p->vscrollbar->total = 100;
	PE_SetVScrollbarScrollPos(no, 100);
	assert(PE_GetVScrollbarScrollPos(no) == 78);
	assert(PE_GetMessageVariableInt(0) == 78);
	assert(PE_GetVScrollbarScrollRate(no) == 1.0f);
	PE_ReleaseMessage();
	puts("PASS short/long history, unchanged position event and late delegate");
	return 0;
}
static int text(void)
{
	int no = PE_GetFreeNumber();
	assert(PE_AddCreateToPartsConstructionProcess(no, 100, 100, 1));
	assert(PE_AddFillWithAlphaToPartsConstructionProcess(no, 0, 0, 100, 100, 0,0,0,0,1));
	struct string *s = cstr_to_string("MMMM\nMMMM\nMMMM");
	assert(PE_AddCopyTextToPartsConstructionProcess(no, 0, 0, s, 0, 22,
			255,255,255,0, 0,0,0,0, 0,2,1));
	free_string(s);
	assert(PE_BuildPartsConstructionProcess(no, 1));
	Texture *t = &parts_try_get(no)->states[0].common.texture;
	unsigned char *pixels = gfx_get_pixels(t);
	int count[3] = {0}, gap = 0;
	for (int y = 0; y < 100; y++)
		for (int x = 0; x < 100; x++)
			if (pixels[(y*100+x)*4+3]) {
				if (y < 22) count[0]++;
				else if (y >= 24 && y < 46) count[1]++;
				else if (y >= 48 && y < 70) count[2]++;
				else gap++;
			}
	free(pixels);
	printf("three lines alpha pixels=%d/%d/%d outside=%d\n",
			count[0],count[1],count[2],gap);
	assert(count[0] > 100 && count[1] == count[0] && count[2] == count[0]);
	assert(gap == 0);
	puts("PASS CopyText newline and line spacing pixels");
	return 0;
}

static void add_text(int no, int y, struct string *s, int size, float bold, float edge)
{
	assert(PE_AddCopyTextToPartsConstructionProcess(no, 0, y, s, 0, size,
			255,255,255,bold, 0,0,0,edge, 0,2,1));
}
static int metrics(void)
{
	const int sizes[] = {22,17,22,17};
	const float bold[] = {0,0,1,0.3f}, edge[] = {0,0,1,0.8f};
	struct string *one = cstr_to_string("MMMM"), *multi = cstr_to_string("MMMM\nMMMM\nMMMM");
	int failures = 0;
	for (int i = 0; i < 4; i++) {
		int a = PE_GetFreeNumber(), b = PE_GetFreeNumber();
		assert(PE_AddCreateToPartsConstructionProcess(a, 100, 120, 1));
		assert(PE_AddCreateToPartsConstructionProcess(b, 100, 120, 1));
		/* CASCharSpriteProperty@GetPixelHeight (real AIN F87):
		 * round size up to even, plus twice the larger rounded stroke. */
		int stroke = (int)ceilf(fmaxf(bold[i], edge[i]));
		int advance = ((sizes[i] + 1) & ~1) + 2 * stroke + 2;
		add_text(a, 0, multi, sizes[i], bold[i], edge[i]);
		for (int line = 0; line < 3; line++)
			add_text(b, line*advance, one, sizes[i], bold[i], edge[i]);
		assert(PE_BuildPartsConstructionProcess(a, 1));
		assert(PE_BuildPartsConstructionProcess(b, 1));
		void *actual = gfx_get_pixels(&parts_try_get(a)->states[0].common.texture);
		void *expected = gfx_get_pixels(&parts_try_get(b)->states[0].common.texture);
		int equal = !memcmp(actual, expected, 100*120*4);
		printf("multiline metric size=%d bold=%.1f edge=%.1f advance=%d equal=%d\n",
				sizes[i],bold[i],edge[i],advance,equal);
		failures += !equal;
		free(actual); free(expected);
		PE_ReleaseParts(a); PE_ReleaseParts(b);
	}
	free_string(one); free_string(multi);
	return failures ? 1 : 0;
}


static int controls(void)
{
	int error;
	struct afa_archive *pact = afa_open("/root/rance03/Rance03Pact.afa",0,&error);
	assert(pact && pact->nr_files);
	struct string *activity = cstr_to_string("backlog-controls-regression");
	assert(PE_ReadActivityFile(activity, pact->files[0].name));
	int controls = 0, failures = 0;
	struct string *name = cstr_to_string("");
	for (int i = 0; i < 7; i++) {
		int no;
		assert(PE_GetActivityParts(i, activity, &name, &no));
		struct parts *p = parts_try_get(no);
		if (p->states[0].type != PARTS_CONSTRUCTION_PROCESS) continue;
		controls++;
		for (int state = 0; state < 3; state++) {
			Texture *t = &p->states[state].common.texture;
			assert(t->handle);
			unsigned char *pixels = gfx_get_pixels(t);
			int visible = 0;
			for (int j = 0; j < t->w*t->h; j++) visible += pixels[j*4+3] != 0;
			free(pixels);
			printf("backlog control=%d kind=%s state=%d visible_pixels=%d\n",
					i,p->vscrollbar ? "scrollbar" : "button",state+1,visible);
			failures += visible == 0;
		}
	}
	assert(controls == 2);
	free_string(name); free_string(activity);
	return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	setbuf(stdout, NULL);
	sjis_set_encoding(SJIS_ENCODING_CP936);
	int error;
	ain = ain_open("/root/rance03/Rance03.ain", &error);
	assert(ain);
	config.game_dir = "/root/rance03";
	config.ain_filename = "Rance03.ain";
	config.game_name = "Backlog synthetic regression";
	config.view_width = 320;
	config.view_height = 240;
	config.font_paths[0] = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";
	config.font_paths[1] = config.font_paths[0];
	asset_manager_init();
	PE_enable_multi_controller();
	assert(PE_Init());
	if (!strcmp(argv[1], "layout")) return layout();
	if (!strcmp(argv[1], "messages")) return messages();
	if (!strcmp(argv[1], "text")) return text();
	if (!strcmp(argv[1], "metrics")) return metrics();
	if (!strcmp(argv[1], "controls")) return controls();
	return 2;
}
