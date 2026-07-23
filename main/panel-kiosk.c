#include <stdio.h>

#include "firefly-hollows.h"

#include "panel-kiosk.h"
#include "panel-claim-qr.h"
#include "poap-claim.h"
#include "poap-config.h"

typedef struct State {
    FfxScene scene;
} State;

// A single static buffer for the current claim URI. Only one claim is displayed
// at a time, and it is copied again inside the QR panel, so this is safe whether
// ffx_pushPanel runs its init synchronously or defers it.
static char s_claimUri[POAP_CLAIM_URI_MAX];

static void issueClaim(void) {
    size_t n = poap_makeClaimURI(s_claimUri, sizeof(s_claimUri), NULL);
    if (n == 0) {
        printf("poap: failed to build claim\n");
        return;
    }
    printf("poap: issued claim (%u bytes) %s\n", (unsigned)n, s_claimUri);
    pushPanelClaimQR(s_claimUri);
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_app) {
    // Pressing OK issues a fresh single-use attestation. Repeated presses are
    // naturally debounced: the QR panel takes over as the active panel until
    // dismissed, so the attendee returns here before issuing again.
    if (props.keys.down == FfxKeyOk) {
        issueClaim();
    }
}

static int initFunc(FfxScene scene, FfxNode node, void *_app, void *arg) {
    State *app = _app;
    app->scene = scene;

    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 150));
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(node, box);
    ffx_sceneNode_setPosition(box, ffx_point(20, 45));

    FfxNode title = ffx_scene_createLabel(scene, FfxFontLargeBold, "Firefly POAP");
    ffx_sceneLabel_setAlign(title, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setTextColor(title, COLOR_GREEN);
    ffx_sceneGroup_appendChild(node, title);
    ffx_sceneNode_setPosition(title, ffx_point(120, 85));

    FfxNode ev = ffx_scene_createLabel(scene, FfxFontMedium, POAP_EVENT_TITLE);
    ffx_sceneLabel_setAlign(ev, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneGroup_appendChild(node, ev);
    ffx_sceneNode_setPosition(ev, ffx_point(120, 120));

    FfxNode hint = ffx_scene_createLabel(scene, FfxFontSmall, "Press OK to claim");
    ffx_sceneLabel_setAlign(hint, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneGroup_appendChild(node, hint);
    ffx_sceneNode_setPosition(hint, ffx_point(120, 165));

    // Log attestor address + fixed cross-check vector once at startup.
    poap_debugDump();

    ffx_onEvent(FfxEventKeys, onKeys, app);

    return 0;
}

int pushPanelKiosk(void) {
    return ffx_pushPanel(initFunc, sizeof(State), NULL);
}
