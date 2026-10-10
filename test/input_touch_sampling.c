/* Regression for actual HLL ordering: every query pumps handle_events().
 * Real input.c + gesture FSM + real SDL queue; no game scripts or variables.
 */
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system4.h"
#include "xsystem4.h"
#include "input.h"
#include "touch_gesture.h"
#include "governor.h"
#include "gfx/private.h"
#include "debugger.h"

struct sdl_private sdl;
struct config config;
bool scene_is_dirty;
bool dbg_dap;
bool dbg_start_in_debugger;
void gfx_swap(void) {}
void gfx_update_screen_scale(void) {}
void gfx_flush_pending_present(void) {}
void screenshot_save(void) {}
void vm_exit(int code) { (void)code; abort(); }
void dbg_repl(enum dbg_stop_type a, const char *b) { (void)a; (void)b; }
void dbg_dap_handle_messages(void) {}
void *_xmalloc(size_t n, const char *caller) { (void)caller; void *p=malloc(n); if (!p) abort(); return p; }
void vm_stack_trace(void) {}
void gfx_toggle_fullscreen(void) {}
extern void input_begin_frame(enum input_frame_source source) __attribute__((weak));
#ifdef TEST_TOUCH_BASELINE
#include "/root/play_D/realgame_touch/baseline/input.c"
#else
#include "../src/input.c"
#endif

static int failures;
#define CHECK(c,m) do { printf("%s %s\n",(c)?"PASS":"FAIL",m); if (!(c)) failures++; } while (0)

static void queue(Uint32 type, Sint64 finger, float x, float y)
{
	SDL_Event e = {0};
	e.type = type;
	e.tfinger.touchId = 77;
	e.tfinger.fingerId = finger;
	e.tfinger.x = x;
	e.tfinger.y = y;
	if (SDL_PushEvent(&e) != 1) abort();
}

/* Same call ordering as sact_Mouse_GetPos and sact_Key_IsDown. */
static void position(int *x, int *y) { handle_events(); mouse_get_pos(x,y); }
static bool query(enum sact_keycode key) { handle_events(); return key_is_down(key); }

static void frame(enum input_frame_source source)
{
#ifdef TEST_TOUCH_BASELINE
	if (input_begin_frame)
#endif
		input_begin_frame(source);
}

static void reset(void)
{
	SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
	int reset_forward,reset_back;
	mouse_get_wheel(&reset_forward,&reset_back);mouse_clear_wheel();
	key_clear_flag(false);
	struct tg_config cfg = { .screen_w=1024,.screen_h=768,
		.viewport_w=1024,.viewport_h=768,.game_w=1024,.game_h=768,
		.hold_ms=350,.drag_threshold=.015f,.scroll_step=.02f };
	touch_gesture_init(&cfg);
	touch_gesture_inited=true;
	touch_owns_buttons=false;
	config.game_name="\xc0\xbc\xcb\xb9" "03";
	config.ain_filename="Rance03.ain";
#ifndef TEST_TOUCH_BASELINE
	touch_frames_active=false;
	touch_frame_polled=false;
	touch_frame_pointer_owned=false;
	memset(&touch_frame_output,0,sizeof(touch_frame_output));
#endif
	frame(INPUT_FRAME_PARTS);
}

static void tap(Sint64 finger, float x, float y)
{
	queue(SDL_FINGERDOWN,finger,x,y);
	queue(SDL_FINGERUP,finger,x,y);
}

