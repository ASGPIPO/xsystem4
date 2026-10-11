/* P2 buttons use a construction process for each input state. */
#include "system4.h"
#include "system4/string.h"
#include "parts.h"
#include "parts_internal.h"

static struct parts_cp_op *button_label(struct parts_construction_process *cproc)
{
	struct parts_cp_op *op, *label = NULL;
	TAILQ_FOREACH(op, &cproc->ops, entry) {
		if (op->type == PARTS_CP_DRAW_TEXT)
			label = op;
	}
	return label;
}

static void center_label(struct parts_cp_op *op, int width, int height)
{
	op->text.x = max(0, (width - (int)gfx_size_text(&op->text.style, op->text.text->text)) / 2);
	op->text.y = max(0, (height - (int)op->text.style.size) / 2);
}

void PE_SetButtonSize(int number, int width, int height)
{
	if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
		return;

	struct parts *p = parts_get(number);
	bool initialize = false;
	for (int state = 0; state < PARTS_NR_STATES; state++) {
		if (p->states[state].type != PARTS_CONSTRUCTION_PROCESS)
			initialize = true;
	}
	for (int state = 0; state < PARTS_NR_STATES; state++) {
		struct parts_construction_process *cproc = parts_get_construction_process(p, state);
		int old_width = cproc->common.w, old_height = cproc->common.h;
		bool have_create = false;
		struct parts_cp_op *op;
		TAILQ_FOREACH(op, &cproc->ops, entry) {
			if (op->type == PARTS_CP_CREATE || op->type == PARTS_CP_CREATE_PIXEL_ONLY) {
				op->create.w = width;
				op->create.h = height;
				have_create = true;
			} else if (op->type == PARTS_CP_DRAW_CUT_CG
					&& op->cut_cg.dx == 0 && op->cut_cg.dy == 0
					&& op->cut_cg.dw == old_width && op->cut_cg.dh == old_height) {
				/* Resize the skin destination; preserve its source CG and crop. */
				op->cut_cg.dw = width;
				op->cut_cg.dh = height;
			}
		}
		if (!have_create)
			PE_AddCreateToPartsConstructionProcess(number, width, height, state + 1);
		op = button_label(cproc);
		if (op)
			center_label(op, width, height);
		if (!PE_BuildPartsConstructionProcess(number, state + 1)) {
			WARNING("Failed to rebuild button %d state %d", number, state + 1);
			return;
		}
	}
	if (initialize)
		PE_SetClickable(number, true);
}

void PE_SetButtonText(int number, struct string *text)
{
	struct parts *p = parts_try_get(number);
	if (!p || !text)
		return;
	for (int state = 0; state < PARTS_NR_STATES; state++) {
		if (p->states[state].type != PARTS_CONSTRUCTION_PROCESS)
			return;
	}

	for (int state = 0; state < PARTS_NR_STATES; state++) {
		struct parts_construction_process *cproc = &p->states[state].cproc;
		struct parts_cp_op *op = button_label(cproc);
		if (op) {
			struct string *old = op->text.text;
			op->text.text = string_dup(text);
			free_string(old);
		} else {
			/* Use the same text defaults as ordinary parts, including font style. */
			struct parts_state defaults = {0};
			parts_state_reset(&defaults, PARTS_TEXT);
			op = xcalloc(1, sizeof(*op));
			op->type = PARTS_CP_DRAW_TEXT;
			op->text.text = string_dup(text);
			op->text.style = defaults.text.ts;
			parts_state_reset(&defaults, PARTS_UNINITIALIZED);
			parts_add_cp_op(cproc, op);
		}
		center_label(op, cproc->common.w, cproc->common.h);
		if (!PE_BuildPartsConstructionProcess(number, state + 1)) {
			WARNING("Failed to rebuild button %d state %d", number, state + 1);
			return;
		}
	}
}
