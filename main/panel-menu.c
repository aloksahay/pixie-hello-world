#include <stdio.h>

#include "firefly-hollows.h"

#include "panel-menu.h"
#include "panel-text.h"

#include "images/image-arrow.h"


typedef struct State {
    size_t cursor;
    FfxScene scene;
    FfxNode nodeCursor;
} State;


static void onKeys(FfxEvent event, FfxEventProps props, void *_app) {
    State *app = _app;


    switch(props.keys.down) {
        case FfxKeyOk: {
            uint32_t result = 0;
            switch(app->cursor) {
                case 0:
                    result = pushPanelText("Hello");
                    break;
                case 1:
                    result = pushPanelText("World");
                    break;
                case 2:
                    printf("You selected 'console'\n");
                    break;
            }
            printf("RESULT-men: %lu\n", result);
            return;
        }

        case FfxKeyNorth:
            if (app->cursor == 0) { return; }
            app->cursor--;
            break;

        case FfxKeySouth:
            if (app->cursor == 2) { return; }
            app->cursor++;
            break;

        default:
            return;
    }

    ffx_sceneNode_stopAnimations(app->nodeCursor, FfxSceneActionStopCurrent);
    ffx_sceneNode_animatePosition(app->nodeCursor,
      ffx_point(25, 58 + (app->cursor * 40)), 0, 150,
      FfxCurveEaseOutQuad, NULL, NULL);
}

static int initFunc(FfxScene scene, FfxNode node, void *_app, void *arg) {
    State *app = _app;
    app->scene = scene;

    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 180));
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(node, box);
    ffx_sceneNode_setPosition(box, (FfxPoint){ .x = 20, .y = 30 });

    FfxNode text;

    text = ffx_scene_createLabel(scene, FfxFontLarge, "Hello...");
    ffx_sceneGroup_appendChild(node, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 70, .y = 63 });


    text = ffx_scene_createLabel(scene, FfxFontLarge, "World!");
    ffx_sceneGroup_appendChild(node, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 70, .y = 103 });

    text = ffx_scene_createLabel(scene, FfxFontLarge, "Console");
    ffx_sceneGroup_appendChild(node, text);
    ffx_sceneNode_setPosition(text, (FfxPoint){ .x = 70, .y = 143 });

    FfxNode cursor = ffx_scene_createImage(scene, image_arrow,
      sizeof(image_arrow));
    ffx_sceneGroup_appendChild(node, cursor);
    ffx_sceneNode_setPosition(cursor, (FfxPoint){ .x = 25, .y = 58 });

    app->nodeCursor = cursor;

    ffx_onEvent(FfxEventKeys, onKeys, app);

    return 0;
}

int pushPanelMenu() {
    return ffx_pushPanel(initFunc, sizeof(State), NULL);
}
