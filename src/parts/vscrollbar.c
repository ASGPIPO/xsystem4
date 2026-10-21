/* Vertical scrollbar skins use a directory of CG states, not an atlas. */
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "system4.h"
#include "system4/cg.h"
#include "system4/string.h"
#include "asset_manager.h"
#include "parts.h"
#include "parts_internal.h"

static struct parts_vscrollbar *get_bar(int number)
{
	struct parts *p = parts_try_get(number);
	return p ? p->vscrollbar : NULL;
}
static int max_pos(struct parts_vscrollbar *b)
{
	// Standard scrollbar semantics: the position is the index of the first
	// visible item, so the maximum is total - view. Rance03's backlog passes
	// NumofLine() as the "scroll to bottom" position and expects the engine
	// to clamp it to the last page.
	return max(0, b->total - b->view);
}
static int track_height(struct parts_vscrollbar *b)
{
	return max(0,b->height-b->up-b->down);
}
static int thumb_height(struct parts_vscrollbar *b)
{
	int track = track_height(b);
	if (b->total <= b->view || b->total <= 0) return track;
	return min(track,max(min(16,track),(int)((int64_t)track*b->view/b->total)));
}
static int thumb_offset(struct parts_vscrollbar *b)
{
	int range = max_pos(b);
	return range > 0 ? (int)((int64_t)(track_height(b)-thumb_height(b))*b->pos/range) : 0;
}
static struct string *skin_name(struct string *base, const char *suffix)
{
	struct string *name = string_dup(base);
	string_append_cstr(&name,suffix,strlen(suffix));
	return name;
}
/* Unlike DrawCutCG, skin layering must retain straight RGBA for later
 * composition over the scene. Keep this operation in the construction list
 * so rebuilding and XPE loading reproduce the same pixels. */
bool parts_vscrollbar_build_skin(Texture *dst, struct parts_cp_cut_cg *op)
{
	if (!dst->handle) return false;
	if (op->dw <= 0 || op->dh <= 0) return true;
	struct cg *cg = asset_cg_load_by_name(op->cg_name->text, NULL);
	if (!cg) return false;
	Texture src = {0}, background = {0};
	gfx_init_texture_with_cg(&src, cg);
	cg_free(cg);
	gfx_init_texture_blank(&background, dst->w, dst->h);
	gfx_copy_with_alpha_map(&background, 0, 0, dst, 0, 0, dst->w, dst->h);

	static Shader shader;
	if (!shader.program)
		gfx_load_shader(&shader, "shaders/render.v.glsl", "shaders/parts_vscrollbar_over.f.glsl");
	mat4 world = WORLD_TRANSFORM(op->dw, op->dh, op->dx, op->dy);
	mat4 view = WV_TRANSFORM(dst->w, dst->h);
	struct gfx_render_job job = {
		.shader = &shader, .shape = GFX_RECTANGLE, .texture = src.handle,
		.world_transform = world[0], .view_transform = view[0]
	};
	GLuint fbo = gfx_set_framebuffer(GL_DRAW_FRAMEBUFFER, dst, 0, 0, dst->w, dst->h);
	gfx_prepare_job(&job);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, background.handle);
	glUniform1i(glGetUniformLocation(shader.program, "background"), 1);
	glUniform4f(glGetUniformLocation(shader.program, "source_rect"),
			(float)op->sx/src.w, (float)op->sy/src.h,
			(float)op->sw/src.w, (float)op->sh/src.h);
	glActiveTexture(GL_TEXTURE0);
	GLint blend[4];
	glGetIntegerv(GL_BLEND_SRC_RGB, &blend[0]);
	glGetIntegerv(GL_BLEND_DST_RGB, &blend[1]);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend[2]);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blend[3]);
	glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
	gfx_run_job(&job);
	glBlendFuncSeparate(blend[0], blend[1], blend[2], blend[3]);
	gfx_reset_framebuffer(GL_DRAW_FRAMEBUFFER, fbo);
	gfx_delete_texture(&background);
	gfx_delete_texture(&src);
	return true;
}
static bool draw_skin(int number, int state, struct string *base, const char *suffix,
		int x,int y,int w,int h)
{
	if (w <= 0 || h <= 0) return true;
	struct string *name = skin_name(base,suffix);
	struct cg_metrics m;
	bool ok = asset_cg_get_metrics_by_name(name->text,&m);
	if (ok) {
		struct parts_cp_op *op = xcalloc(1, sizeof(*op));
		op->type = PARTS_CP_SKIN_CUT_CG;
		op->cut_cg = (struct parts_cp_cut_cg) {
			.cg_name = string_ref(name), .dx = x, .dy = y, .dw = w, .dh = h,
			.sw = m.w, .sh = m.h, .interp_type = 1
		};
		parts_add_cp_op(parts_get_construction_process(parts_get(number), state-1), op);
	}
	free_string(name);
	return ok;
}
static bool redraw(struct parts *p)
{
	struct parts_vscrollbar *b = p->vscrollbar;
	const char *up[] = {"\xa3\xaf\xc9\xcf\xb0\xb4\xc5\xa5\xa3\xaf\xcd\xa8\xb3\xa3" /* ／上按钮／通常 */,"\xa3\xaf\xc9\xcf\xb0\xb4\xc5\xa5\xa3\xaf\xbf\xaa" /* ／上按钮／开 */,"\xa3\xaf\xc9\xcf\xb0\xb4\xc5\xa5\xa3\xaf\xbb\xf7\xb5\xb9" /* ／上按钮／击倒 */};
	const char *down[] = {"\xa3\xaf\xcf\xc2\xb0\xb4\xc5\xa5\xa3\xaf\xcd\xa8\xb3\xa3" /* ／下按钮／通常 */,"\xa3\xaf\xcf\xc2\xb0\xb4\xc5\xa5\xa3\xaf\xbf\xaa" /* ／下按钮／开 */,"\xa3\xaf\xcf\xc2\xb0\xb4\xc5\xa5\xa3\xaf\xbb\xf7\xb5\xb9" /* ／下按钮／击倒 */};
	const char *thumb[] = {"\xa3\xaf\xcc\xf5\xa3\xaf\xcd\xa8\xb3\xa3" /* ／条／通常 */,"\xa3\xaf\xcc\xf5\xa3\xaf\xbf\xaa" /* ／条／开 */,"\xa3\xaf\xcc\xf5\xa3\xaf\xbb\xf7\xb5\xb9" /* ／条／击倒 */};
	int height = thumb_height(b), y = b->up+thumb_offset(b);
	for (int s = 1; s <= 3; s++) {
		PE_ClearPartsConstructionProcess(p->no,s);
		PE_AddCreateToPartsConstructionProcess(p->no,b->width,b->height,s);
		if (b->width > 0 && b->height > 0)
			PE_AddFillWithAlphaToPartsConstructionProcess(p->no,0,0,b->width,b->height,0,0,0,0,s);
		if (!draw_skin(p->no,s,b->cg_name,"\xa3\xaf\xb1\xb3\xbe\xb0" /* ／背景 */,0,0,b->width,b->height)
			|| !draw_skin(p->no,s,b->cg_name,up[s-1],0,0,b->width,b->up)
			|| !draw_skin(p->no,s,b->cg_name,down[s-1],0,b->height-b->down,b->width,b->down)
			|| !draw_skin(p->no,s,b->cg_name,thumb[s-1],0,y,b->width,height)
			|| !PE_BuildPartsConstructionProcess(p->no,s))
			return false;
	}
	return true;
}
/* Core position update. The drag path calls this every frame while the thumb
 * is held, so it only notifies on an actual change; the exported
 * PE_SetVScrollbarScrollPos forces the notification (see there). */