int main(void)
{
	setbuf(stdout,NULL);
	SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER)<0) abort();
	sdl.window=SDL_CreateWindow("touch sampling regression",0,0,1024,768,0);
	if (!sdl.window) abort();
	sdl.w=1024;sdl.h=768;
	sdl.viewport.w=1024;sdl.viewport.h=768;
	config.view_width=1024;config.view_height=768;

	reset();
	tap(1,.25f,.5f);
	int x,y;
	position(&x,&y);
	for(int i=0;i<20;i++) { handle_events(); mouse_get_pos(&x,&y); }
	CHECK(query(VK_LBUTTON),"tap survives coordinate HLL and repeated event pumps before L sample");
	CHECK(x==256&&y==384,"tap coordinates stay attached before actual button sample");
	CHECK(!query(VK_RETURN),"unrelated key remains false");
	CHECK(!query(VK_RBUTTON),"R query remains false during L pulse");
	CHECK(query(VK_LBUTTON),"same logical frame repeated L queries retain press");
	frame(INPUT_FRAME_SACT);
	CHECK(query(VK_LBUTTON),"Rance Parts then SACT does not advance twice");
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_LBUTTON),"next logical Parts frame observes release");
	position(&x,&y);
	CHECK(!query(VK_LBUTTON),"release does not create another tap");

	reset();
	tap(2,.25f,.5f);tap(3,.75f,.25f);
	position(&x,&y);
	CHECK(query(VK_LBUTTON)&&x==256&&y==384,"FIFO first tap press/position survive position then button");
	frame(INPUT_FRAME_PARTS);
	position(&x,&y);
	CHECK(!query(VK_LBUTTON)&&x==256&&y==384,"FIFO first release still reports first coordinates");
	frame(INPUT_FRAME_PARTS);
	position(&x,&y);
	CHECK(query(VK_LBUTTON)&&x==768&&y==192,"FIFO next tap press has its own coordinates");
	frame(INPUT_FRAME_PARTS);
	position(&x,&y);
	CHECK(!query(VK_LBUTTON)&&x==768&&y==192,"FIFO next release has its own coordinates");
	CHECK(!query(VK_LBUTTON),"FIFO drains with no extra click");

	reset();
	queue(SDL_FINGERDOWN,4,.30f,.50f);queue(SDL_FINGERDOWN,5,.60f,.50f);
	queue(SDL_FINGERUP,4,.30f,.50f);queue(SDL_FINGERUP,5,.60f,.50f);
	position(&x,&y);
	CHECK(!query(VK_LBUTTON),"two-finger cancel does not emit L");
	CHECK(query(VK_RBUTTON),"two-finger cancel R survives preceding coordinate/L HLLs");
	CHECK(!query(VK_RETURN),"unrelated key does not consume R");
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_RBUTTON),"R release is sampled in the next logical frame");

	reset();
	tap(6,.5f,.5f);
	CHECK(query(VK_LBUTTON),"button-first sampling sees tap press");
	position(&x,&y);
	CHECK(x==512&&y==384,"button-first position retains same pulse");
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_LBUTTON),"button-first query observes next-frame release");

	// A newly arrived tap cannot disappear at a marker or between HLLs.
	reset();
	handle_events(); // current frame has already published idle
	tap(7,.25f,.5f);
	CHECK(!query(VK_LBUTTON),"mid-frame new tap waits for a full logical sample");
	frame(INPUT_FRAME_PARTS);
	position(&x,&y);
	CHECK(query(VK_LBUTTON)&&x==256&&y==384,"mid-frame tap appears intact in next frame");
	for(int i=0;i<20;i++) CHECK(query(VK_LBUTTON),"repeated same-key getters do not guess frames");
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_LBUTTON),"mid-frame tap then releases next frame");

	// A key the game never reads must not block subsequent useful input.
	reset();
	queue(SDL_FINGERDOWN,8,.30f,.50f);queue(SDL_FINGERDOWN,9,.60f,.50f);
	queue(SDL_FINGERUP,8,.30f,.50f);queue(SDL_FINGERUP,9,.60f,.50f);
	tap(10,.75f,.25f);
	CHECK(!query(VK_LBUTTON),"unread R phase does not create L");
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_LBUTTON),"unread R has a release frame");
	frame(INPUT_FRAME_PARTS);
	position(&x,&y);
	CHECK(query(VK_LBUTTON)&&x==768&&y==192,"unread R does not block next FIFO L tap");

	reset();tap(11,.25f,.5f);tap(12,.75f,.25f);
	CHECK(query(VK_LBUTTON),"clear-flags test starts with a queued tap");
	key_clear_flag(false);
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_LBUTTON),"clear flags discards published press and queued next tap");
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_LBUTTON),"discarded old taps never leak across UI clear");

	// First marker adopts an already visible pulse without advancing it.
	reset();
#ifndef TEST_TOUCH_BASELINE
	touch_frames_active=false;
	touch_frame_polled=false;
