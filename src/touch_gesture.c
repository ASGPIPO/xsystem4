/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Pure touch-screen gesture engine for the Android port of xsystem4.
 *
 * Converts raw finger events into System4 virtual key state, replacing
 * the legacy input.c touch handling (touch in viewport = left click,
 * touch on the letterbox border = right click).  See touch_gesture.h
 * for the state machine diagram and the public API contract.
 */

#include <stddef.h>
#include <stdlib.h>

#include "touch_gesture.h"

// ---------------------------------------------------------------------------
// State

static struct tg_config cfg;
static bool inited;
static enum tg_state state = TG_IDLE;
static tg_haptic_fn haptic_hook;

// Finger tracking. The first two fingers participate in two-finger
// gestures; any further fingers only bump nfingers so the gesture ends
// only when every finger is up.
#define TG_MAX_TRACKED 2
struct tracked_finger {
	bool active;
	int64_t id;
	float x, y;   // current normalized position
	float x0, y0; // position at finger-down
};
static struct tracked_finger fingers[TG_MAX_TRACKED];
static int nfingers; // total fingers down, including untracked ones

// Single-finger gesture memory
static int64_t primary_id;
static uint32_t t0;     // finger-down timestamp

// Multi-touch gesture memory
enum mlock {
	MLOCK_NONE = 0, // direction not decided yet
	MLOCK_SCROLL,   // vertical scroll: emits mouse wheel events
	MLOCK_FF,       // fast-forward: holds VK_CONTROL
};
static enum mlock mlock;
static uint32_t multi_t0;
static float multi_y0;        // average finger y at gesture start
static float scroll_anchor;   // base y for scroll step accumulation
static bool multi_moved;      // any real movement in this gesture

// Button pulses retain their button and position in arrival order.
// Each published pulse is followed by a release frame, including when
// a new finger is already dragging or holding.
struct button_pulse {
	struct button_pulse *next;
	bool right;
	int x, y;
};
static struct button_pulse *pulse_head, *pulse_tail;
static bool pulse_last_published;
static int pulse_x, pulse_y;
static int pending_wheel;
static int game_x, game_y;

// ---------------------------------------------------------------------------
// Helpers

static void clear_button_pulses(void)
{
	while (pulse_head) {
		struct button_pulse *next = pulse_head->next;
		free(pulse_head);
		pulse_head = next;
	}
	pulse_tail = NULL;
	pulse_last_published = false;
	pulse_x = pulse_y = 0;
}

static void queue_button_pulse(bool right)
{
	struct button_pulse *pulse = malloc(sizeof(*pulse));
	if (!pulse)
		abort();
	*pulse = (struct button_pulse) {
		.right = right, .x = game_x, .y = game_y,
	};
	if (pulse_tail)
		pulse_tail->next = pulse;
	else
		pulse_head = pulse;
	pulse_tail = pulse;
}

static struct tracked_finger *find_finger(int64_t id)
{
	for (int i = 0; i < TG_MAX_TRACKED; i++) {
		if (fingers[i].active && fingers[i].id == id)
			return &fingers[i];
	}
	return NULL;
}

static struct tracked_finger *track_finger(int64_t id, float x, float y)
{
	struct tracked_finger *f = find_finger(id);
	if (f) {
		f->x = x;
		f->y = y;
		return f;
	}
	for (int i = 0; i < TG_MAX_TRACKED; i++) {
		if (!fingers[i].active) {
			fingers[i].active = true;
			fingers[i].id = id;
			fingers[i].x = fingers[i].x0 = x;
			fingers[i].y = fingers[i].y0 = y;
			return &fingers[i];
		}
	}
	return NULL; // 3rd+ finger: untracked
}

static bool untrack_finger(int64_t id)
{
	for (int i = 0; i < TG_MAX_TRACKED; i++) {
		if (fingers[i].active && fingers[i].id == id) {
			fingers[i].active = false;
			return true;
		}
	}
	return false;
}

static float tracked_avg_y(void)
{
	float sum = 0.0f;
	int n = 0;
	for (int i = 0; i < TG_MAX_TRACKED; i++) {
		if (fingers[i].active) {
			sum += fingers[i].y;
			n++;
		}
	}
	return n ? sum / n : 0.0f;
}

static bool any_finger_moved(void)
{
	for (int i = 0; i < TG_MAX_TRACKED; i++) {
		if (!fingers[i].active)
			continue;
		float dx = fingers[i].x - fingers[i].x0;
		float dy = fingers[i].y - fingers[i].y0;
		if (dx * dx + dy * dy > cfg.drag_threshold * cfg.drag_threshold)
			return true;
	}
	return false;
}

