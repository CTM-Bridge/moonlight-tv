/* moonlight-facing glue for the embedded CTM bridge core. Replicates the startup
 * the standalone app does in ui_app.c (stopSniff worker -> discover agent ->
 * enumerate -> bridge), minus the LVGL UI. Runs the bridge in-process; the
 * controller threads own the physical HID (hidraw + EVIOCGRAB), so moonlight must
 * have released its own input grip (see the ctm_bridge setting). */

#include "ctm_bridge_glue.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ctm_state.h"   /* core API + shared globals (g_running, g_scan, ...) */

static bool s_active = false;
static pthread_t s_refresh_thread;
static bool s_refresh_started = false;

/* Mirror ui_app.c's 2 s refresh_devices(): re-enumerate and re-publish the BT MAC
 * list so a controller that appeared (or whose MAC populated) slightly after
 * stream start still gets kept out of sniff mode by the stopSniff worker. Only
 * this thread touches g_scan/g_devices after start; g_bt_macs is mutex-guarded. */
static void *ctm_refresh_worker(void *arg)
{
    (void) arg;
    while (g_running) {
        usleep(2000000);   /* 2 s, matching the standalone app's refresh timer */
        if (!g_running) {
            break;
        }
        enumerate_devices(&g_scan);
        build_logical_devices(&g_scan, &g_devices);
        publish_bt_macs();
    }
    return NULL;
}

bool ctm_bridge_start(void)
{
    if (s_active) {
        return true;
    }
    g_running = true;

    /* Keep every bridged BT controller out of sniff mode for the session. */
    if (!g_stop_sniff_thread_started) {
        if (pthread_create(&g_stop_sniff_thread, NULL, stop_sniff_worker, NULL) == 0) {
            g_stop_sniff_thread_started = true;
            log_append("ctm glue: stopSniff worker started");
        } else {
            log_append("ctm glue: stopSniff worker start failed");
        }
    }

    /* Re-publish BT MACs every 2 s (mirrors the standalone app's refresh timer). */
    if (!s_refresh_started) {
        if (pthread_create(&s_refresh_thread, NULL, ctm_refresh_worker, NULL) == 0) {
            s_refresh_started = true;
        }
    }

    /* Locate the Windows agent, enumerate controllers, bridge the first one we
     * recognise. Minimal: a single controller; multi-controller and hotplug
     * (via ctm_monitor) are later refinements. */
    if (!discover_agent_once()) {
        log_append("ctm glue: no CTM agent found on the network");
    }
    enumerate_devices(&g_scan);
    build_logical_devices(&g_scan, &g_devices);
    // Fill g_bt_macs so the stopSniff worker actually keeps the BT controllers
    // out of sniff mode (the worker reads this list every 500 ms).
    publish_bt_macs();

    bool bridged = false;
    for (int i = 0; i < g_devices.count; ++i) {
        logical_device_t *item = &g_devices.items[i];
        const char *kind = bridge_kind_for_item(item);
        /* bridge_kind_for_item() NEVER returns NULL: it returns "hid" for anything
         * unrecognised (the TV remote, a keyboard, the wrong composite node, ...).
         * Auto-bridge ONLY a known game controller, or we grab the first random HID
         * -> wrong inputs and the wrong polling rate. In the standalone app the user
         * picks the device; here we must select it. */
        if (kind == NULL || strcmp(kind, "hid") == 0) {
            continue;
        }
        if (plug_in_item(item)) {
            log_append("ctm glue: bridged '%s' vid=%s pid=%s (%s)",
                       item->name, item->vid, item->pid, kind);
            bridged = true;
            break;
        }
    }
    if (!bridged) {
        log_append("ctm glue: no controller bridged");
    }

    s_active = true;
    return bridged;
}

void ctm_bridge_stop(void)
{
    if (!s_active) {
        return;
    }
    release_local_sessions_on_exit();
    g_running = false;
    if (s_refresh_started) {
        pthread_join(s_refresh_thread, NULL);
        s_refresh_started = false;
    }
    if (g_stop_sniff_thread_started) {
        pthread_join(g_stop_sniff_thread, NULL);
        g_stop_sniff_thread_started = false;
    }
    log_append("ctm glue: bridge stopped");
    s_active = false;
}

bool ctm_bridge_active(void)
{
    return s_active;
}

void ctm_bridge_status(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    size_t n = 0;
    n += (size_t) snprintf(out + n, out_len - n, "Bridge: %s\n", s_active ? "active" : "inactive");
    if (n >= out_len) return;
    n += (size_t) snprintf(out + n, out_len - n, "Agent: %s\n",
                           (g_agent_online && g_agent_host[0]) ? g_agent_host : "not found");
    if (n >= out_len) return;
    n += (size_t) snprintf(out + n, out_len - n, "Bridged controllers: %d\n", g_session_count);
    for (int i = 0; i < g_session_count && n < out_len; ++i) {
        n += (size_t) snprintf(out + n, out_len - n, "  - %s [%s]\n",
                               g_sessions[i].key, g_sessions[i].busid);
    }
}
