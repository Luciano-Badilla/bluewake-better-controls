#include "dpad_menus.h"

#include <stdatomic.h>
#include <stdlib.h>

static CPUState* g_cpu;
static atomic_bool g_on;
// The game's menu state, as the last pad read saw it (bluewake_dpad_menu_state).
static atomic_uchar g_menu_open;
static atomic_uchar g_menu_status;

void bluewake_dpad_menus_reload(void) {
    const char* value = getenv("BLUEWAKE_DPAD_MENUS");
    atomic_store_explicit(&g_on, value != NULL && value[0] == '1', memory_order_relaxed);
}

void bluewake_dpad_menus_attach(CPUState* cpu) {
    g_cpu = cpu;
    bluewake_dpad_menus_reload();
}

void bluewake_dpad_menus_apply(DolPadState* pad) {
    CPUState* cpu = g_cpu;
    if (cpu == NULL || pad == NULL)
        return;
    const uint8_t open = (uint8_t)mem_read8(cpu, kBwDpadMenusOpen);
    const uint8_t status = (uint8_t)mem_read8(cpu, kBwDpadMenusStatus);
    atomic_store_explicit(&g_menu_open, open, memory_order_relaxed);
    atomic_store_explicit(&g_menu_status, status, memory_order_relaxed);
    if (!atomic_load_explicit(&g_on, memory_order_relaxed) || (pad->button & kBwPadDpad) == 0u)
        return;
    (void)bw_dpad_menus_map(open, status, &pad->button, &pad->stick_x, &pad->stick_y);
}

bool bluewake_dpad_menu_state(uint8_t* open, uint8_t* status) {
    if (open != NULL)
        *open = atomic_load_explicit(&g_menu_open, memory_order_relaxed);
    if (status != NULL)
        *status = atomic_load_explicit(&g_menu_status, memory_order_relaxed);
    return true;
}