// Map a normalized window position to the logical game coordinate system
// (e.g. 1024x768), through the letterbox viewport.  Same math as
// mouse_get_pos() in input.c, but targeting the game resolution instead
// of the window resolution.
static void update_game_pos(void)
{
	const struct tracked_finger *f = find_finger(primary_id);
	if (!inited || !f || cfg.viewport_w <= 0 || cfg.viewport_h <= 0) {
		game_x = 0;
		game_y = 0;
		return;
	}
	// TG_HOLDING reports the original touch position (the finger may
	// have drifted a little since finger-down); every other state
	// reports the current finger position.
	bool at_origin = state == TG_HOLDING;
	int wx = (int)((at_origin ? f->x0 : f->x) * cfg.screen_w);
	int wy = (int)((at_origin ? f->y0 : f->y) * cfg.screen_h);
	game_x = (wx - cfg.viewport_x) * cfg.game_w / cfg.viewport_w;
	game_y = (wy - cfg.viewport_y) * cfg.game_h / cfg.viewport_h;
}

static void cancel_single_finger_action(void)
{
	// Leaving the single-finger states releases the emulated buttons.
	state = TG_MULTI_TOUCH;
}

// ---------------------------------------------------------------------------
// State transitions

static void enter_pending_tap(uint32_t now, int64_t finger, float x, float y)
{
	state = TG_PENDING_TAP;
	primary_id = finger;
	t0 = now;
	track_finger(finger, x, y);
	update_game_pos();
}

static void enter_multi_touch(uint32_t now)
{
	// A second finger cancels any in-flight single-finger action.
	cancel_single_finger_action();
	mlock = MLOCK_NONE;
	multi_moved = false;
	multi_t0 = now;
	multi_y0 = tracked_avg_y();
	scroll_anchor = multi_y0;
}

static bool drag_threshold_exceeded(float x, float y)
{
	const struct tracked_finger *f = find_finger(primary_id);
	if (!f)
		f = &fingers[0];
	float dx = (x - f->x0) * cfg.screen_w;
	float dy = (y - f->y0) * cfg.screen_h;
	// Threshold is a fraction of the screen diagonal, compared squared.
	float diag2 = (float)cfg.screen_w * cfg.screen_w + (float)cfg.screen_h * cfg.screen_h;
	float thr2 = cfg.drag_threshold * cfg.drag_threshold * diag2;
	return dx * dx + dy * dy > thr2;
}

// ---------------------------------------------------------------------------
// Public API

void touch_gesture_init(const struct tg_config *config)
{
	if (config)
		cfg = *config;
	if (!cfg.hold_ms)
		cfg.hold_ms = 350;
	if (cfg.drag_threshold <= 0.0f)
		cfg.drag_threshold = 0.015f;
	if (cfg.scroll_step <= 0.0f)
		cfg.scroll_step = 0.02f;
	state = TG_IDLE;
	nfingers = 0;
	for (int i = 0; i < TG_MAX_TRACKED; i++)
		fingers[i].active = false;
	clear_button_pulses();
	pending_wheel = 0;
	game_x = 0;
	game_y = 0;
	inited = true;
}

void touch_gesture_shutdown(void)
{
	inited = false;
	state = TG_IDLE;
	nfingers = 0;
	for (int i = 0; i < TG_MAX_TRACKED; i++)
		fingers[i].active = false;
	clear_button_pulses();
	pending_wheel = 0;
	game_x = game_y = 0;
	haptic_hook = NULL;
}

void touch_gesture_set_screen_size(int w, int h)
{
	cfg.screen_w = w;
	cfg.screen_h = h;
}

void touch_gesture_set_viewport(int x, int y, int w, int h)
{
	cfg.viewport_x = x;
	cfg.viewport_y = y;
	cfg.viewport_w = w;
	cfg.viewport_h = h;
}

void touch_gesture_set_haptic_hook(tg_haptic_fn fn)
{
	haptic_hook = fn;
}

enum tg_state touch_gesture_get_state(void)
{
	return state;
}

void touch_gesture_finger_down(uint32_t now, int64_t finger, float x, float y)
{
	if (!inited)
		return;

	nfingers++;
	if (state == TG_MULTI_TOUCH) {
		bool restart = nfingers == 2; // fresh gesture after an abort
		track_finger(finger, x, y);
		if (restart)
			enter_multi_touch(now);
		return;
	}
	if (nfingers >= 2) {
		track_finger(finger, x, y);
		enter_multi_touch(now);
		return;
	}
	enter_pending_tap(now, finger, x, y);
}

