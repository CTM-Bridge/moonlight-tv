#ifndef CTM_BRIDGE_GLUE_H
#define CTM_BRIDGE_GLUE_H

/* Thin moonlight-facing facade over the embedded CTM bridge core. moonlight calls
 * only these three functions; everything else (agent discovery, enumeration,
 * controller bridging, stopSniff keep-alive) stays inside the ctmbridge lib so
 * the core's headers don't leak into moonlight-lib. */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start the bridge: discover the Windows CTM agent, enumerate controllers,
 * auto-plug the first one we recognise, and run the stopSniff keep-alive.
 * Idempotent (a second call while active is a no-op). Returns true if a
 * controller was bridged. */
bool ctm_bridge_start(void);

/* Stop bridging: unplug all sessions and stop the keep-alive thread. Idempotent. */
void ctm_bridge_stop(void);

/* True while the bridge is active. */
bool ctm_bridge_active(void);

/* Write a short human-readable status (active state, agent, bridged controllers)
 * into out (NUL-terminated). For the on-stream CTM overlay panel. */
void ctm_bridge_status(char *out, size_t out_len);

/* One detected device, for the overlay's manual plug list. */
typedef struct {
    int index;      /* opaque device index; pass to ctm_bridge_plug/unplug_index */
    char name[128];
    char vid[8];
    char pid[8];
    char kind[8];   /* "ds5" / "ds4" / "xbox" / "puck" / "hid" */
    bool plugged;
} ctm_bridge_dev_t;

/* Re-enumerate and fill out[0..max-1] with the detected devices; returns the
 * count. Index i is stable for ctm_bridge_plug_index(i)/unplug_index(i) until the
 * next ctm_bridge_list() call. */
int ctm_bridge_list(ctm_bridge_dev_t *out, int max);

/* Manually plug / unplug the device at the given list index. */
bool ctm_bridge_plug_index(int index);
void ctm_bridge_unplug_index(int index);

/* Plug every recognised controller (skips already-plugged); returns count newly
 * plugged. Unplug all releases every bridged session. */
int ctm_bridge_plug_all(void);
void ctm_bridge_unplug_all(void);

/* Flat per-controller settings (mirrors the bridge's tv_bridge_worker_settings_t,
 * so moonlight doesn't need the ctmcore headers). */
typedef struct {
    int kind;                     /* 0 = hid, 4 = ds4, 5 = ds5 */
    int audio_mode;               /* 0 Auto / 1 Off / 2 Speaker / 3 Headset / 4 Both */
    int latency_ms;
    int haptics_gain_centi;
    int headset_volume_percent;
    int speaker_volume_percent;
    int ds5_patch_high, ds5_patch_low, ds5_patch2_high, ds5_patch2_low;
} ctm_bridge_settings_t;

/* Get / apply (live) the per-controller settings for the device at the index. */
bool ctm_bridge_get_settings(int index, ctm_bridge_settings_t *out);
void ctm_bridge_set_settings(int index, const ctm_bridge_settings_t *in);

#ifdef __cplusplus
}
#endif

#endif /* CTM_BRIDGE_GLUE_H */