#endif
	tap(13,.5f,.5f);
	handle_events();
	frame(INPUT_FRAME_PARTS);
	CHECK(query(VK_LBUTTON),"first driver adoption preserves already published press");
	frame(INPUT_FRAME_PARTS);
	CHECK(!query(VK_LBUTTON),"adopted press releases at the next primary marker");

	reset();tap(14,.25f,.5f);
	queue(SDL_FINGERDOWN,15,.70f,.5f);queue(SDL_FINGERMOTION,15,.90f,.5f);
	position(&x,&y);
	CHECK(query(VK_LBUTTON)&&x==256,"queued tap precedes newer drag");
	frame(INPUT_FRAME_PARTS);position(&x,&y);
	CHECK(!query(VK_LBUTTON)&&x==256,"queued tap release remains at its original position");
	frame(INPUT_FRAME_PARTS);position(&x,&y);
	CHECK(query(VK_LBUTTON)&&x>=920&&x<=922,"active drag resumes after queued press/release");
	queue(SDL_FINGERUP,15,.90f,.5f);
	handle_events();
	CHECK(query(VK_LBUTTON),"drag release event cannot mutate current logical sample");
	position(&x,&y);
	CHECK(x>=920&&x<=922,"same-frame drag UP retains latched position while L is down");
	frame(INPUT_FRAME_PARTS);
	// PE_UpdateInputState reads key/position directly before SACT pumps events.
	mouse_get_pos(&x,&y);
	CHECK(key_is_down(VK_LBUTTON)&&x>=920&&x<=922,
		"Parts direct key/position reads retain previous drag sample before poll");
	CHECK(!query(VK_LBUTTON),"drag release is visible next frame");

	reset();tap(16,.25f,.5f);queue(SDL_FINGERDOWN,17,.70f,.5f);
	handle_events();SDL_Delay(360);handle_events();
	CHECK(query(VK_LBUTTON)&&!query(VK_RBUTTON),"queued tap press stays stable during 350ms hold recognition");
	frame(INPUT_FRAME_PARTS);position(&x,&y);
	CHECK(!query(VK_LBUTTON)&&!query(VK_RBUTTON)&&x==256,"tap release phase precedes held R");
	frame(INPUT_FRAME_PARTS);position(&x,&y);
	CHECK(query(VK_RBUTTON)&&x>=715&&x<=717,"long-held R resumes at its touch origin");
	key_clear_flag(false);position(&x,&y);
	CHECK(x>=715&&x<=717,"clear flags preserves sampled coordinates while finger is active");
	frame(INPUT_FRAME_PARTS);
	CHECK(query(VK_RBUTTON),"physical long hold resumes after cleared frame");
	queue(SDL_FINGERUP,17,.70f,.5f);handle_events();
	position(&x,&y);
	CHECK(query(VK_RBUTTON)&&x>=715&&x<=717,"same-frame hold UP retains latched position while R is down");
	frame(INPUT_FRAME_PARTS);
	mouse_get_pos(&x,&y);
	CHECK(key_is_down(VK_RBUTTON)&&x>=715&&x<=717,
		"Parts direct key/position reads retain previous hold sample before poll");
	CHECK(!query(VK_RBUTTON),"held R release is visible at primary frame boundary");

	// Rance03 MouseInput.updateWheel uses Forward-Back; positive opens backlog.
	reset();mouse_clear_wheel();
	queue(SDL_FINGERDOWN,19,.30f,.30f);queue(SDL_FINGERDOWN,20,.60f,.30f);
	queue(SDL_FINGERMOTION,19,.30f,.45f);queue(SDL_FINGERMOTION,20,.60f,.45f);
	handle_events();
	int forward,back;
	mouse_get_wheel(&forward,&back);
	CHECK(forward&&!back,"two-finger down emits positive game wheel for actual AIN backlog");
	CHECK(!query(VK_LBUTTON)&&!query(VK_RBUTTON)&&!query(VK_CONTROL),
		"down-scroll has no click or fast-forward side effect");
	mouse_clear_wheel();
	for(int i=0;i<20;i++) handle_events();
	mouse_get_wheel(&forward,&back);
	CHECK(!forward&&!back,"cleared wheel is not replayed by same-frame HLL pumps");
	frame(INPUT_FRAME_PARTS);handle_events();
	mouse_get_wheel(&forward,&back);
	CHECK(!forward&&!back,"wheel step is delivered once across the next frame");
	queue(SDL_FINGERUP,19,.30f,.45f);queue(SDL_FINGERUP,20,.60f,.45f);
	frame(INPUT_FRAME_PARTS);handle_events();
	CHECK(!query(VK_LBUTTON)&&!query(VK_RBUTTON)&&!query(VK_CONTROL),
		"completed down-scroll does not become tap or cancel");

	// Actual SYS_View_Update: Parts marker -> render poll -> Clear -> Peek.
	// The actual input consumer later calls GetCount -> ClearCount.
	reset();mouse_clear_wheel();
	queue(SDL_FINGERDOWN,21,.30f,.30f);queue(SDL_FINGERDOWN,22,.60f,.30f);
	queue(SDL_FINGERMOTION,21,.30f,.45f);queue(SDL_FINGERMOTION,22,.60f,.45f);
	handle_events();
	mouse_clear_wheel(); // SYS_View_Update 0x14dc66, before input GetCount
	handle_events(); // Peek can pump again in the same logical frame
	frame(INPUT_FRAME_PARTS);handle_events();mouse_clear_wheel();
	mouse_get_wheel(&forward,&back);
	CHECK(forward&&!back,"unread touch wheel survives real render Clear and later frame Clear");
	int forward2,back2;
	mouse_get_wheel(&forward2,&back2);
	CHECK(forward2==forward&&back2==back&&forward2,
		"separate wheel getters see the same delivered count before consumer Clear");
	mouse_clear_wheel();handle_events();frame(INPUT_FRAME_PARTS);handle_events();
	mouse_get_wheel(&forward,&back);
	CHECK(!forward&&!back,"consumer Get then Clear prevents same-frame and next-frame replay");

	// A fresh step after a getter must not be discarded by the old read's clear.
	queue(SDL_FINGERMOTION,21,.30f,.60f);queue(SDL_FINGERMOTION,22,.60f,.60f);
	frame(INPUT_FRAME_PARTS);handle_events();mouse_get_wheel(&forward,&back);
	CHECK(forward&&!back,"new scroll step is delivered normally");
	queue(SDL_FINGERMOTION,21,.30f,.75f);queue(SDL_FINGERMOTION,22,.60f,.75f);
	frame(INPUT_FRAME_PARTS);handle_events();mouse_clear_wheel();
	mouse_get_wheel(&forward,&back);
	CHECK(forward&&!back,"old consumer Clear cannot discard newly staged unread touch step");
	mouse_clear_wheel();
	queue(SDL_FINGERUP,21,.30f,.75f);queue(SDL_FINGERUP,22,.60f,.75f);
	frame(INPUT_FRAME_PARTS);handle_events();
	mouse_get_wheel(&forward,&back);
	CHECK(!forward&&!back,"finished and consumed touch wheel leaves no idle residue");

	// Key_ClearFlag historically clears keys, not mouse wheel counts.
	reset();queue(SDL_FINGERDOWN,23,.30f,.30f);queue(SDL_FINGERDOWN,24,.60f,.30f);
	queue(SDL_FINGERMOTION,23,.30f,.45f);queue(SDL_FINGERMOTION,24,.60f,.45f);
	handle_events();mouse_clear_wheel();key_clear_flag(false);
	mouse_get_wheel(&forward,&back);
	CHECK(forward&&!back,"key flags clear does not silently consume unread mouse wheel");
	mouse_clear_wheel();

	// Physical counter API retains its original unconditional clear behavior.
	// Seed the counter: VM sdl2-compat changes pushed wheel y=-1 to polled y=0.
	// This checks counter semantics, not physical-device or SDL queue delivery.
	reset();wheel_dir=-1;
	mouse_clear_wheel();mouse_get_wheel(&forward,&back);
	CHECK(!forward&&!back,"unread physical counter is still cleared unconditionally");
	wheel_dir=-1;mouse_get_wheel(&forward,&back);
	CHECK(!forward&&back,"physical backward counter remains readable with idle touch");
	mouse_clear_wheel();

	// Other games retain SACT as their default marker, not sticky Parts.
	reset();
	config.game_name="Other game";config.ain_filename="Other.ain";
	tap(18,.5f,.5f);
	CHECK(query(VK_LBUTTON),"non-Rance tap publishes");
	frame(INPUT_FRAME_PARTS);
	CHECK(query(VK_LBUTTON),"non-Rance Parts does not steal SACT frame source");
	frame(INPUT_FRAME_SACT);
	CHECK(!query(VK_LBUTTON),"non-Rance SACT advances the logical frame");

	SDL_DestroyWindow(sdl.window);SDL_Quit();
	printf("RESULT failures=%d\n",failures);
	return failures?1:0;
}
