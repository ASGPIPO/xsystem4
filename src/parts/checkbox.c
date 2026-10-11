/* Checkbox skins use separate CGs for checked and unchecked input states. */
#include <string.h>
#include "system4.h"
#include "system4/cg.h"
#include "system4/string.h"
#include "asset_manager.h"
#include "parts.h"
#include "parts_internal.h"

static const char *skins[2][3] = {
	{
		"\xa3\xaf\xcd\xa8\xb3\xa3",
		"\xa3\xaf\xbf\xaa",
		"\xa3\xaf\xbb\xf7\xb5\xb9"
	},
	{
		"\xa3\xaf" "Check\xa3\xaf\xcd\xa8\xb3\xa3",
		"\xa3\xaf" "Check\xa3\xaf\xbf\xaa",
		"\xa3\xaf" "Check\xa3\xaf\xbb\xf7\xb5\xb9"
	}
};

static bool redraw(struct parts *p)
{
	struct parts_checkbox *b = p->checkbox;
	for (int state = 1; state <= 3; state++) {
		struct string *name = string_dup(b->cg_name);
		const char *suffix = skins[b->checked][state-1];
		string_append_cstr(&name, suffix, strlen(suffix));
		struct cg_metrics m;
		bool ok = asset_cg_get_metrics_by_name(name->text, &m);
		if (ok) {
			PE_ClearPartsConstructionProcess(p->no, state);
			PE_AddCreateToPartsConstructionProcess(p->no, b->width, b->height, state);
			ok = PE_AddDrawCutCGToPartsConstructionProcess(p->no, name,
					0, 0, b->width, b->height, 0, 0, m.w, m.h, 1, state)
				&& PE_BuildPartsConstructionProcess(p->no, state);
		}
		free_string(name);
		if (!ok)
			return false;
	}
	return true;
}

bool PE_CreateCheckBox(int number, struct string *name, int width, int height,
		bool checked, int mode, int red, int green, int blue)
{
	if (!name || !name->size || width <= 0 || height <= 0
			|| width > 4096 || height > 4096 || mode != 0)
		return false;
	struct parts *p = parts_get(number);
	parts_checkbox_free(p);
	p->checkbox = xcalloc(1, sizeof(*p->checkbox));
	p->checkbox->cg_name = string_dup(name);
	p->checkbox->width = width;
	p->checkbox->height = height;
	p->checkbox->checked = checked;
	p->checkbox->red = min(255, max(0, red));
	p->checkbox->green = min(255, max(0, green));
	p->checkbox->blue = min(255, max(0, blue));
	if (redraw(p)) {
		PE_SetClickable(number, true);
		parts_checkbox_apply_color(p);
		return true;
	}
	parts_checkbox_free(p);
	return false;
}

void parts_checkbox_free(struct parts *p)
{
	if (!p->checkbox)
		return;
	free_string(p->checkbox->cg_name);
	free(p->checkbox);
	p->checkbox = NULL;
}

void parts_checkbox_apply_color(struct parts *p)
{
	if (p->checkbox)
		PE_SetMultiplyColor(p->no, p->checkbox->red, p->checkbox->green, p->checkbox->blue);
}

void PE_CheckBoxChecked(int number, bool checked)
{
	struct parts *p = parts_try_get(number);
	if (!p || !p->checkbox || p->checkbox->checked == checked)
		return;
	p->checkbox->checked = checked;
	if (!redraw(p)) {
		p->checkbox->checked = !checked;
		redraw(p);
	}
}

bool PE_IsCheckBoxChecked(int number)
{
	struct parts *p = parts_try_get(number);
	return p && p->checkbox && p->checkbox->checked;
}

void PE_SetCheckBoxColor(int number, int red, int green, int blue)
{
	struct parts *p = parts_try_get(number);
	if (!p || !p->checkbox)
		return;
	p->checkbox->red = min(255, max(0, red));
	p->checkbox->green = min(255, max(0, green));
	p->checkbox->blue = min(255, max(0, blue));
	parts_checkbox_apply_color(p);
}

void parts_checkbox_toggle(struct parts *p)
{
	if (p->checkbox)
		PE_CheckBoxChecked(p->no, !p->checkbox->checked);
}