void touch_gesture_finger_motion(uint32_t now, int64_t finger, float x, float y)
{
	(void)now; // time-based transitions run in touch_gesture_tick()
	if (!inited)
		return;

	bool primary = (finger == primary_id);
	if (track_finger(finger, x, y) && primary)
		update_game_pos();

	switch (state) {
	case TG_PENDING_TAP:
		// Movement past the drag threshold transitions to DRAGGING:
		// VK_LBUTTON is held from now on and follows the finger.
		if (drag_threshold_exceeded(x, y))
			state = TG_DRAGGING;
		break;
	case TG_MULTI_TOUCH: {
		// If the gesture was aborted (one finger lifted), ignore the
		// remaining finger until it is lifted too.
		if (nfingers < 2)
			break;
		float avg_y = tracked_avg_y();
		if (mlock == MLOCK_NONE) {
			float dy = avg_y - multi_y0;
			if (dy >= cfg.scroll_step) {
				mlock = MLOCK_SCROLL;
			} else if (dy <= -cfg.scroll_step) {
				mlock = MLOCK_FF;
			}
		}
		if (mlock == MLOCK_SCROLL) {
			// Swipe down: emit one mouse-wheel-back step per
			// scroll_step of accumulated movement. The game opens
			// the backlog in response to the wheel.
			while (avg_y - scroll_anchor >= cfg.scroll_step) {
				pending_wheel = -1;
				scroll_anchor += cfg.scroll_step;
			}
		}
		multi_moved = multi_moved || any_finger_moved();
		break;
	}
	default:
		break; // HOLDING/DRAGGING: position is tracked by SDL itself
	}
}

void touch_gesture_finger_up(uint32_t now, int64_t finger, float x, float y)
{
	if (!inited)
		return;

	struct tracked_finger *f = find_finger(finger);
	if (f) {
		f->x = x;
		f->y = y;
		if (finger == primary_id)
			update_game_pos();
	}
	if (nfingers > 0)
		nfingers--;

	switch (state) {
	case TG_PENDING_TAP:
		if (finger == primary_id) {
			// A tap (up before the hold timeout, without turning into
			// a drag or a hold) delivers a one-frame left click.
			queue_button_pulse(false);
			state = TG_IDLE;
		}
		break;
	case TG_DRAGGING:
		if (finger == primary_id)
			state = TG_IDLE; // releases VK_LBUTTON
		break;
	case TG_HOLDING:
		if (finger == primary_id)
			state = TG_IDLE; // releases VK_RBUTTON
		break;
	case TG_MULTI_TOUCH:
		untrack_finger(finger);
		if (nfingers <= 0) {
			// Gesture finished. A quick two-finger tap with (almost)
			// no movement cancels: one-frame right click.
			if (mlock == MLOCK_NONE && !multi_moved && now - multi_t0 < cfg.hold_ms) {
				queue_button_pulse(true);
			}
			mlock = MLOCK_NONE;
			state = TG_IDLE;
		} else if (nfingers == 1) {
			// One finger left over: abort the two-finger gesture and
			// ignore the remaining finger until it is lifted.
			mlock = MLOCK_NONE;
			state = TG_MULTI_TOUCH;
		}
		return;
	default:
		break;
	}
	untrack_finger(finger);
}

void touch_gesture_tick(uint32_t now)
{
	if (!inited)
		return;

	switch (state) {
	case TG_PENDING_TAP:
		// Long-press: fire an in-place right click (never a "black
		// border" click) plus haptic feedback, then hold.
		if (now - t0 >= cfg.hold_ms) {
			state = TG_HOLDING;
			// Re-anchor the reported position to the touch origin.
			update_game_pos();
			if (haptic_hook)
				haptic_hook(30);
		}
		break;
	case TG_MULTI_TOUCH:
		// Two-finger long-press: fast-forward.
		if (mlock == MLOCK_NONE && nfingers >= 2 && now - multi_t0 >= cfg.hold_ms)
			mlock = MLOCK_FF;
		break;
	default:
		break;
	}
}

struct tg_output touch_gesture_poll(void)
{
	struct tg_output o = {0};
	if (pulse_last_published) {
		// Release the previous pulse even if a new gesture is held.
		pulse_last_published = false;
	} else if (pulse_head) {
		struct button_pulse *pulse = pulse_head;
		o.lbutton_pulse = !pulse->right;
		o.rbutton_pulse = pulse->right;
		pulse_x = pulse->x;
		pulse_y = pulse->y;
		pulse_head = pulse->next;
		if (!pulse_head)
			pulse_tail = NULL;
		free(pulse);
		pulse_last_published = true;
	} else {
		// Resume the active gesture after the queued press/release pairs.
		o.lbutton = state == TG_DRAGGING;
		o.rbutton = state == TG_HOLDING;
		o.control = state == TG_MULTI_TOUCH && mlock == MLOCK_FF;
	}
	o.wheel = pending_wheel;
	touch_gesture_get_pos(&o.game_x, &o.game_y);
	pending_wheel = 0;
	return o;
}

bool touch_gesture_pulse_pending(void)
{
	return pulse_head || pulse_last_published;
}

void touch_gesture_get_pos(int *x, int *y)
{
	if (x)
		*x = pulse_last_published ? pulse_x : game_x;
	if (y)
		*y = pulse_last_published ? pulse_y : game_y;
}