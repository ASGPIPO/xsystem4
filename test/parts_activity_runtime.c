/* Real AIN/PACTEX activity lifecycle regression; never prints asset text. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "system4/ain.h"
#include "system4/afa.h"
#include "system4/string.h"
#include "system4/utfsjis.h"
#include "asset_manager.h"
#include "parts.h"
#include "vm/page.h"
#include "xsystem4.h"
#include "../src/parts/parts_internal.h"
#include "../src/hll/hll.h"
extern struct static_library lib_PartsEngine;
static void *bound(const char *name) {
 int l=ain_get_library(ain,"PartsEngine");
 assert(l>=0&&ain_get_library_function(ain,l,name)>=0);
 for(int i=0;lib_PartsEngine.functions[i].name;i++)
  if(!strcmp(lib_PartsEngine.functions[i].name,name))return lib_PartsEngine.functions[i].fun;
 return NULL;
}
int main(void) {
 setbuf(stdout,NULL); sjis_set_encoding(SJIS_ENCODING_CP936);
 int err; ain=ain_open("/root/rance03/Rance03.ain",&err); assert(ain);
 const char *required[]={"IsExistActivity","ReadActivityFile","NumofActivityParts","GetActivityParts","IsExistActivityPartsByName","GetActivityPartsNumber","NumofActivityEndKey","GetActivityEndKey","IsExistActivityIntentData","GetComponentAbsoluteMaxPosZ","GetComponentAbsolutePosZ","GetComponentAbsolutePosX","GetComponentAbsolutePosY","SetFocus","IsFocus","AddChild"};
 for(unsigned i=0;i<sizeof(required)/sizeof(required[0]);i++) if(!bound(required[i])) {
  fprintf(stderr,"FAIL real AIN requires unbound PartsEngine.%s\n",required[i]); return 1;
 }
 config.game_dir="/root/rance03"; config.ain_filename="Rance03.ain";
 config.game_name="Activity private regression"; config.view_width=1024;config.view_height=768;
 config.font_paths[0]="/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";
 config.font_paths[1]=config.font_paths[0];
 asset_manager_init();
 for(int i=0;lib_PartsEngine.functions[i].name;i++)
  if(!strcmp(lib_PartsEngine.functions[i].name,"_PreLink"))((void(*)(void))lib_PartsEngine.functions[i].fun)();
 PE_Init();
 printf("REAL_AIN_MULTI_CONTROLLER:%d\n",parts_multi_controller);
 struct afa_archive *pact=afa_open("/root/rance03/Rance03Pact.afa",0,&err);assert(pact&&pact->nr_files);
 struct string *file=pact->files[0].name,*name=cstr_to_string("codex-real-backlog");
 bool (*readfile)(struct string*,struct string*)=bound("ReadActivityFile");
 bool (*exists)(struct string*)=bound("IsExistActivity");
 int (*count)(struct string*)=bound("NumofActivityParts");
 int (*keys)(struct string*)=bound("NumofActivityEndKey");
 int (*key)(struct string*,int)=bound("GetActivityEndKey");
 bool (*part)(int,struct string*,struct string**,int*)=bound("GetActivityParts");
 int (*number)(struct string*,struct string*)=bound("GetActivityPartsNumber");
 assert(!exists(name));assert(readfile(name,file));assert(exists(name));
 int outside=PE_GetFreeNumber();PE_SetPos(outside,0,0);
 assert(!readfile(name,file));assert(count(name)==7);assert(keys(name)==3);
 assert(key(name,0)==2&&key(name,1)==27&&key(name,2)==32);assert(key(name,-1)==0&&key(name,3)==0);
 int ids[7];struct string *s=cstr_to_string("");
 for(int i=0;i<7;i++){assert(part(i,name,&s,&ids[i]));assert(PE_IsExist(ids[i]));assert(number(name,s)==ids[i]);}
 assert(!part(-1,name,&s,&ids[0]));assert(!part(7,name,&s,&ids[0]));
 assert(PE_GetPartsOriginPosMode(ids[3])==3);
 assert(PE_GetPartsOriginPosMode(ids[4])==3);
 assert(PE_GetPartsX(ids[3])==1009&&PE_GetPartsY(ids[3])==110);
 int (*absmax)(int)=bound("GetComponentAbsoluteMaxPosZ");
 int (*absz)(int)=bound("GetComponentAbsolutePosZ");
 float (*absx)(int)=bound("GetComponentAbsolutePosX");
 float (*absy)(int)=bound("GetComponentAbsolutePosY");
 PE_SetZ(outside,1000);PE_SetZ(ids[0],10);PE_SetPos(ids[0],11,12);
 assert(absz(ids[0])==10&&absz(ids[3])==13&&absmax(ids[0])==16);
 assert(absmax(ids[3])==13&&absmax(-99)==0&&absz(-99)==0);
 assert(absx(ids[3])==1020.0f&&absy(ids[3])==122.0f);
 int negative=PE_GetFreeNumber();PE_SetZ(negative,-20);
 assert(absmax(negative)==-20);PE_ReleaseParts(negative);
 int type=-1,size=-1,r=-1,g=-1,b=-1,er=-1,eg=-1,eb=-1;float bold=-1,edge=-1;
 void (*getfont)(int,int*,int*,int*,int*,int*,float*,int*,int*,int*,float*,int)=bound("GetPartsTextFontProperty");
 int (*charspace)(int,int)=bound("Parts_GetTextCharSpace");
 int (*linespace)(int,int)=bound("Parts_GetTextLineSpace");
 assert(getfont&&charspace&&linespace);
 getfont(ids[5],&type,&size,&r,&g,&b,&bold,&er,&eg,&eb,&edge,1);
 assert(type==256&&size==22&&r==255&&g==255&&b==255&&bold==0&&edge==0);
 assert(charspace(ids[5],1)==0&&linespace(ids[5],1)==2);
 PE_SetTextCharSpace(ids[5],-2,1);PE_SetTextLineSpace(ids[5],5,1);
 assert(charspace(ids[5],1)==-2&&linespace(ids[5],1)==5);
 PE_SetFont(ids[5],0,17,10,20,30,0.3f,1,2,3,0.8f,1);
 getfont(ids[5],&type,&size,&r,&g,&b,&bold,&er,&eg,&eb,&edge,1);
 assert(type==0&&size==17&&r==10&&g==20&&b==30&&bold==0.3f&&er==1&&eg==2&&eb==3&&edge==0.8f);
 PE_SetPos(ids[0],0,0);PE_SetZ(ids[0],1);

 int (*total)(int)=bound("GetVScrollbarTotalSize");int (*view)(int)=bound("GetVScrollbarViewSize");
 int (*pos)(int)=bound("GetVScrollbarScrollPos");float (*rate)(int)=bound("GetVScrollbarScrollRate");
 void (*settotal)(int,int)=bound("SetVScrollbarTotalSize");void (*setview)(int,int)=bound("SetVScrollbarViewSize");
 void (*setpos)(int,int)=bound("SetVScrollbarScrollPos");void (*setrate)(int,float)=bound("SetVScrollbarScrollRate");
 assert(total&&view&&pos&&rate&&settotal&&setview&&setpos&&setrate);
 assert(total(ids[3])==528&&view(ids[3])==22&&pos(ids[3])==0);
 PE_SetDelegateIndex(ids[3],42);PE_ReleaseMessage();
 settotal(ids[3],7);setview(ids[3],22);setpos(ids[3],7);
 assert(pos(ids[3])==7&&rate(ids[3])==1.0f);
 assert(PE_GetMessageType()==20&&PE_GetMessageVariableInt(0)==7&&PE_GetMessageVariableInt(1)==7);
 PE_ReleaseMessage();
 void (*setfocus)(int)=bound("SetFocus");bool (*isfocus)(int)=bound("IsFocus");
 setfocus(ids[3]);assert(isfocus(ids[3])&&!isfocus(ids[4]));
 assert(PE_GetMessageType()==18&&PE_GetMessageVariableCount()==0);PE_ReleaseMessage();
 setfocus(ids[3]);assert(PE_GetMessageType()==0);
 PE_SetDelegateIndex(ids[4],43);setfocus(ids[4]);
 assert(!isfocus(ids[3])&&isfocus(ids[4]));
 assert(PE_GetMessageType()==19&&PE_GetMessagePartsNumber()==ids[3]&&PE_GetMessageVariableCount()==0);PE_PopMessage();
 assert(PE_GetMessageType()==18&&PE_GetMessagePartsNumber()==ids[4]&&PE_GetMessageVariableCount()==0);PE_ReleaseMessage();

 settotal(ids[3],100);setview(ids[3],20);setpos(ids[3],999);assert(pos(ids[3])==100);
 assert(PE_GetMessageType()==20);assert(PE_GetMessagePartsNumber()==ids[3]);assert(PE_GetMessageDelegateIndex()==42);
 assert(PE_GetMessageVariableCount()==2&&PE_GetMessageVariableType(0)==1&&PE_GetMessageVariableType(1)==1);
 assert(PE_GetMessageVariableInt(0)==100&&PE_GetMessageVariableInt(1)==100);PE_ReleaseMessage();
 setpos(ids[3],-5);assert(pos(ids[3])==0);setrate(ids[3],0.5f);assert(pos(ids[3])==50&&rate(ids[3])==0.5f);
 struct parts *bar=parts_try_get(ids[3]);assert(bar&&bar->vscrollbar);
 parts_vscrollbar_mouse(bar,(Point){986,375},true,false,true,true);
 parts_vscrollbar_mouse(bar,(Point){986,200},true,true,true,true);assert(pos(ids[3])==0);
 parts_vscrollbar_mouse(bar,(Point){986,1000},true,true,false,true);assert(pos(ids[3])==100);
 parts_vscrollbar_mouse(bar,(Point){986,1000},false,true,false,true);assert(!bar->vscrollbar->dragging);
 setview(ids[3],1000);assert(pos(ids[3])==100);assert(rate(ids[3])==1.0f);
 settotal(ids[3],7);setview(ids[3],22);setpos(ids[3],7);PE_ReleaseMessage();
 {int track=bar->vscrollbar->height-bar->vscrollbar->up-bar->vscrollbar->down;
  int thumb=(int)((long long)track*22/(7+22));if(thumb<16)thumb=16;if(thumb>track)thumb=track;
  int travel=track-thumb;assert(travel>0);
  int oy=bar->global.pos.y+bar->states[0].common.origin_offset.y;
  parts_vscrollbar_mouse(bar,(Point){986,oy+bar->vscrollbar->up+travel+thumb/2},true,false,true,true);
  assert(bar->vscrollbar->dragging);
  parts_vscrollbar_mouse(bar,(Point){986,oy},true,true,true,true);assert(pos(ids[3])==0&&rate(ids[3])==0.0f);
  assert(PE_GetMessageType()==20&&PE_GetMessagePartsNumber()==ids[3]&&PE_GetMessageVariableCount()==2&&PE_GetMessageVariableInt(0)==0&&PE_GetMessageVariableInt(1)==7);PE_ReleaseMessage();
  parts_vscrollbar_mouse(bar,(Point){986,oy+track},true,true,false,true);assert(pos(ids[3])==7&&rate(ids[3])==1.0f);
  assert(PE_GetMessageType()==20&&PE_GetMessageVariableInt(0)==7&&PE_GetMessageVariableInt(1)==7);PE_ReleaseMessage();
  parts_vscrollbar_mouse(bar,(Point){986,0},false,true,false,true);assert(!bar->vscrollbar->dragging);}
 {void (*addchild)(int,int)=bound("AddChild");
  int box=PE_GetFreeNumber();PE_SetPos(box,50,60);PE_SetComponentType(box,8,1);
  int l1=PE_GetFreeNumber();PE_SetPos(l1,1,2);
  int l2=PE_GetFreeNumber();PE_SetPos(l2,3,4);
  addchild(box,l1);addchild(box,l2);PE_UpdateComponent(0);
  struct parts *bp=parts_try_get(box);assert(bp&&bp->states[0].type==PARTS_LAYOUT_BOX);
  struct parts *it;int order[2],n=0;
  PARTS_FOREACH_CHILD(it,bp){if(n<2)order[n]=it->no;n++;}
  assert(n==2&&order[0]==l1&&order[1]==l2);
  assert(PE_GetParentPartsNumber(l1)==box&&PE_GetParentPartsNumber(l2)==box);
  assert(absx(l1)==50.0f&&absy(l1)==60.0f);
  PE_ReleaseParts(l1);PE_ReleaseParts(l2);PE_ReleaseParts(box);}
 int dynamic=PE_GetFreeNumber();PE_SetPos(dynamic,1,2);PE_SetParentPartsNumber(dynamic,ids[0]);PE_UpdateComponent(0);
 struct page *erased=NULL;
 bool (*release)(struct string*,struct page**)=bound("ReleaseActivity");assert(release);
 assert(release(name,&erased));assert(!exists(name));assert(!PE_IsExist(dynamic));assert(PE_IsExist(outside));
 for(int i=0;i<7;i++)assert(!PE_IsExist(ids[i]));
 assert(erased&&erased->nr_vars==8);delete_page_vars(erased);free_page(erased);
 assert(!release(name,&erased));
 assert(readfile(name,file));assert(count(name)==7);parts_release_all();assert(!exists(name));
 free_string(s);free_string(name);
 printf("PASS real PACTEX 7 parts, exact endkeys, names/coordinates/origins, Scroll20 two-int delegate ABI, focus state switch, short 7/22 drag to both ends, AddChild ordered parenting, scrollbar bounds/rate/drag, scoped dynamic descendant release, bottom-level reset\n");
 return 0;
}
