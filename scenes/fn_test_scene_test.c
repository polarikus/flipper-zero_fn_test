//
// Created by Игорь Данилов on 08.03.2023.
//
#include "../fn_test_app_i.h"
#include "../lib/fn/fn_helpers.h"
#include "unit_tests/test_index.h"

void fn_test_scene_test_on_enter(void* context) {
    FNApp* app = context;


    FuriString* string = furi_string_alloc();
    fn_run_tests(string, context);
    uint16_t crc = 0xFEDD;
    uint8_t crc2[2];
    uint16t_LE_to_uint8t_bytes(crc, crc2);
    furi_string_printf(string, "%x", crc2[1]);

    widget_add_string_element(
        app->widget, 40, 12, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(string));
    furi_string_free(string);

    view_dispatcher_switch_to_view(app->view_dispatcher, FNTestViewWidget);
}

bool fn_test_scene_test_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void fn_test_scene_test_on_exit(void* context) {
    FNApp* app = context;
    widget_reset(app->widget);
}