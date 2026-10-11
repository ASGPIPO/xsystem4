/* Horizontal scrollbar skins use separately named CG states. */
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "system4.h"
#include "system4/cg.h"
#include "system4/string.h"
#include "asset_manager.h"
#include "parts.h"
#include "parts_internal.h"

static const char *thumb_skins[] = {
	"\xa3\xaf\xcc\xf5\xa3\xaf\xcd\xa8\xb3\xa3" /* ／条／通常 */,
	"\xa3\xaf\xcc\xf5\xa3\xaf\xbf\xaa" /* ／条／开 */,
	"\xa3\xaf\xcc\xf5\xa3\xaf\xbb\xf7\xb5\xb9" /* ／条／击倒 */
};

static int max_pos(struct parts_hscrollbar *b)
{
	return max(0, b->total - b->view);
}

static int track_width(struct parts_hscrollbar *b)
{
	return b->width - b->left - b->right;
}

static int thumb_width(struct parts_hscrollbar *b)
{
	int track = track_width(b);
	if (!max_pos(b))
		return track;
	return min(track, max(b->thumb_min_width,
			(int)((int64_t)track * b->view / b->total)));
}

static int thumb_offset(struct parts_hscrollbar *b)
{
	int range = max_pos(b);
	return range ? (int)((int64_t)(track_width(b) - thumb_width(b)) * b->pos / range) : 0;
}

static struct string *skin_name(struct string *base, const char *suffix)
{
	struct string *name = string_dup(base);
	string_append_cstr(&name, suffix, strlen(suffix));
	return name;
}

static bool draw_skin(int number, int state, struct string *base, const char *suffix,
		int x, int y, int width, int height)
{
	if (width <= 0 || height <= 0)
		return true;
	struct string *name = skin_name(base, suffix);
	struct cg_metrics m;
	bool ok = asset_cg_get_metrics_by_name(name->text, &m);
	if (ok)
		ok = PE_AddDrawCutCGToPartsConstructionProcess(number, name,
				x, y, width, height, 0, 0, m.w, m.h, 1, state);
	free_string(name);
	return ok;
}

static bool redraw(struct parts *p)
{
	struct parts_hscrollbar *b = p->hscrollbar;
	const char *left[] = {
		"\xa3\xaf\xd7\xf3\xb0\xb4\xc5\xa5\xa3\xaf\xcd\xa8\xb3\xa3" /* ／左按钮／通常 */,
		"\xa3\xaf\xd7\xf3\xb0\xb4\xc5\xa5\xa3\xaf\xbf\xaa" /* ／左按钮／开 */,
		"\xa3\xaf\xd7\xf3\xb0\xb4\xc5\xa5\xa3\xaf\xbb\xf7\xb5\xb9" /* ／左按钮／击倒 */
	};
	const char *right[] = {
		"\xa3\xaf\xd3\xd2\xb0\xb4\xc5\xa5\xa3\xaf\xcd\xa8\xb3\xa3" /* ／右按钮／通常 */,
		"\xa3\xaf\xd3\xd2\xb0\xb4\xc5\xa5\xa3\xaf\xbf\xaa" /* ／右按钮／开 */,
		"\xa3\xaf\xd3\xd2\xb0\xb4\xc5\xa5\xa3\xaf\xbb\xf7\xb5\xb9" /* ／右按钮／击倒 */
	};
	int width = thumb_width(b), x = b->left + thumb_offset(b);
	for (int state = 1; state <= 3; state++) {
		PE_ClearPartsConstructionProcess(p->no, state);
		PE_AddCreateToPartsConstructionProcess(p->no, b->width, b->height, state);
		if (!draw_skin(p->no, state, b->cg_name, "\xa3\xaf\xb1\xb3\xbe\xb0" /* ／背景 */,
					0, 0, b->width, b->height)
				|| !draw_skin(p->no, state, b->cg_name, left[state-1],
					0, 0, b->left, b->height)
				|| !draw_skin(p->no, state, b->cg_name, right[state-1],
					b->width-b->right, 0, b->right, b->height)
				|| !draw_skin(p->no, state, b->cg_name, thumb_skins[state-1],
					x, 0, width, b->height)
				|| !PE_BuildPartsConstructionProcess(p->no, state))
			return false;
	}
	return true;
}

