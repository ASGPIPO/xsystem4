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
 * Power governor for the Android port of xsystem4.  See governor.h for
 * the design and the wiring contract.
 */

#include <SDL.h>

#include "governor.h"
#include "scene.h"

static struct governor_config gcfg = {
	.active_frame_ms = 16,
	.power_save_frame_ms = 100,
	.idle_enter_ms = 2000,
};
static enum governor_state gstate = GOV_ACTIVE;
static bool inited;
static bool force_render;
static uint32_t last_input_ms;
static uint32_t last_render_ms;
// Independent clock for the input-pump pacing (power save).  Unlike
// last_render_ms it advances even when nothing is rendered, so the
// pacing loop keeps working on a fully static scene with no swap
// requests at all.
static uint32_t last_pump_ms;

void governor_init(const struct governor_config *cfg)
{
	if (cfg && cfg->active_frame_ms && cfg->power_save_frame_ms && cfg->idle_enter_ms)
		gcfg = *cfg;
	last_input_ms = SDL_GetTicks();
	last_render_ms = last_input_ms;
	last_pump_ms = last_input_ms;
	gstate = GOV_ACTIVE;
	force_render = true;
	inited = true;
}

void governor_shutdown(void)
{
	inited = false;
}

void governor_mark_input(uint32_t now)
{
	last_input_ms = now;
	if (inited && gstate != GOV_ACTIVE) {
		// Wake up immediately on user input: render the first
		// active frame without waiting for a whole interval.
		gstate = GOV_ACTIVE;
		force_render = true;
	}
}

void governor_mark_scene_dirty(void)
{
	if (!inited)
		return;
	// Scene content changed: keep rendering at full rate.
	last_input_ms = SDL_GetTicks();
	if (gstate != GOV_ACTIVE) {
		gstate = GOV_ACTIVE;
		force_render = true;
	}
}

void governor_note_render_request(uint32_t now)
{
	if (!inited || gstate != GOV_POWER_SAVE)
		return;
	// Something wants to animate while the scene was judged static:
	// go back to full rate (self-correcting: a truly static scene
	// stops requesting swaps and drops back to power save).
	gstate = GOV_ACTIVE;
	force_render = true;
	last_input_ms = now;
}

void governor_tick(uint32_t now)
{
	if (!inited)
		return;

	// Static reading (scene clean, no recent input) drops to the low
	// frame rate; any dirty scene or user input restores 60 FPS.
	bool active = scene_is_dirty || (uint32_t)(now - last_input_ms) < gcfg.idle_enter_ms;
	enum governor_state next = active ? GOV_ACTIVE : GOV_POWER_SAVE;
	if (next != gstate) {
		gstate = next;
		if (next == GOV_ACTIVE) {
			force_render = true; // render immediately on wake-up
		} else {
			last_pump_ms = now; // start a fresh power-save frame slot
		}
	}
}

enum governor_state governor_get_state(void)
{
	return gstate;
}

bool governor_power_save(void)
{
	return gstate == GOV_POWER_SAVE;
}

uint32_t governor_frame_interval_ms(void)
{
	return gstate == GOV_ACTIVE ? gcfg.active_frame_ms : gcfg.power_save_frame_ms;
}

bool governor_should_render(uint32_t now)
{
	if (!inited)
		return true;
	if (force_render && gstate == GOV_ACTIVE) {
		force_render = false;
		last_render_ms = now;
		return true;
	}
	if ((uint32_t)(now - last_render_ms) >= governor_frame_interval_ms()) {
		last_render_ms = now;
		return true;
	}
	return false;
}

uint32_t governor_ms_until_next_frame(uint32_t now)
{
	if (!inited || gstate != GOV_POWER_SAVE)
		return 0;
	uint32_t elapsed = (uint32_t)(now - last_pump_ms);
	if (elapsed >= gcfg.power_save_frame_ms) {
		// A new frame slot starts: the caller runs one pump cycle
		// now and paces the rest of the slot on later calls.
		last_pump_ms = now;
		return 0;
	}
	return gcfg.power_save_frame_ms - elapsed;
}

bool governor_wait_event(SDL_Event *event)
{
	int timeout = 0;
	if (inited && gstate == GOV_POWER_SAVE) {
		// Block until input arrives or the next low-rate frame is due.
		uint32_t now = SDL_GetTicks();
		uint32_t elapsed = (uint32_t)(now - last_render_ms);
		timeout = elapsed >= gcfg.power_save_frame_ms
			? 0
			: (int)(gcfg.power_save_frame_ms - elapsed);
	}
	return SDL_WaitEventTimeout(event, timeout) == 1;
}