/* Real AIN/PACTEX button callback regression. Prints only numeric UI evidence. */
#include <assert.h>
#include <ffi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "parts.h"
#include "scene.h"
#include "xsystem4.h"
#include "../src/parts/parts_internal.h"
#include "../src/hll/hll.h"

extern struct static_library lib_PartsEngine;
static void *bound(const char *name)
{
 for (int i=0; lib_PartsEngine.functions[i].name; i++)
  if (!strcmp(name,lib_PartsEngine.functions[i].name))
   return lib_PartsEngine.functions[i].fun;
 return NULL;
}
static void invoke(const char *name, void **args)
{
 int l=ain_get_library(ain,"PartsEngine"), f=ain_get_library_function(ain,l,name);
 struct ain_hll_function *h=&ain->libraries[l].functions[f];
 assert(h->return_type.data==AIN_VOID && h->nr_arguments<=3);
 ffi_type *types[3];
 for (int i=0;i<h->nr_arguments;i++) {
  assert(h->arguments[i].type.data==AIN_INT || h->arguments[i].type.data==AIN_STRING);
  types[i]=h->arguments[i].type.data==AIN_INT ? &ffi_type_sint32 : &ffi_type_pointer;
 }
 ffi_cif cif;
 assert(ffi_prep_cif(&cif,FFI_DEFAULT_ABI,h->nr_arguments,&ffi_type_void,types)==FFI_OK);
 ffi_call(&cif,FFI_FN(bound(name)),NULL,args);
}
static void size(int no,int w,int h)
{
 void *args[]={&no,&w,&h}; invoke("SetButtonSize",args);
}
static void label(int no,struct string *s)
{
 void *args[]={&no,&s}; invoke("SetButtonText",args);
}
static uint64_t pixels(struct parts *p,int state)
{
 struct parts_common *c=&p->states[state].common;
 unsigned char *data=gfx_get_pixels(&c->texture); assert(data);
 uint64_t hash=1469598103934665603ULL;
 for (int i=0;i<c->w*c->h*4;i++) hash=(hash^data[i])*1099511628211ULL;
 free(data); return hash;
}
static void geometry(struct parts *p,int w,int h)
{
 for(int s=0;s<3;s++) {
  struct parts_common *c=&p->states[s].common;
  assert(p->states[s].type==PARTS_CONSTRUCTION_PROCESS);
  assert(c->w==w && c->h==h && c->texture.w==w && c->texture.h==h && c->texture.handle);
  assert(c->origin_offset.x==-w/2 && c->origin_offset.y==-h/2);
  assert(c->hitbox.x==37-w/2 && c->hitbox.y==53-h/2);
  assert(c->hitbox.w==w && c->hitbox.h==h);
  assert(PE_GetPartsWidth(p->no,s+1)==w && PE_GetPartsHeight(p->no,s+1)==h);
 }
 parts_render_update(); assert(scene_is_dirty);
}
int main(int argc,char **argv)
{
 setbuf(stdout,NULL); sjis_set_encoding(SJIS_ENCODING_CP936);
 int err; ain=ain_open("/root/rance03/Rance03.ain",&err); assert(ain);
 bool text=argc>1&&!strcmp(argv[1],"text");
 const char *required=text?"SetButtonText":"SetButtonSize";
 int l=ain_get_library(ain,"PartsEngine"), f=ain_get_library_function(ain,l,required);
 assert(l==21 && f==(text?206:193));
 struct ain_hll_function *hf=&ain->libraries[l].functions[f];
 assert(hf->return_type.data==AIN_VOID && hf->nr_arguments==(text?2:3));
 assert(hf->arguments[0].type.data==AIN_INT && hf->arguments[1].type.data==(text?AIN_STRING:AIN_INT));
 if(!text) assert(hf->arguments[2].type.data==AIN_INT);
 if(!bound(required)) {
  fprintf(stderr,"FAIL real AIN lib=%d function=%d void %s callback=NULL\n",l,f,required);
  return 1;
 }
 if(text) assert(bound("SetButtonSize"));
 config.game_dir="/root/rance03"; config.ain_filename="Rance03.ain";
 config.game_name="Private numeric button regression"; config.view_width=1024;config.view_height=768;
 config.font_paths[0]="/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";
 config.font_paths[1]=config.font_paths[0];
 asset_manager_init(); ((void(*)(void))bound("_PreLink"))(); PE_Init();
 struct afa_archive *pact=afa_open("/root/rance03/Rance03Pact.afa",0,&err); assert(pact&&pact->nr_files);
 struct string *name=cstr_to_string("codex-button-regression");
 bool (*readfile)(struct string*,struct string*)=bound("ReadActivityFile");
 bool (*part)(int,struct string*,struct string**,int*)=bound("GetActivityParts");
 assert(readfile(name,pact->files[0].name));
 struct parts *loaded=NULL; struct string *s=cstr_to_string(""); int no;
 for(int i=0;i<7;i++) {
  assert(part(i,name,&s,&no)); struct parts *p=parts_try_get(no); assert(p);
  if(!p->vscrollbar && p->states[0].type==PARTS_CONSTRUCTION_PROCESS
   && p->states[1].type==PARTS_CONSTRUCTION_PROCESS && p->states[2].type==PARTS_CONSTRUCTION_PROCESS)
   loaded=p;
 }
 assert(loaded);
 struct string *skins[3]={0};
 for(int i=0;i<3;i++) {
  struct parts_cp_op *op; TAILQ_FOREACH(op,&loaded->states[i].cproc.ops,entry)
   if(op->type==PARTS_CP_DRAW_CUT_CG) {skins[i]=string_dup(op->cut_cg.cg_name);break;}
  assert(skins[i]);
 }
 PE_SetPos(loaded->no,37,53); PE_SetPartsOriginPosMode(loaded->no,5);
 parts_engine_clean(); scene_is_dirty=false;
 PE_SetClickable(loaded->no,false);
 size(loaded->no,150,35); geometry(loaded,150,35);
 assert(!loaded->clickable);
 PE_SetClickable(loaded->no,true);
 for(int i=0;i<3;i++) {
  struct parts_cp_op *op; bool found=false;
  TAILQ_FOREACH(op,&loaded->states[i].cproc.ops,entry)
   if(op->type==PARTS_CP_DRAW_CUT_CG) {
    assert(!strcmp(skins[i]->text,op->cut_cg.cg_name->text));found=true;break;
   }
  assert(found);free_string(skins[i]);
 }
 int fresh=PE_GetFreeNumber(); assert(!PE_IsExist(fresh));
 size(fresh,150,35); assert(PE_IsExist(fresh));
 struct parts *p=parts_try_get(fresh);assert(p&&p->clickable);
 PE_SetPos(fresh,37,53);PE_SetPartsOriginPosMode(fresh,5);
 parts_engine_clean();scene_is_dirty=false;size(fresh,180,40);geometry(p,180,40);
 uint64_t before[3];for(int i=0;i<3;i++)before[i]=pixels(p,i);
 size(fresh,0,40);size(fresh,-1,40);size(fresh,180,4097);
 for(int i=0;i<3;i++)assert(pixels(p,i)==before[i]);
 int invalid=PE_GetFreeNumber();size(invalid,0,20);assert(!PE_IsExist(invalid));
 if(text) {
  struct string *a=cstr_to_string("OK"),*b=cstr_to_string("Cancel");
  parts_engine_clean();scene_is_dirty=false;label(fresh,a);geometry(p,180,40);
  for(int i=0;i<3;i++)assert(pixels(p,i)!=before[i]);
  label(fresh,b);uint64_t replaced[3];for(int i=0;i<3;i++)replaced[i]=pixels(p,i);
  label(fresh,b);label(fresh,NULL);
  for(int i=0;i<3;i++)assert(pixels(p,i)==replaced[i]);
  size(fresh,220,50);geometry(p,220,50);
  label(loaded->no,b);geometry(loaded,150,35);
  free_string(a);free_string(b);
 }
 parts_release_all();free_string(s);free_string(name);
 printf("PASS real AIN/libffi %s: PACTEX three-state skin retained, resized textures/origins/hitboxes/dirty, dynamic 150x35 button, invalid sizes%s\n",
  required,text?", real label pixels and replacement":"");
 return 0;
}
