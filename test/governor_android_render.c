/* Android render/input integration: synthetic scene, no game VM or assets.
 * Link real input.c, SACT2.c, video.c, governor.c and touch_gesture.c with
 * -D__ANDROID__ -DUSE_GLES -DSDL_MAIN_HANDLED -ffunction-sections;
 * discard unused engine sections. SDL time and platform presentation
 * are replaced for deterministic stall measurements.
 */
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4.h"
#include "xsystem4.h"
#include "input.h"
#include "scene.h"
#include "gfx/private.h"
#include "governor.h"
#include "touch_gesture.h"

struct config config;
bool scene_is_dirty;
bool dbg_dap;
bool dbg_start_in_debugger;
static Uint32 clock_ms;
static Uint32 delayed_ms;
static unsigned presented;
static unsigned rendered;
static unsigned failures;

Uint32 SDL_GetTicks(void) { return clock_ms; }
Uint64 SDL_GetTicks64(void) { return clock_ms; }
void SDL_Delay(Uint32 ms) { clock_ms += ms; delayed_ms += ms; }
void SDL_GL_SwapWindow(SDL_Window *window) { (void)window; presented++; }
void scene_render(void) { rendered++; }
void sprite_call_plugins(void) {}
void __wrap_gfx_update_screen_scale(void) {}
void __wrap_gfx_toggle_fullscreen(void) {}
void screenshot_save(void) {}
void vm_exit(int code) { (void)code; abort(); }
void vm_stack_trace(void) {}
void dbg_repl(int a, const char *b) { (void)a; (void)b; }
void dbg_dap_handle_messages(void) {}
void *_xmalloc(size_t n, const char *caller) { (void)caller; return malloc(n); }

#define GL_NOOP(name, args) void name args {}
GL_NOOP(glBindFramebuffer, (GLenum a, GLuint b))
GL_NOOP(glViewport, (GLint a, GLint b, GLsizei c, GLsizei d))
GL_NOOP(glClearColor, (GLfloat a, GLfloat b, GLfloat c, GLfloat d))
GL_NOOP(glClear, (GLbitfield a))
GL_NOOP(glUseProgram, (GLuint a))
GL_NOOP(glUniformMatrix4fv, (GLint a, GLsizei b, GLboolean c, const GLfloat *d))
GL_NOOP(glActiveTexture, (GLenum a))
GL_NOOP(glBindTexture, (GLenum a, GLuint b))
GL_NOOP(glUniform1i, (GLint a, GLint b))
GL_NOOP(glBindVertexArray, (GLuint a))
GL_NOOP(glEnableVertexAttribArray, (GLuint a))
GL_NOOP(glBindBuffer, (GLenum a, GLuint b))
GL_NOOP(glVertexAttribPointer, (GLuint a, GLint b, GLenum c, GLboolean d, GLsizei e, const void *f))
GL_NOOP(glDrawElements, (GLenum a, GLsizei b, GLenum c, const void *d))
GL_NOOP(glDisableVertexAttribArray, (GLuint a))

extern int sact_Update(void);
#define CHECK(c, label) do { printf("%s %s\n", (c) ? "PASS" : "FAIL", label); if (!(c)) failures++; } while (0)

static void init_governor(Uint32 start)
{
 clock_ms=start; delayed_ms=0; presented=rendered=0;
 scene_is_dirty=false;
 const struct governor_config cfg={16,100,2000};
 governor_init(&cfg);
 touch_gesture_init(&(struct tg_config){.screen_w=640,.screen_h=480,.viewport_w=640,.viewport_h=480,.game_w=640,.game_h=480});
 SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
}

int main(void)
{
 SDL_setenv("SDL_VIDEODRIVER","dummy",1);
 CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)==0,"SDL event queue initialized");
 config.view_width=640; config.view_height=480;
 sdl.w=640; sdl.h=480; sdl.viewport=(SDL_Rect){0,0,640,480};
 sdl.window=SDL_CreateWindow("synthetic governor fixture",0,0,640,480,0);
 CHECK(sdl.window != NULL,"synthetic window created");

 init_governor(1000);
 // SACT's real update presents each dirty animation frame and then clears
 // scene_is_dirty. Multiple HLL input queries pump between frame updates.
 for (unsigned frame=0; frame<190; frame++) {
  clock_ms += 16;
  for (unsigned query=0; query<8; query++)
   handle_events();
  scene_is_dirty=true;
  sact_Update();
 }
 printf("OBSERVE animation frames=190 presents=%u scene_renders=%u delayed_ms=%u clock_ms=%u\n",
  presented,rendered,delayed_ms,clock_ms);
 CHECK(presented==190 && rendered==190,"real SACT -> gfx_swap -> SDL present path used for every synthetic frame");
 CHECK(delayed_ms==0,"continuous animation never enters Android idle-sleep block");
 CHECK(!governor_power_save(),"continuous animation remains active beyond idle threshold");

 // Stopping render requests must still permit static reading power save.
 delayed_ms=0;
 clock_ms+=2000;
 handle_events();
 CHECK(governor_power_save(),"static scene enters power save after render activity expires");
 CHECK(delayed_ms==100,"static Android input pump retains low-rate pacing");
 const unsigned static_presented=presented;
 const unsigned static_rendered=rendered;
 for (unsigned frame=0;frame<5;frame++)
  sact_Update();
 CHECK(governor_power_save() && presented==static_presented && rendered==static_rendered,
  "actual clean SACT frames remain static without requesting swaps");

 // A dirty frame resumes full rate through the real render entry point.
 scene_is_dirty=true;
 sact_Update();
 CHECK(!governor_power_save(),"actual dirty swap wakes power save without user input");
 delayed_ms=0;
 handle_events();
 CHECK(delayed_ms==0,"woken animation input pump does not sleep");

 // An actual queued tap still interrupts idle before its first delay chunk.
 clock_ms+=2000;
 governor_tick(clock_ms);
 CHECK(governor_power_save(),"fixture returned to static power save");
 SDL_Event e={0}; e.type=SDL_FINGERDOWN; e.tfinger.touchId=1; e.tfinger.fingerId=1;
 e.tfinger.x=.5f; e.tfinger.y=.5f; SDL_PushEvent(&e);
 delayed_ms=0;
 handle_events();
 CHECK(delayed_ms==0 && !governor_power_save(),"queued touch immediately wakes Android idle path");

 // Unsigned activity age must remain correct across SDL tick wraparound.
 init_governor(UINT32_MAX-1500);
 for (unsigned frame=0;frame<190;frame++) {
  clock_ms+=16; handle_events(); scene_is_dirty=true; sact_Update();
 }
 CHECK(delayed_ms==0 && !governor_power_save(),"continuous animation remains active across tick wraparound");
 touch_gesture_shutdown();
 SDL_DestroyWindow(sdl.window); SDL_Quit();
 printf("RESULT failures=%u\n",failures);
 return failures ? 1 : 0;
}
