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

#ifndef SYSTEM4_TOUCH_GESTURE_H
#define SYSTEM4_TOUCH_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Pure touch-screen gesture engine for the Android port.
 *
 * A finite state machine translates raw finger events into System4
 * virtual key state (see input.h), so the game can be driven without a
 * mouse.  This replaces the legacy input.c touch handling, where a touch
 * inside the viewport was a left click and a touch on the letterbox
 * border was a right click; here a long-press anywhere produces the
 * right click at the original touch position.
 *
 * Single finger:
 *   IDLE --finger down--> PENDING_TAP
 *   PENDING_TAP --350 ms without movement--> HOLDING
 *       (VK_RBUTTON held at the touch origin + haptic feedback hook)
 *   PENDING_TAP --move > 1.5% of screen diagonal--> DRAGGING
 *       (VK_LBUTTON held, cursor follows the finger)
 *   PENDING_TAP --finger up--> tap (VK_LBUTTON pulse)
 *
 * Two fingers (MULTI_TOUCH):
 *   swipe down            -> mouse wheel backward (opens the backlog)
 *   swipe up / long press -> VK_CONTROL held (text fast-forward)
 *   quick tap             -> VK_RBUTTON pulse (cancel)
 *
 * The engine is platform independent: it performs no SDL calls.
 * Timestamps (milliseconds) and normalized finger coordinates ([0,1])
 * are supplied by the caller, typically from SDL finger events in
 * input.c.  Call touch_gesture_tick() once per frame so that hold
 * timeouts fire without new finger events, and touch_gesture_poll()
 * once per frame to read (and clear) the one-shot outputs.
 */

enum tg_state {
	TG_IDLE = 0,     // no finger on screen
	TG_PENDING_TAP,  // one finger down; may become a tap, a hold, or a drag
	TG_HOLDING,      // long-press fired: VK_RBUTTON held at the touch origin
	TG_DRAGGING,     // finger moved past the threshold: VK_LBUTTON held
	TG_MULTI_TOUCH,  // two or more fingers on screen
};

struct tg_config {
	int screen_w, screen_h;     // window size in px (sdl.w, sdl.h)
	int viewport_x, viewport_y; // letterbox viewport within the window (sdl.viewport)
	int viewport_w, viewport_h;
	int game_w, game_h;         // logical game resolution (1024x768 for Rance03)
	uint32_t hold_ms;           // long-press duration (default: 350)
	float drag_threshold;       // fraction of screen diagonal (default: 0.015)
	float scroll_step;          // 2-finger scroll step, fraction of height (default: 0.02)
};

struct tg_output {
	bool lbutton;       // VK_LBUTTON state (held while dragging)
	bool rbutton;       // VK_RBUTTON state (held while holding)
	bool control;       // VK_CONTROL state (2-finger fast-forward)
	bool lbutton_pulse; // one-shot VK_LBUTTON press (tap)
	bool rbutton_pulse; // one-shot VK_RBUTTON press (2-finger tap = cancel)
	int wheel;          // one-shot wheel event: +1 forward, -1 backward
	int game_x, game_y; // primary finger position in game coordinates
};

// Install the configuration and reset the state machine.
void touch_gesture_init(const struct tg_config *cfg);
void touch_gesture_shutdown(void);

// Update the window geometry when it changes (SDL_WINDOWEVENT_SIZE_CHANGED).
void touch_gesture_set_screen_size(int w, int h);
void touch_gesture_set_viewport(int x, int y, int w, int h);

// Haptic feedback hook, fired once when a long-press is recognized.
typedef void (*tg_haptic_fn)(uint32_t duration_ms);
void touch_gesture_set_haptic_hook(tg_haptic_fn fn);

enum tg_state touch_gesture_get_state(void);

// Feed raw finger events. Coordinates are normalized ([0,1]) window
// coordinates; "now" is a millisecond timestamp (SDL_GetTicks()).
// Finger identifiers are 64-bit (SDL_FingerID is Sint64; truncating to
// 32 bits can alias two distinct fingers).
void touch_gesture_finger_down(uint32_t now, int64_t finger, float x, float y);
void touch_gesture_finger_motion(uint32_t now, int64_t finger, float x, float y);
void touch_gesture_finger_up(uint32_t now, int64_t finger, float x, float y);

// Drive time-based transitions (long-press, 2-finger long-press). Call
// once per frame.
void touch_gesture_tick(uint32_t now);

// Read the gesture output. Absolute key states persist across calls;
// one-shot outputs (pulses and wheel) are cleared by this call.
struct tg_output touch_gesture_poll(void);

// Non-destructive read of the primary finger position in game
// coordinates (e.g. 1024x768). While no finger is down this returns the
// last known position.  Unlike touch_gesture_poll() this never clears
// one-shot outputs, so it is safe to call from mouse position queries.
void touch_gesture_get_pos(int *x, int *y);

#endif /* SYSTEM4_TOUCH_GESTURE_H */