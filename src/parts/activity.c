/* Parts activity templates are standard EX trees, separate from PE_Save XPE. */
#include <math.h>
#include <string.h>
#include "system4.h"
#include "system4/archive.h"
#include "system4/cg.h"
#include "system4/ex.h"
#include "system4/string.h"
#include "asset_manager.h"
#include "parts.h"
#include "parts_internal.h"
#include "vm/page.h"
#include "xsystem4.h"

struct activity_part {
	struct string *name;
	int number;
	bool has_intent;
};
struct parts_activity {
	struct parts_activity *next;
	struct string *name;
	struct ex *data;
	int nr_parts;
	struct activity_part *parts;
	int nr_endkeys;
	int *endkeys;
};
static struct parts_activity *activities;

static struct parts_activity *find_activity(struct string *name)
{
	if (!name) return NULL;
	for (struct parts_activity *a = activities; a; a = a->next)
		if (!strcmp(a->name->text, name->text)) return a;
	return NULL;
}
static struct activity_part *find_part(struct parts_activity *a, struct string *name)
{
	if (!a || !name) return NULL;
	for (int i = 0; i < a->nr_parts; i++)
		if (!strcmp(a->parts[i].name->text, name->text)) return &a->parts[i];
	return NULL;
}
static struct ex_tree *child(struct ex_tree *t, const char *key)
{
	if (!t || t->is_leaf) return NULL;
	return ex_tree_get_child(t, key);
}
static struct ex_value *field(struct ex_tree *t, const char *key)
{
	struct ex_tree *n = child(t, key);
	return n && n->is_leaf ? &n->leaf.value : NULL;
}
static int integer(struct ex_tree *t, const char *key, int fallback)
{
	struct ex_value *v = field(t, key);
	return v && v->type == EX_INT ? v->i : fallback;
}
static float real(struct ex_tree *t, const char *key, float fallback)
{
	struct ex_value *v = field(t, key);
	if (!v) return fallback;
	return v->type == EX_FLOAT ? v->f : v->type == EX_INT ? v->i : fallback;
}
static struct string *text(struct ex_tree *t, const char *key)
{
	struct ex_value *v = field(t, key);
	return v && v->type == EX_STRING ? v->s : NULL;
}
static struct ex_value *item(struct ex_tree *t, const char *key, unsigned index)
{
	struct ex_value *v = field(t, key);
	return v && v->type == EX_LIST ? ex_list_get(v->list, index) : NULL;
}
static float vector(struct ex_tree *t, const char *key, unsigned index, float fallback)
{
	struct ex_value *v = item(t, key, index);
	if (!v) return fallback;
	return v->type == EX_FLOAT ? v->f : v->type == EX_INT ? v->i : fallback;
}
static void font(int number, struct ex_tree *t, int state)
{
	PE_SetFont(number, integer(t,"\xd7\xd6\xcc\xe5\xc0\xe0\xd0\xcd" /* 字体类型 */,0), integer(t,"\xd7\xd6\xcc\xe5\xb3\xdf\xb4\xe7" /* 字体尺寸 */,16),
		vector(t,"\xd7\xd6\xcc\xe5\xd1\xd5\xc9\xab" /* 字体颜色 */,0,255), vector(t,"\xd7\xd6\xcc\xe5\xd1\xd5\xc9\xab" /* 字体颜色 */,1,255),
		vector(t,"\xd7\xd6\xcc\xe5\xd1\xd5\xc9\xab" /* 字体颜色 */,2,255), real(t,"\xd7\xd6\xcc\xe5\xb4\xd6\xcf\xb8" /* 字体粗细 */,0),
		vector(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf\xc9\xab" /* 字体描边色 */,0,0), vector(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf\xc9\xab" /* 字体描边色 */,1,0),
		vector(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf\xc9\xab" /* 字体描边色 */,2,0), real(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf" /* 字体描边 */,0), state);
	PE_SetTextCharSpace(number, integer(t,"\xd7\xd6\xbc\xe4\xbe\xe0" /* 字间距 */,0), state);
	PE_SetTextLineSpace(number, integer(t,"\xd0\xd0\xbc\xe4\xbe\xe0" /* 行间距 */,0), state);
}
static bool load_state(int number, struct ex_tree *t, int state)
{
	int kind = integer(t,"\xb2\xbf\xbc\xfe\xc0\xe0\xd0\xcd" /* 部件类型 */,11);
	struct string *s;
	if (kind == 11) {
		s = text(t,"\x43\x47\xc3\xfb" /* CG名 */);
		if (!PE_SetPartsCG(number,s,integer(t,"\x89\xe4\xd0\xce" /* 変形 */,0),state)) return false;
	} else if (kind == 13) {
		PE_SetComponentType(number,13,state);
		font(number,child(t,"\xce\xc4\xb1\xbe\xd7\xb0\xef\x97" /* 文本装飾 */),state);
		s = text(t,"\xce\xc4\xb1\xbe" /* 文本 */);
		if (s && !PE_SetText(number,s,state)) return false;
	} else {
		WARNING("Unsupported activity state type: %d",kind);
		return false;
	}
	int w = vector(t,"\xa5\xb5\xa9\x60\xa5\xd5\xa5\xa7\xa5\xa4\xa5\xb9\xa5\xa8\xa5\xea\xa5\xa2" /* サーフェイスエリア */,2,0);
	int h = vector(t,"\xa5\xb5\xa9\x60\xa5\xd5\xa5\xa7\xa5\xa4\xa5\xb9\xa5\xa8\xa5\xea\xa5\xa2" /* サーフェイスエリア */,3,0);
	if (w > 0 && h > 0) {
		int x = vector(t,"\xa5\xb5\xa9\x60\xa5\xd5\xa5\xa7\xa5\xa4\xa5\xb9\xa5\xa8\xa5\xea\xa5\xa2" /* サーフェイスエリア */,0,0);
		int y = vector(t,"\xa5\xb5\xa9\x60\xa5\xd5\xa5\xa7\xa5\xa4\xa5\xb9\xa5\xa8\xa5\xea\xa5\xa2" /* サーフェイスエリア */,1,0);
		if (kind == 11) PE_SetPartsCGSurfaceArea(number,x,y,w,h,state);
		else PE_SetPartsTextSurfaceArea(number,x,y,w,h,state);
	}
	return true;
}
static bool load_button(int number, struct ex_tree *t)
{
	struct string *skin = text(t,"\x43\x47\xc3\xfb" /* CG名 */);
	struct string *label = text(t,"\xce\xc4\xb1\xbe" /* 文本 */);
	int w = vector(t,"\xb3\xdf\xb4\xe7" /* 尺寸 */,0,0), h = vector(t,"\xb3\xdf\xb4\xe7" /* 尺寸 */,1,0);
	struct cg_metrics m;
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || !skin)
		return false;
	/* Each button state is a separately named CG under the skin directory. */
	const char *suffixes[] = {"\xa3\xaf\xcd\xa8\xb3\xa3" /* ／通常 */, "\xa3\xaf\xbf\xaa" /* ／开 */, "\xa3\xaf\xbb\xf7\xb5\xb9" /* ／击倒 */};
	for (int state = 1; state <= 3; state++) {
		PE_ClearPartsConstructionProcess(number,state);
		PE_AddCreateToPartsConstructionProcess(number,w,h,state);
		struct string *state_skin = string_dup(skin);
		string_append_cstr(&state_skin,suffixes[state-1],strlen(suffixes[state-1]));
		if (!asset_cg_get_metrics_by_name(state_skin->text,&m)) {
			free_string(state_skin); return false;
		}
		PE_AddDrawCutCGToPartsConstructionProcess(number,state_skin,0,0,w,h,
				0,0,m.w,m.h,1,state);
		free_string(state_skin);
		if (label && label->size) {
			struct text_style ts = {
				.face = integer(t,"\xd7\xd6\xcc\xe5\xc0\xe0\xd0\xcd" /* 字体类型 */,0),
				.size = integer(t,"\xd7\xd6\xcc\xe5\xb3\xdf\xb4\xe7" /* 字体尺寸 */,16),
				.scale_x = 1.0f, .space_scale_x = 1.0f,
				.bold_width = real(t,"\xd7\xd6\xcc\xe5\xb4\xd6\xcf\xb8" /* 字体粗细 */,0)
			};
			int x = max(0,(w-(int)gfx_size_text(&ts,label->text))/2);
			int y = max(0,(h-(int)ts.size)/2);
			PE_AddDrawTextToPartsConstructionProcess(number,x,y,label,ts.face,ts.size,
				vector(t,"\xd7\xd6\xcc\xe5\xd1\xd5\xc9\xab" /* 字体颜色 */,0,0),vector(t,"\xd7\xd6\xcc\xe5\xd1\xd5\xc9\xab" /* 字体颜色 */,1,0),
				vector(t,"\xd7\xd6\xcc\xe5\xd1\xd5\xc9\xab" /* 字体颜色 */,2,0),ts.bold_width,
				vector(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf\xc9\xab" /* 字体描边色 */,0,0),vector(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf\xc9\xab" /* 字体描边色 */,1,0),
				vector(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf\xc9\xab" /* 字体描边色 */,2,0),real(t,"\xd7\xd6\xcc\xe5\xc3\xe8\xb1\xdf" /* 字体描边 */,0),
				integer(t,"\xce\xc4\xd7\xd6\xbc\xe4\xbe\xe0" /* 文字间距 */,0),integer(t,"\xd0\xd0\xbc\xe4\xbe\xe0" /* 行间距 */,0),state);
		}
		if (!PE_BuildPartsConstructionProcess(number,state)) return false;
	}
	PE_SetClickable(number,integer(t,"\xd3\xd0\xd0\xa7" /* 有效 */,1));
	return true;
}
static bool load_checkbox(int number, struct ex_tree *t)
{
	return PE_CreateCheckBox(number,text(t,"\x43\x47\xc3\xfb"),
			vector(t,"\xb3\xdf\xb4\xe7",0,0),vector(t,"\xb3\xdf\xb4\xe7",1,0),
			integer(t,"\x43\x68\x65\x63\x6b\xd7\xb4\xcc\xac",0),integer(t,"\xb0\xb4\xc5\xa5\xc4\xa3\xca\xbd",0),
			vector(t,"\xb0\xb4\xc5\xa5\xd1\xd5\xc9\xab",0,255),vector(t,"\xb0\xb4\xc5\xa5\xd1\xd5\xc9\xab",1,255),
			vector(t,"\xb0\xb4\xc5\xa5\xd1\xd5\xc9\xab",2,255));
}
static bool load_layout_box(int number, struct ex_tree *t)
{
	if (integer(t,"\xbb\xbb\xd0\xd0\xb3\xdf\xb4\xe7\xa4\xf2\xa5\xec\xa9\x60\xa5\xc8\xa4\xc8\xa4\xb7\xa4\xc6\xd5\x4a\xd7\x52\xa4\xb9\xa4\xeb",0))
		return false;
	PE_SetComponentType(number,8,1);
	PE_SetLayoutBoxLayoutType(number,integer(t,"\xb2\xbc\xbe\xd6\xc0\xe0\xd0\xcd",0));
	PE_SetLayoutBoxReturn(number,integer(t,"\xbb\xbb\xd0\xd0\xd0\xed\xbf\xc9",0),integer(t,"\xbb\xbb\xd0\xd0\xb3\xdf\xb4\xe7",0));
	PE_SetLayoutBoxAlign(number,integer(t,"\xd6\xc3\xb7\xc5",1));
	PE_set_layoutbox_padding(number,vector(t,"\xa5\xd1\xa5\xc7\xa5\xa3\xa5\xf3\xa5\xb0",0,0),vector(t,"\xa5\xd1\xa5\xc7\xa5\xa3\xa5\xf3\xa5\xb0",1,0),
			vector(t,"\xa5\xd1\xa5\xc7\xa5\xa3\xa5\xf3\xa5\xb0",2,0),vector(t,"\xa5\xd1\xa5\xc7\xa5\xa3\xa5\xf3\xa5\xb0",3,0));
	return true;
}
static bool load_component(int number, struct ex_tree *t)
{
	struct ex_tree *kind = child(t,"\xb7\x4e\xee\x90\x84\x65\xc7\xe9\x88\xf3" /* 種類別情報 */);
	if (!kind) return false;
	switch (integer(kind,"\xb2\xbf\xbc\xfe\xc0\xe0\xd0\xcd" /* 部件类型 */,-1)) {
	case 10:
		return load_state(number,child(kind,"\xcd\xa8\xb3\xa3\xd7\xb4\xcc\xac" /* 通常状态 */),1)
			&& load_state(number,child(kind,"\xd6\xb8\xd5\xeb\xd2\xc6\xc8\xeb\xd7\xb4\xcc\xac" /* 指针移入状态 */),2)
			&& load_state(number,child(kind,"\xb0\xb4\xbc\xfc\xb0\xb4\xcf\xc2\xd7\xb4\xcc\xac" /* 按键按下状态 */),3);
	case 0:
		return load_button(number,kind);
	case 1:
		return load_checkbox(number,kind);
	case 8:
		return load_layout_box(number,kind);
	case 3:
		return PE_CreateHScrollbar(number,text(kind,"\x43\x47\xc3\xfb" /* CG名 */),
				integer(kind,"\xb3\xa4\xb6\xc8" /* 长度 */,0),integer(kind,"\xb7\xf9" /* 幅 */,0),
				integer(kind,"\xc7\xb0\xb3\xdf\xb4\xe7" /* 前尺寸 */,0),integer(kind,"\xb4\xce\xb3\xdf\xb4\xe7" /* 次尺寸 */,0),
				integer(kind,"\xc8\xab\xcc\xe5\xa5\xb9\xa5\xaf\xa5\xed\xa9\x60\xa5\xeb\xc1\xbf" /* 全体スクロール量 */,0),integer(kind,"\xcf\xd4\xca\xbe\xc1\xbf" /* 显示量 */,0),
				integer(kind,"\xb9\xf6\xb6\xaf\xce\xbb\xd6\xc3" /* 滚动位置 */,0),integer(kind,"\xb0\xb4\xc5\xa5\xb5\xe3\xbb\xf7\xd2\xc6\xb6\xaf\xc1\xbf" /* 按钮点击移动量 */,1));
	case 2:
		return PE_CreateVScrollbar(number,text(kind,"\x43\x47\xc3\xfb" /* CG名 */),
				integer(kind,"\xb7\xf9" /* 幅 */,0),integer(kind,"\xb3\xa4\xb6\xc8" /* 长度 */,0),
				integer(kind,"\xc7\xb0\xb3\xdf\xb4\xe7" /* 前尺寸 */,0),integer(kind,"\xb4\xce\xb3\xdf\xb4\xe7" /* 次尺寸 */,0),
				integer(kind,"\xc8\xab\xcc\xe5\xa5\xb9\xa5\xaf\xa5\xed\xa9\x60\xa5\xeb\xc1\xbf" /* 全体スクロール量 */,0),integer(kind,"\xcf\xd4\xca\xbe\xc1\xbf" /* 显示量 */,0),
				integer(kind,"\xb9\xf6\xb6\xaf\xce\xbb\xd6\xc3" /* 滚动位置 */,0),integer(kind,"\xb0\xb4\xc5\xa5\xb5\xe3\xbb\xf7\xd2\xc6\xb6\xaf\xc1\xbf" /* 按钮点击移动量 */,1));
	default:
		WARNING("Unsupported activity component kind");
		return false;
	}
}
static bool load_tree(struct parts_activity *a, struct ex_tree *t, int parent)
{
	if (!t || t->is_leaf || a->nr_parts >= 4096) return false;
	for (int i = 0; i < a->nr_parts; i++)
		if (!strcmp(a->parts[i].name->text,t->name->text)) return false;
	int number = PE_GetFreeNumber();
	a->parts = xrealloc(a->parts,(a->nr_parts+1)*sizeof(*a->parts));
	a->parts[a->nr_parts++] = (struct activity_part) {
		.name = string_ref(t->name), .number = number,
		.has_intent = child(t,"\xa5\xa4\xa5\xf3\xa5\xc6\xa5\xf3\xa5\xc8\xc7\xe9\x88\xf3" /* インテント情報 */) != NULL
	};
	PE_SetPos(number,vector(t,"\xd7\xf8\xb1\xea" /* 坐标 */,0,0),vector(t,"\xd7\xf8\xb1\xea" /* 坐标 */,1,0));
	if (!load_component(number,t)) return false;
	PE_SetZ(number,vector(t,"\xd7\xf8\xb1\xea" /* 坐标 */,2,1));
	PE_SetPartsOriginPosMode(number,integer(t,"\xd4\xad\xb5\xe3\xd7\xf8\xb1\xea\xc4\xa3\xca\xbd" /* 原点坐标模式 */,1));
	PE_SetShow(number,integer(t,"\xcf\xd4\xca\xbe" /* 显示 */,1));
	PE_SetAlpha(number,integer(t,"Alpha",255));
	PE_SetAddColor(number,vector(t,"\xbc\xd3\xcb\xe3\xc9\xab" /* 加算色 */,0,0),vector(t,"\xbc\xd3\xcb\xe3\xc9\xab" /* 加算色 */,1,0),vector(t,"\xbc\xd3\xcb\xe3\xc9\xab" /* 加算色 */,2,0));
	PE_SetMultiplyColor(number,vector(t,"\x81\x5c\xcb\xe3\xc9\xab" /* 乗算色 */,0,255),vector(t,"\x81\x5c\xcb\xe3\xc9\xab" /* 乗算色 */,1,255),vector(t,"\x81\x5c\xcb\xe3\xc9\xab" /* 乗算色 */,2,255));
	parts_checkbox_apply_color(parts_get(number));
	PE_SetPartsDrawFilter(number,integer(t,"\xbb\xe6\xd6\xc6\xc2\xcb\xbe\xb5" /* 绘制滤镜 */,0));
	PE_SetPartsMagX(number,vector(t,"\x92\x88\xb4\xf3\xbf\x73\xd0\xa1" /* 拡大縮小 */,0,1));
	PE_SetPartsMagY(number,vector(t,"\x92\x88\xb4\xf3\xbf\x73\xd0\xa1" /* 拡大縮小 */,1,1));
	if (vector(t,"\xd0\xfd\xd7\xaa" /* 旋转 */,0,0))
		PE_SetPartsRotateX(number,vector(t,"\xd0\xfd\xd7\xaa" /* 旋转 */,0,0));
	if (vector(t,"\xd0\xfd\xd7\xaa" /* 旋转 */,1,0))
		PE_SetPartsRotateY(number,vector(t,"\xd0\xfd\xd7\xaa" /* 旋转 */,1,0));
	PE_SetPartsRotateZ(number,vector(t,"\xd0\xfd\xd7\xaa" /* 旋转 */,2,0));
	PE_SetComponentMargin(number,vector(t,"\xcd\xe2\xb1\xdf\xbe\xe0" /* 外边距 */,0,0),vector(t,"\xcd\xe2\xb1\xdf\xbe\xe0" /* 外边距 */,1,0),vector(t,"\xcd\xe2\xb1\xdf\xbe\xe0" /* 外边距 */,2,0),vector(t,"\xcd\xe2\xb1\xdf\xbe\xe0" /* 外边距 */,3,0));
	PE_SetPartsPixelDecide(number,integer(t,"\xca\xf3\xb1\xea\xd6\xb8\xd5\xeb\xcf\xf1\xcb\xd8\xc5\xd0\xb6\xa8" /* 鼠标指针像素判定 */,0));
	PE_SetPassCursor(number,integer(t,"\xd6\xb8\xd5\xeb\xd2\xc6\xc8\xeb\xcd\xb8\xc3\xf7" /* 指针移入透明 */,0));
	if (integer(t,"\xa5\xaf\xa5\xea\xa5\xc3\xa5\xaf\xd0\xed\xbf\xc9" /* クリック许可 */,0)) PE_SetClickable(number,true);
	PE_SetDrag(number,integer(t,"\xcd\xcf\xd7\xa7" /* 拖拽 */,0));
	PE_SetPartsMessageWindowShowLink(number,integer(t,"\xb6\xd4\xb0\xd7\xb4\xb0\xbf\xda\xcf\xd4\xca\xbe\xc1\xaa\xb6\xaf" /* 对白窗口显示联动 */,0));
	if (parent >= 0) PE_SetParentPartsNumber(number,parent);
	struct ex_tree *children = child(t,"\xd7\xd3\xb2\xbf\xbc\xfe" /* 子部件 */);
	if (children && !children->is_leaf)
		for (unsigned i = 0; i < children->nr_children; i++)
			if (!load_tree(a,&children->children[i],number)) return false;
	return true;
}
static void free_activity(struct parts_activity *a)
{
	for (int i = 0; i < a->nr_parts; i++) free_string(a->parts[i].name);
	free(a->parts); free(a->endkeys); free_string(a->name);
	ex_free(a->data); free(a);
}
static bool load_ex(struct string *name, struct ex *ex)
{
	struct ex_tree *tree = ex_get_tree(ex,"\xbb\xee\xb6\xaf" /* 活动 */);
	struct ex_list *keys = ex_get_list(ex,"\xbd\xe1\xca\xf8\xa5\xad\xa9\x60" /* 结束キー */);
	if (!name || !name->size || find_activity(name) || !tree || tree->is_leaf || !tree->nr_children || !keys || keys->nr_items > 256) {
		ex_free(ex); return false;
	}
	struct parts_activity *a = xcalloc(1,sizeof(*a));
	a->name = string_dup(name); a->data = ex;
	a->nr_endkeys = keys->nr_items;
	a->endkeys = xcalloc(keys->nr_items,sizeof(int));
	for (unsigned i = 0; i < keys->nr_items; i++) {
		struct ex_value *key = ex_list_get(keys,i);
		if (!key || key->type != EX_INT || key->i < 0 || key->i > 255) goto error;
		a->endkeys[i] = key->i;
	}
	for (unsigned i = 0; i < tree->nr_children; i++)
		if (!load_tree(a,&tree->children[i],-1)) goto error;
	PE_UpdateComponent(0);
	a->next = activities; activities = a;
	return true;
error:
	for (int i = a->nr_parts-1; i >= 0; i--) PE_ReleaseParts(a->parts[i].number);
	free_activity(a);
	return false;
}
bool PE_ReadActivityFile(struct string *name, struct string *filename)
{
	if (!name || !name->size || find_activity(name) || !filename) return false;
	struct archive_data *d = asset_get_by_name(ASSET_PACT,filename->text,NULL);
	if (!d) return false;
	struct ex *ex = ex_read(d->data,d->size);
	archive_free_data(d);
	return ex && load_ex(name,ex);
}
bool PE_IsExistActivity(struct string *name) { return find_activity(name) != NULL; }
int PE_NumofActivityParts(struct string *name) {
	struct parts_activity *a = find_activity(name); return a ? a->nr_parts : 0;
}
bool PE_GetActivityParts(int index, struct string *name, struct string **part_name, int *number)
{
	struct parts_activity *a = find_activity(name);
	if (!a || index < 0 || index >= a->nr_parts || !part_name || !number) return false;
	free_string(*part_name); *part_name = string_ref(a->parts[index].name);
	*number = a->parts[index].number; return true;
}
bool PE_IsExistActivityPartsByName(struct string *name, struct string *part)
{ return find_part(find_activity(name),part) != NULL; }
int PE_GetActivityPartsNumber(struct string *name, struct string *part) {
	struct activity_part *p = find_part(find_activity(name),part); return p ? p->number : -1;
}
bool PE_IsExistActivityIntentData(struct string *name, struct string *part) {
	struct activity_part *p = find_part(find_activity(name),part); return p && p->has_intent;
}
int PE_NumofActivityEndKey(struct string *name) {
	struct parts_activity *a = find_activity(name); return a ? a->nr_endkeys : 0;
}
int PE_GetActivityEndKey(struct string *name, int index) {
	struct parts_activity *a = find_activity(name);
	return a && index >= 0 && index < a->nr_endkeys ? a->endkeys[index] : 0;
}
static bool owns_part(struct parts_activity *a, struct parts *p)
{
	for (struct parts *ancestor = p; ancestor; ancestor = ancestor->parent)
		for (int i = 0; i < a->nr_parts; i++)
			if (ancestor->no == a->parts[i].number) return true;
	return false;
}
bool PE_ReleaseActivity(struct string *name, struct page **erased)
{
	struct parts_activity **link = &activities;
	while (*link && (!name || strcmp((*link)->name->text,name->text))) link = &(*link)->next;
	if (!*link) return false;
	struct parts_activity *a = *link;
	/* Resolve pending links before collecting dynamic children. */
	PE_UpdateComponent(0);
	int *numbers = NULL, count = 0;
	struct parts *p;
	PARTS_LIST_FOREACH(p) if (owns_part(a,p)) {
		numbers = xrealloc(numbers,(count+1)*sizeof(int)); numbers[count++] = p->no;
	}
	for (int i = count-1; i >= 0; i--) {
		if (erased) *erased = array_pushback(*erased,(union vm_value){.i=numbers[i]},AIN_ARRAY_INT,-1);
		PE_ReleaseParts(numbers[i]);
	}
	free(numbers); *link = a->next; free_activity(a); return true;
}
void PE_ActivityReset(void)
{
	while (activities) {
		struct parts_activity *a = activities;
		activities = a->next; free_activity(a);
	}
}
