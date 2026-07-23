#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firefly-demos.h"
#include "firefly-hollows.h"

#include "panel-kiosk.h"

// Called by ffx_init in the app_main; this pushes a new (initial) panel
// on the panel stack.
static int initPanel(void *arg) {
    return pushPanelKiosk();
}

void app_main() {
    vTaskSetApplicationTaskTag( NULL, (void*)NULL);
    ffx_init(FFX_VERSION(0, 0, 1), ffx_demo_backgroundPixies, initPanel, NULL);

    while(1) {
        ffx_dumpStats();
        vTaskDelay(60000);
    }
}
