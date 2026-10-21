/* Scrollbar skin alpha and source-over composition regression. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/cg.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "parts.h"
#include "xsystem4.h"
#include "vm.h"
#include "vm/heap.h"
#include "vm/page.h"
#include "../src/parts/parts_internal.h"

static bool synthetic;
bool __real_asset_exists_by_name(enum asset_type, const char *, int *);
struct cg *__real_asset_cg_load_by_name(const char *, int *);
bool __real_asset_cg_get_metrics_by_name(const char *, struct cg_metrics *);
bool __wrap_asset_exists_by_name(enum asset_type type, const char *name, int *id)
{
 return synthetic ? true : __real_asset_exists_by_name(type,name,id);
}
bool __wrap_asset_cg_get_metrics_by_name(const char *name, struct cg_metrics *m)
{
 if (!synthetic) return __real_asset_cg_get_metrics_by_name(name,m);
 memset(m,0,sizeof(*m)); m->w=2;m->h=2; return true;
}
struct cg *__wrap_asset_cg_load_by_name(const char *name, int *id)
{
 if (!synthetic) return __real_asset_cg_load_by_name(name,id);
 struct cg *cg=calloc(1,sizeof(*cg));
 cg->metrics.w=2;cg->metrics.h=2;cg->metrics.bpp=32;
 cg->pixels=malloc(16);
 /* Background red/128; overlay blue/128, with one transparent texel. */
 bool background=strlen(name)==5+6;
 for (int i=0;i<4;i++) {
  ((unsigned char *)cg->pixels)[i*4]=background?200:0;
  ((unsigned char *)cg->pixels)[i*4+1]=background?40:80;
  ((unsigned char *)cg->pixels)[i*4+2]=background?20:240;
  ((unsigned char *)cg->pixels)[i*4+3]=!background && i==0?0:128;
 }
 return cg;
}
static void check_lifecycle(int no)
{
	unsigned char *expected[3];
	int size[3];
	for (int s = 0; s < 3; s++) {
		Texture *t = &parts_try_get(no)->states[s].common.texture;
		size[s] = t->w*t->h*4;
		expected[s] = gfx_get_pixels(t);
		assert(PE_BuildPartsConstructionProcess(no,s+1));
		unsigned char *rebuilt = gfx_get_pixels(t);
		assert(!memcmp(expected[s],rebuilt,size[s]));
		free(rebuilt);
	}
	struct page *saved = NULL;
	assert(PE_Save(&saved));
	PE_Reset();
	assert(PE_Load(&saved));
	for (int s = 0; s < 3; s++) {
		struct parts_state *state = &parts_try_get(no)->states[s];
		assert(state->type == PARTS_CONSTRUCTION_PROCESS);
		int skins = 0;
		struct parts_cp_op *op;
		TAILQ_FOREACH(op,&state->cproc.ops,entry)
			skins += op->type == PARTS_CP_SKIN_CUT_CG;
		assert(skins == 4);
		Texture *t = &state->common.texture;
		assert(t->w*t->h*4 == size[s]);
		unsigned char *loaded = gfx_get_pixels(t);
		assert(!memcmp(expected[s],loaded,size[s]));
		free(loaded);free(expected[s]);
	}
	delete_page_vars(saved);free_page(saved);
	puts("PASS rebuild and XPE Save/Reset/Load: 4 skin ops/state and identical RGBA");
}
static int real_skin(void)
{
 int error;
 struct afa_archive *pact=afa_open("/root/rance03/Rance03Pact.afa",0,&error);
 assert(pact && pact->nr_files);
 struct string *activity=cstr_to_string("backlog-skin-alpha");
 assert(PE_ReadActivityFile(activity,pact->files[0].name));
 struct string *name=cstr_to_string("");
 int no;assert(PE_GetActivityParts(3,activity,&name,&no));
 struct parts *p=parts_try_get(no);assert(p && p->vscrollbar);
 PE_SetVScrollbarTotalSize(no,100);PE_SetVScrollbarViewSize(no,10);
 PE_SetVScrollbarScrollPos(no,0);
 struct string *bg=string_dup(p->vscrollbar->cg_name);
 string_append_cstr(&bg,"\xa3\xaf\xb1\xb3\xbe\xb0",6);
 struct cg *cg=asset_cg_load_by_name(bg->text,NULL);assert(cg);
 int i=(cg->metrics.h/2*cg->metrics.w+cg->metrics.w/2)*4;
 printf("source background RGBA=%u/%u/%u/%u\n",((unsigned char *)cg->pixels)[i],((unsigned char *)cg->pixels)[i+1],((unsigned char *)cg->pixels)[i+2],((unsigned char *)cg->pixels)[i+3]);
 assert(((unsigned char *)cg->pixels)[i]==0 && ((unsigned char *)cg->pixels)[i+1]==0 && ((unsigned char *)cg->pixels)[i+2]==0 && ((unsigned char *)cg->pixels)[i+3]==128);
 cg_free(cg);
 int failures=0;
 for(int s=0;s<3;s++) {
  Texture *t=&p->states[s].common.texture;
  int x=t->w/2,y=t->h/2;
  SDL_Color c=gfx_get_pixel(t,x,y);
  Texture target={0};gfx_init_texture_rgba(&target,t->w,t->h,(SDL_Color){200,100,50,255});
  gfx_copy_stretch_blend_amap(&target,0,0,t->w,t->h,t,0,0,t->w,t->h);
  SDL_Color scene=gfx_get_pixel(&target,x,y);gfx_delete_texture(&target);
  printf("real state=%d stored=%u/%u/%u/%u scene=%u/%u/%u/%u expected=100/50/25/255\n",s+1,c.r,c.g,c.b,c.a,scene.r,scene.g,scene.b,scene.a);
  failures+=c.a!=128 || c.r!=0 || c.g!=0 || c.b!=0 || abs(scene.r-100)>1 || abs(scene.g-50)>1 || abs(scene.b-25)>1;
 }
 check_lifecycle(no);
 free_string(bg);free_string(name);free_string(activity);
 return failures?1:0;
}
static int overlap(void)
{
 synthetic=true;
 int no=PE_GetFreeNumber();struct string *name=cstr_to_string("test/");
 assert(PE_CreateVScrollbar(no,name,2,6,2,2,100,10,0,1));
 struct parts *p=parts_try_get(no);
 int failures=0;
 for(int s=0;s<3;s++) {
  Texture *t=&p->states[s].common.texture;
  SDL_Color transparent=gfx_get_pixel(t,0,0), mixed=gfx_get_pixel(t,1,0);
  printf("overlap state=%d transparent=%u/%u/%u/%u mixed=%u/%u/%u/%u expected=66/67/167/192\n",
    s+1,transparent.r,transparent.g,transparent.b,transparent.a,mixed.r,mixed.g,mixed.b,mixed.a);
  failures+=transparent.r!=200 || transparent.g!=40 || transparent.b!=20 || transparent.a!=128;
  failures+=abs(mixed.r-66)>1 || abs(mixed.g-67)>1 || abs(mixed.b-167)>1 || abs(mixed.a-192)>1;
 }
 check_lifecycle(no);
 assert(glGetError()==GL_NO_ERROR);free_string(name);
 return failures?1:0;
}
int main(int argc,char **argv)
{
 assert(argc==2);setbuf(stdout,NULL);sjis_set_encoding(SJIS_ENCODING_CP936);
 int error;ain=ain_open("/root/rance03/Rance03.ain",&error);assert(ain);
 config.game_dir="/root/rance03";config.ain_filename="Rance03.ain";config.game_name="Scrollbar alpha regression";
 config.view_width=320;config.view_height=240;
 config.font_paths[0]="/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";config.font_paths[1]=config.font_paths[0];
 heap_init();
 asset_manager_init();PE_enable_multi_controller();assert(PE_Init());
 return !strcmp(argv[1],"real")?real_skin():overlap();
}
