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

#ifndef SYSTEM4_GOVERNOR_H
#define SYSTEM4_GOVERNOR_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL.h>

/*
 * Power governor for the Android port.
 *
 * Mobile games spend most of their time on a static screen (reading).
 * The governor watches two activity signals:
 *
 *   - the input timestamp (fed by the input layer on every touch/key
 *     event), and
 *   - the scene dirty flag (scene_is_dirty in scene.c, plus explicit
 *     governor_mark_scene_dirty() calls from render-side hooks).
 *
 * and switches between two modes:
 *
 *   GOV_ACTIVE     60 FPS.  Paced by vsync; the event loop polls.
 *   GOV_POWER_SAVE low frame rate for static reading.  The event loop
 *                  blocks in SDL_WaitEventTimeout() until input arrives
 *                  or the next low-rate frame is due, and rendering is
 *                  gated by governor_should_render().
 *
 * Wiring (as connected in the Android port):
 *
 *   governor_init(NULL);                  // system4.c main(), __ANDROID__
 *   governor_mark_input(now);             // input.c, on touch/key events
 *   governor_note_render_request(now);    // video.c gfx_swap(), every swap
 *   governor_tick(now);                   // input.c fire_deferred_events()
 *   if (governor_power_save() && !governor_should_render(now))
 *       defer;                            // video.c gfx_swap() present gate
 *   idle: SDL_Delay in <=10ms chunks for governor_ms_until_next_frame()
 *                                          // input.c fire_deferred_events()
 *
 * The engine has no single main render loop (the VM drives rendering
 * through SACT2.Update), so the governor is connected at gfx_swap()
 * and at the input pump instead of wrapping an event loop.
 */

enum governor_state {
	GOV_ACTIVE = 0,     // 60 FPS
	GOV_POWER_SAVE = 1, // static reading: low frame rate + event blocking
};

struct governor_config {
	uint32_t active_frame_ms;     // frame interval in active mode (default: 16)
	uint32_t power_save_frame_ms; // frame interval in power save (default: 100)
	uint32_t idle_enter_ms;       // inactivity before entering power save (default: 2000)
};

void governor_init(const struct governor_config *cfg);
void governor_shutdown(void);

// Activity hooks. "now" is a millisecond timestamp (SDL_GetTicks()).
void governor_mark_input(uint32_t now);
void governor_mark_scene_dirty(void);

// Render-side hook: call on every swap request (from gfx_swap). A swap
// request while in power save means something is animating, so the
// governor returns to active mode immediately.
void governor_note_render_request(uint32_t now);

// Milliseconds until the next power-save frame slot is due (0 in
// active mode, or when the slot is already due).  Lets the input layer
// sleep in small chunks while the scene is static without adding
// input latency.
uint32_t governor_ms_until_next_frame(uint32_t now);

// Re-evaluate the mode from the scene dirty flag and the input timestamp.
// Call once per frame.
void governor_tick(uint32_t now);

enum governor_state governor_get_state(void);
bool governor_power_save(void);
uint32_t governor_frame_interval_ms(void);

// Render gate: true at most once per governor_frame_interval_ms().
// The transition into active mode (wake-up on input or on a render
// request) latches one immediate render so the first active frame is
// not delayed by a whole interval.
bool governor_should_render(uint32_t now);

// Event wait for the main loop, built on SDL_WaitEventTimeout().  In
// power save mode it blocks until an event arrives or the next low-rate
// frame is due; in active mode it behaves like SDL_PollEvent-style
// waiting with a zero timeout (vsync paces the loop).
bool governor_wait_event(SDL_Event *event);

#endif /* SYSTEM4_GOVERNOR_H */