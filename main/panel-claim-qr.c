#include <stdio.h>
#include <string.h>

#include "firefly-hollows.h"

#include "panel-claim-qr.h"
#include "poap-claim.h"

// Auto-return to the idle kiosk after this many milliseconds so the next
// attendee starts from a clean screen.
#define CLAIM_TIMEOUT_MS   30000

typedef struct State {
    FfxScene scene;
    uint32_t elapsed;
    char uri[POAP_CLAIM_URI_MAX];
} State;

static void onKeys(FfxEvent event, FfxEventProps props, void *_st) {
    if (props.keys.down == FfxKeyCancel || props.keys.down == FfxKeyOk) {
        ffx_popPanel(0);
    }
}

static void onRender(FfxEvent event, FfxEventProps props, void *_st) {
    State *st = _st;
    st->elapsed += props.render.dt;
    if (st->elapsed >= CLAIM_TIMEOUT_MS) {
        ffx_popPanel(0);
    }
}

static int initFunc(FfxScene scene, FfxNode node, void *_st, void *initArg) {
    State *st = _st;
    st->scene = scene;
    st->elapsed = 0;
    strncpy(st->uri, (const char *)initArg, sizeof(st->uri) - 1);
    st->uri[sizeof(st->uri) - 1] = '\0';

    // Full white background for maximum scanner contrast.
    FfxNode bg = ffx_scene_createFill(scene, COLOR_WHITE);
    ffx_sceneGroup_appendChild(node, bg);

    // Green "attested" banner in the top margin (kept clear of the QR).
    FfxNode banner = ffx_scene_createLabel(scene, FfxFontSmallBold, "ATTESTED");
    ffx_sceneLabel_setAlign(banner, FfxTextAlignCenter | FfxTextAlignMiddle);
    ffx_sceneLabel_setTextColor(banner, COLOR_GREEN);
    ffx_sceneGroup_appendChild(node, banner);
    ffx_sceneNode_setPosition(banner, ffx_point(120, 12));

    // QR code, byte mode, low ECC (the payload is already long).
    FfxNode qr = ffx_scene_createQR(scene, st->uri, FfxQRCorrectionLow);

    // Size each module to fill the 240x240 display below the banner. At the
    // default module size of 1, getSize() returns the module count (incl. the
    // quiet zone); scale it up to fit within ~208px.
    uint16_t modules = ffx_sceneQR_getSize(qr);
    uint8_t moduleSize = (modules > 0) ? (uint8_t)(208 / modules) : 1;
    if (moduleSize < 1) { moduleSize = 1; }
    ffx_sceneQR_setModuleSize(qr, moduleSize);

    uint16_t px = ffx_sceneQR_getSize(qr);
    int16_t x = (240 - (int16_t)px) / 2;
    int16_t y = 24 + (216 - (int16_t)px) / 2;   // centered in the area below banner
    if (y < 24) { y = 24; }
    ffx_sceneGroup_appendChild(node, qr);
    ffx_sceneNode_setPosition(qr, ffx_point(x, y));

    ffx_onEvent(FfxEventKeys, onKeys, st);
    ffx_onEvent(FfxEventRenderScene, onRender, st);

    return 0;
}

int pushPanelClaimQR(const char *uri) {
    return ffx_pushPanel(initFunc, sizeof(State), (void *)uri);
}
