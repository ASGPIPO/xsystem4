/* Focused semantics test for the OverFrameRateSleep / SleepByInactive
 * governor wiring. Links governor.c standalone (scene_is_dirty mocked). */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "governor.h"

bool scene_is_dirty = false; // mocks scene.c
static int failures;
#define CHECK(cond, name) do { \
    if (cond) printf("PASS %s\n", name); \
    else { printf("FAIL %s\n", name); failures++; } } while (0)

int main(void)
{
    uint32_t t = 1000;
    governor_init(NULL);
    CHECK(governor_get_state() == GOV_ACTIVE, "init active");

    // 1. static 2.5s (clean scene, no input) -> POWER_SAVE
    governor_mark_input(t);
    governor_tick(t + 2500);
    CHECK(governor_power_save(), "static drops to power save");

    // 2. OverFrameRateSleep OFF -> always active, render gate open
    governor_set_over_frame_rate_sleep(false);
    governor_tick(t + 2600);
    CHECK(!governor_power_save(), "OFF: forced active");
    CHECK(governor_should_render(t + 2601) && governor_should_render(t + 2602),
          "OFF: render gate always open");

    // 3. ON again -> static falls back to power save
    governor_set_over_frame_rate_sleep(true);
    governor_mark_input(t + 3000);
    governor_tick(t + 3000);
    CHECK(!governor_power_save(), "ON: fresh input active");
    governor_tick(t + 5600);
    CHECK(governor_power_save(), "ON: static again power save");

    // 4. SleepByInactive ON + window inactive -> power save despite input
    governor_set_sleep_by_inactive(true);
    governor_mark_input(t + 6000); // input while active-window assumed
    governor_note_window_active(false);
    governor_tick(t + 6001);
    CHECK(governor_power_save(), "inactive window -> power save");
    // 5. window active again -> active
    governor_note_window_active(true);
    CHECK(!governor_power_save(), "window active -> active");

    // 6. OverFrameRateSleep OFF overrides SleepByInactive
    governor_note_window_active(false);
    governor_tick(t + 6100);
    CHECK(governor_power_save(), "inactive again power save");
    governor_set_over_frame_rate_sleep(false);
    governor_tick(t + 6200);
    CHECK(!governor_power_save(), "OFF overrides inactive: active");

    printf("RESULT failures=%d\n", failures);
    return failures ? 1 : 0;
}
