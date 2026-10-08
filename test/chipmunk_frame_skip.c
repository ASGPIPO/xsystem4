/* Included after ChipmunkSpriteEngine.c with a focused export table. */
#include <stdio.h>

bool scene_is_dirty = true;
static bool read_skipping, frame_skip = true, invalidated;
static int events, plugins, draws;

bool StoatSpriteEngine_SYSTEM_GetReadMessageSkipping(void) { return read_skipping; }
bool StoatSpriteEngine_SYSTEM_GetConfigFrameSkipWhileMessageSkip(void) { return frame_skip; }
bool StoatSpriteEngine_SYSTEM_GetInvalidateFrameSkipWhileMessageSkip(void) { return invalidated; }
void handle_events(void) { events++; }
void sprite_call_plugins(void) { plugins++; }
int sact_Update(void)
{
	handle_events();
	sprite_call_plugins();
	if (scene_is_dirty) {
		draws++;
		scene_is_dirty = false;
	}
	return 1;
}

int main(void)
{
	int (*update)(void) = lib_ChipmunkSpriteEngine.functions[0].fun;
	void (*set_all)(bool) = lib_ChipmunkSpriteEngine.functions[1].fun;
	assert(update && set_all);
	set_all(true);
	assert(update() == 1); /* configuration alone never skips normal reading */
	assert(draws == 1);

	scene_is_dirty = true;
	read_skipping = true;
	assert(update() == 1);
	assert(draws == 1 && scene_is_dirty);
	assert(events == 2 && plugins == 2); /* input and animation stay alive */

	invalidated = true;
	assert(update() == 1);
	assert(draws == 2 && !scene_is_dirty);
	invalidated = false;
	scene_is_dirty = true;
	frame_skip = false;
	assert(update() == 1);
	assert(draws == 3);

	frame_skip = true;
	scene_is_dirty = true;
	assert(update() == 1);
	assert(draws == 3 && scene_is_dirty);
	set_all(false);
	assert(update() == 1);
	assert(draws == 4 && !scene_is_dirty);
	assert(events == 6 && plugins == 6);
	puts("PASS frame_skip_all/input/plugins/dirty/resume/overrides");
	return 0;
}
