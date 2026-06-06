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

#ifdef __cplusplus
}
#endif

#endif /* CTM_BRIDGE_GLUE_H */
