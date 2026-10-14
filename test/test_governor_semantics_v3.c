/* v3: reviewer2 F2 gap coverage. Append-style extra cases run on the
 * same linked governor.c as v1/v2 (fresh governor_init each section). */
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

    // (a) SBI OFF + focus lost + continuous render requests: must keep
    // waking to ACTIVE (BUG006 unconditional-refresh contract guarded).
    governor_init(NULL);
    governor_set_sleep_by_inactive(false);
    governor_note_window_active(false);
    governor_mark_input(t);
    governor_tick(t + 2500);                 // static -> power save
    CHECK(governor_power_save(), "a0 static drops");
    for (int i = 0; i < 5; i++) {
        governor_note_render_request(t + 2600 + i);
        governor_tick(t + 2601 + i);
    }
    CHECK(!governor_power_save(), "a SBI-off+focuslost: render requests keep waking (BUG006 guard)");

    // (b) SBI ON + window ACTIVE + POWER_SAVE: render request wakes to ACTIVE
    governor_init(NULL);
    governor_set_sleep_by_inactive(true);
    governor_note_window_active(true);
    governor_mark_input(t);
    governor_tick(t + 2500);
    CHECK(governor_power_save(), "b0 static drops");
    governor_note_render_request(t + 2700);
    CHECK(!governor_power_save(), "b SBI-on+active: render request wakes");

    // (c) master OFF + SBI ON + focus lost: note_window_active must not
    // transition to POWER_SAVE (doc contract) and tick stays ACTIVE.
    governor_init(NULL);
    governor_set_over_frame_rate_sleep(false);
    governor_set_sleep_by_inactive(true);
    governor_note_window_active(false);
    CHECK(!governor_power_save(), "c master-off: note_window_active does not throttle");
    governor_tick(t + 3000);
    CHECK(!governor_power_save(), "c master-off: tick stays active");

    // (d) suppression keeps 100ms render gating (no full-speed flap)
    governor_init(NULL);
    governor_set_sleep_by_inactive(true);
    governor_mark_input(t);
    governor_note_window_active(false);
    governor_tick(t + 2500);
    CHECK(governor_power_save(), "d0 suppressed power save");
    bool first = governor_should_render(t + 2501);
    bool second = governor_should_render(t + 2510);   // 9ms later: gated
    bool third = governor_should_render(t + 2601);    // 100ms slot: open
    CHECK(first && !second && third, "d suppressed: 100ms slot gating intact");

    printf("RESULT failures=%d\n", failures);
    return failures ? 1 : 0;
}