static void vscrollbar_set_pos(struct parts *p, int pos, bool force_notify)
{
	struct parts_vscrollbar *b = p->vscrollbar;
	pos = min(max(0,pos),max_pos(b));
	if (!force_notify && b->pos == pos) return;
	b->pos = pos; redraw(p);
	parts_msg_push(p,PARTS_MSG_SCROLL,"ii",b->pos,b->total);
}
bool PE_CreateVScrollbar(int number, struct string *name, int width, int height,
		int up,int down,int total,int view,int pos,int move)
{
	if (!name || !name->size || width <= 0 || height <= 0
		|| width > 4096 || height > 4096 || up < 0 || down < 0
		|| up > height || down > height-up) return false;
	struct parts *p = parts_get(number);
	parts_vscrollbar_free(p);
	p->vscrollbar = xcalloc(1,sizeof(*p->vscrollbar));
	struct parts_vscrollbar *b = p->vscrollbar;
	b->cg_name = string_dup(name);b->width=width;b->height=height;
	b->up=up;b->down=down;b->total=max(0,total);b->view=max(0,view);
	b->pos=min(max(0,pos),max_pos(b));b->move=max(0,move);
	PE_SetClickable(number,true);
	if (redraw(p)) return true;
	parts_vscrollbar_free(p);
	return false;
}
void parts_vscrollbar_free(struct parts *p)
{
	if (!p->vscrollbar) return;
	free_string(p->vscrollbar->cg_name);free(p->vscrollbar);p->vscrollbar=NULL;
}
void PE_SetVScrollbarScrollPos(int number,int pos)
{
	// Always notify: SetScrollPos is the AIN's explicit request, and Rance03's
	// backlog relies on the SCROLL message to trigger the first SetLineIndex.
	// When the whole history fits the view (total <= view), the clamped
	// position does not change, but the event must still fire.
	struct parts *p=parts_try_get(number);
	if (!p || !p->vscrollbar) return;
	vscrollbar_set_pos(p,pos,true);
}
int PE_GetVScrollbarScrollPos(int number)
{ struct parts_vscrollbar *b=get_bar(number);return b?b->pos:0; }
void PE_SetVScrollbarTotalSize(int number,int size)
{
	struct parts *p=parts_try_get(number);if(!p||!p->vscrollbar)return;
	struct parts_vscrollbar*b=p->vscrollbar;b->total=max(0,size);
	int old_pos=b->pos; b->pos=min(b->pos,max_pos(b));redraw(p);
	if (b->pos!=old_pos) parts_msg_push(p,PARTS_MSG_SCROLL,"ii",b->pos,b->total);
}
int PE_GetVScrollbarTotalSize(int number)
{struct parts_vscrollbar*b=get_bar(number);return b?b->total:0;}
void PE_SetVScrollbarViewSize(int number,int size)
{
	struct parts *p=parts_try_get(number);if(!p||!p->vscrollbar)return;
	struct parts_vscrollbar*b=p->vscrollbar;b->view=max(0,size);
	int old_pos=b->pos; b->pos=min(b->pos,max_pos(b));redraw(p);
	if (b->pos!=old_pos) parts_msg_push(p,PARTS_MSG_SCROLL,"ii",b->pos,b->total);
}
int PE_GetVScrollbarViewSize(int number)
{struct parts_vscrollbar*b=get_bar(number);return b?b->view:0;}
float PE_GetVScrollbarScrollRate(int number)
{
	struct parts_vscrollbar*b=get_bar(number);
	return b&&max_pos(b)>0 ? (float)b->pos/max_pos(b):0.0f;
}
void PE_SetVScrollbarScrollRate(int number,float rate)
{
	struct parts_vscrollbar*b=get_bar(number);if(!b||!isfinite(rate))return;
	rate=fminf(1.0f,fmaxf(0.0f,rate));
	PE_SetVScrollbarScrollPos(number,(int)roundf(rate*max_pos(b)));
}
void PE_SetVScrollbarMoveSizeByButton(int number,int size)
{struct parts_vscrollbar*b=get_bar(number);if(b)b->move=max(0,size);}
int PE_GetVScrollbarMoveSizeByButton(int number)
{struct parts_vscrollbar*b=get_bar(number);return b?b->move:0;}
void PE_SetVScrollbarSize(int number,int width,int height)
{
	struct parts*p=parts_try_get(number);if(!p||!p->vscrollbar)return;
	struct parts_vscrollbar*b=p->vscrollbar;
	if(width<=0||height<=0||width>4096||height>4096||b->up+b->down>height)return;
	b->width=width;b->height=height;redraw(p);
}
void PE_SetVScrollbarUpHeight(int number,int height)
{
	struct parts*p=parts_try_get(number);if(!p||!p->vscrollbar)return;
	struct parts_vscrollbar*b=p->vscrollbar;if(height<0||height>b->height-b->down)return;
	b->up=height;redraw(p);
}
void PE_SetVScrollbarDownHeight(int number,int height)
{
	struct parts*p=parts_try_get(number);if(!p||!p->vscrollbar)return;
	struct parts_vscrollbar*b=p->vscrollbar;if(height<0||height>b->height-b->up)return;
	b->down=height;redraw(p);
}
int PE_GetVScrollbarUpHeight(int number)
{struct parts_vscrollbar*b=get_bar(number);return b?b->up:0;}
int PE_GetVScrollbarDownHeight(int number)
{struct parts_vscrollbar*b=get_bar(number);return b?b->down:0;}
void PE_SetVScrollbarCGName(int number,struct string *name)
{
	struct parts*p=parts_try_get(number);if(!p||!p->vscrollbar||!name||!name->size)return;
	struct string*old=p->vscrollbar->cg_name;p->vscrollbar->cg_name=string_dup(name);
	if(!redraw(p)){free_string(p->vscrollbar->cg_name);p->vscrollbar->cg_name=old;redraw(p);}
	else free_string(old);
}
void PE_GetVScrollbarCGName(int number,struct string **name)
{
	struct parts_vscrollbar*b=get_bar(number);
	free_string(*name);*name=b?string_ref(b->cg_name):cstr_to_string("");
}
void parts_vscrollbar_mouse(struct parts *p,Point mouse,bool clicking,
		bool was_clicking,bool hovered,bool click_available)
{
	struct parts_vscrollbar*b=p->vscrollbar;
	if(!b)return;
	if(!clicking){b->dragging=false;return;}
	int y=mouse.y-p->global.pos.y-p->states[0].common.origin_offset.y;
	if(!was_clicking&&hovered&&click_available){
		int thumb=b->up+thumb_offset(b);
		if(y<b->up) vscrollbar_set_pos(p,b->pos-b->move,false);
		else if(y>=b->height-b->down) vscrollbar_set_pos(p,b->pos+b->move,false);
		else {
			b->dragging=true;
			if(y>=thumb&&y<thumb+thumb_height(b)) b->drag_offset=y-thumb;
			else b->drag_offset=thumb_height(b)/2;
		}
	}
	if(b->dragging){
		int travel=track_height(b)-thumb_height(b);
		if(travel>0){
			int offset=min(travel,max(0,y-b->up-b->drag_offset));
			vscrollbar_set_pos(p,(int)((int64_t)offset*max_pos(b)/travel),false);
		}
	}
}
