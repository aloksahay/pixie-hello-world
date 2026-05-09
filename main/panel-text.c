#include "firefly-hollows.h"

#include "panel-text.h"

static void onKeys(FfxEvent event, FfxEventProps props, void *_app) {
    if (props.keys.changed && props.keys.down) {
        ffx_popPanel(0);
    }
}

static int initFunc(FfxScene scene, FfxNode node, void *_app, void *arg) {
    const char* str = arg;

    FfxNode text = ffx_scene_createLabel(scene, FfxFontLarge, str);
    ffx_sceneGroup_appendChild(node, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 120, .y = 110 });
    ffx_sceneLabel_setAlign(text, FfxTextAlignCenter);

    ffx_onEvent(FfxEventKeys, onKeys, NULL);

    return 0;
}

int pushPanelText(const char* text) {
    return ffx_pushPanel(initFunc, 0, (void*)text);
}


