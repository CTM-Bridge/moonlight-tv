/* enet_stub.c — no-op ENet client for the moonlight-tv embed of the CTM bridge
 * core.
 *
 * moonlight bundles its OWN ENet (core/moonlight-common-c/enet). Linking the
 * bridge's enet_transport.c (+ its ctm_enet/lsalzman ENet) into moonlight would
 * duplicate ENet symbols. The bridge uses direct TCP anyway, so the ctmbridge
 * lib EXCLUDES enet_transport.c and satisfies the enet_client_* references with
 * these stubs. enet_client_global_init() returns failure, so controller_common.c
 * keeps c->enet == NULL and ctm_transport stays on the TCP path at runtime.
 *
 * Signatures must match ctm-bridge-test-webos/src/shared/enet_transport.h. */

#include "enet_transport.h"

int enet_client_global_init(void) { return -1; }
void enet_client_global_deinit(void) {}

ctm_enet_client_t *enet_client_create(void) { return NULL; }
void enet_client_destroy(ctm_enet_client_t *client) { (void)client; }

int enet_client_connect(ctm_enet_client_t *client, const char *host, int port,
                        unsigned int timeout_ms)
{
    (void)client; (void)host; (void)port; (void)timeout_ms;
    return -1;
}

void enet_client_disconnect(ctm_enet_client_t *client) { (void)client; }

int enet_client_connected(const ctm_enet_client_t *client) { (void)client; return 0; }

int enet_client_service(ctm_enet_client_t *client, unsigned int timeout_ms)
{
    (void)client; (void)timeout_ms;
    return 0;
}

int enet_client_send_msg(ctm_enet_client_t *client, uint16_t type, uint32_t flags,
                         uint32_t request_id, const void *payload, size_t len)
{
    (void)client; (void)type; (void)flags; (void)request_id; (void)payload; (void)len;
    return -1;
}

int enet_client_recv_msg(ctm_enet_client_t *client, ctmb_header_t *h, uint8_t **payload)
{
    (void)client; (void)h;
    if (payload) *payload = NULL;
    return 0;
}
