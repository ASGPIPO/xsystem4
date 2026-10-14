#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "governor.h"

bool scene_is_dirty = false;
static int failures;
#define CHECK(cond, name) do { \
    if (cond) printf("PASS %s\n", name); \
    else { printf("FAIL %s\n", name); failures++; } } while (0)

int main(void)
{
    uint32_t t = 1000;
    governor_init(NULL);

    // A. sleep_by_inactive OFF (default) + focus loss: static judgement unchanged
    governor_set_sleep_by_inactive(false);
    governor_mark_input(t);
    governor_note_window_active(false);          // focus lost, option off
    governor_tick(t + 500);                       // recent input -> active
    CHECK(!governor_power_save(), "OFF+focuslost: recent input stays active");
    governor_tick(t + 3000);                      // static 2s+ -> normal drop
    CHECK(governor_power_save(), "OFF+focuslost: static still drops (option off semantics)");

    // B. OFF + focus lost + fresh input -> wake (input beats nothing-suppressed)
    governor_mark_input(t + 4000);
    governor_tick(t + 4001);
    CHECK(!governor_power_save(), "OFF+focuslost: input wakes");

    // C. ON + focus lost + continuous render requests: no flapping, stays POWER_SAVE
    governor_set_sleep_by_inactive(true);
    governor_note_window_active(false);
    governor_tick(t + 4500);
    CHECK(governor_power_save(), "ON+focuslost: power save");
    for (int i = 0; i < 5; i++) {
        governor_note_render_request(t + 4600 + i);  // background animation swaps
        governor_tick(t + 4601 + i);
    }
    CHECK(governor_power_save(), "ON+focuslost: render requests do not flap to active");

    // D. ON + refocus + render request -> active again
    governor_note_window_active(true);
    governor_note_render_request(t + 5000);
    governor_tick(t + 5001);
    CHECK(!governor_power_save(), "ON+refocus: active again");

    // E. OFF overrides window flag entirely (OverFrameRateSleep master switch)
    governor_set_over_frame_rate_sleep(false);
    governor_note_window_active(false);
    governor_set_sleep_by_inactive(true);
    governor_tick(t + 6000);
    CHECK(!governor_power_save(), "master OFF beats inactive: active");

    printf("RESULT failures=%d\n", failures);
    return failures ? 1 : 0;
}