bool PE_CreateHScrollbar(int number, struct string *name, int width, int height,
		int left, int right, int total, int view, int pos, int move)
{
	if (!name || !name->size || width <= 0 || height <= 0
			|| width > 4096 || height > 4096 || left < 0 || right < 0
			|| left > width || right > width-left)
		return false;
	struct string *thumb = skin_name(name, thumb_skins[0]);
	struct cg_metrics m;
	bool valid = asset_cg_get_metrics_by_name(thumb->text, &m) && m.w > 0;
	free_string(thumb);
	if (!valid)
		return false;
	struct parts *p = parts_get(number);
	parts_hscrollbar_free(p);
	p->hscrollbar = xcalloc(1, sizeof(*p->hscrollbar));
	struct parts_hscrollbar *b = p->hscrollbar;
	b->cg_name = string_dup(name);
	b->width = width;
	b->height = height;
	b->left = left;
	b->right = right;
	b->total = max(0, total);
	b->view = max(0, view);
	b->pos = min(max(0, pos), max_pos(b));
	b->move = max(0, move);
	b->thumb_min_width = m.w;
	if (redraw(p)) {
		PE_SetClickable(number, true);
		return true;
	}
	parts_hscrollbar_free(p);
	return false;
}

void parts_hscrollbar_free(struct parts *p)
{
	if (!p->hscrollbar)
		return;
	free_string(p->hscrollbar->cg_name);
	free(p->hscrollbar);
	p->hscrollbar = NULL;
}

static void set_pos(struct parts *p, int pos)
{
	struct parts_hscrollbar *b = p->hscrollbar;
	pos = min(max(0, pos), max_pos(b));
	if (b->pos == pos)
		return;
	b->pos = pos;
	redraw(p);
	parts_msg_push(p, PARTS_MSG_SCROLL, "ii", b->pos, b->total);
}

float PE_GetHScrollbarScrollRate(int number)
{
	struct parts *p = parts_try_get(number);
	struct parts_hscrollbar *b = p ? p->hscrollbar : NULL;
	return b && max_pos(b) ? (float)b->pos / max_pos(b) : 0.0f;
}

void PE_SetHScrollbarScrollRate(int number, float rate)
{
	struct parts *p = parts_try_get(number);
	if (!p || !p->hscrollbar || !isfinite(rate))
		return;
	rate = fminf(1.0f, fmaxf(0.0f, rate));
	/* Use double so an INT_MAX range cannot round past the integer limit. */
	set_pos(p, (int)round((double)rate * max_pos(p->hscrollbar)));
}

void parts_hscrollbar_mouse(struct parts *p, Point mouse, bool clicking,
		bool was_clicking, bool hovered, bool click_available)
{
	struct parts_hscrollbar *b = p->hscrollbar;
	if (!b)
		return;
	if (!clicking) {
		b->dragging = false;
		return;
	}
	int x = mouse.x - p->global.pos.x - p->states[0].common.origin_offset.x;
	if (!was_clicking && hovered && click_available) {
		int thumb = b->left + thumb_offset(b);
		if (x < b->left)
			set_pos(p, max(0, b->pos-b->move));
		else if (x >= b->width-b->right)
			set_pos(p, b->pos + min(b->move, max_pos(b)-b->pos));
		else {
			b->dragging = true;
			b->drag_offset = x >= thumb && x < thumb+thumb_width(b)
					? x-thumb : thumb_width(b)/2;
		}
	}
	if (b->dragging) {
		int travel = track_width(b) - thumb_width(b);
		if (travel > 0) {
			int offset = min(travel, max(0, x-b->left-b->drag_offset));
			set_pos(p, (int)((int64_t)offset * max_pos(b) / travel));
		}
	}
}
