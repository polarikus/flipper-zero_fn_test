#include "fn_test_app_i.h"
#include "fn_objects/fn_info/fn_i.h"
#ifdef FURI_DEBUG
#include "unit_tests/test_index.h"
#endif
#include <furi.h>

#define TAG "FNTest"
#define WORKER_TAG TAG "App"

static bool fn_test_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FNApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool fn_test_app_back_event_callback(void* context) {
    furi_assert(context);
    FNApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void fn_test_app_tick_event_callback(void* context) {
    furi_assert(context);
    FNApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

FNApp* fn_test_app_alloc(void) {
    FURI_LOG_D(WORKER_TAG, "Start AppAlloc");
    FNApp* app = malloc(sizeof(FNApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->view_detect = fn_test_view_detect_alloc();
    app->view_progress = fn_test_view_progress_alloc();
    app->worker = fn_worker_alloc();
    app->popup = popup_alloc();
    app->fn_info = malloc(sizeof(FNInfo));
    app->fn_info->serial_number[0] = 'N';

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);

    app->scene_manager = scene_manager_alloc(&fn_test_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, fn_test_app_tick_event_callback, 500);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fn_test_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fn_test_app_back_event_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FNTestViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, FNTestViewPopup, popup_get_view(app->popup));
    //view_dispatcher_add_view(
    //  app->view_dispatcher, FNTestViewDialogEx, dialog_ex_get_view(app->dialog_ex));
    // view_dispatcher_add_view(
    //   app->view_dispatcher, FNTestViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(app->view_dispatcher, FNTestViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(

        app->view_dispatcher, FNTestViewDetect, fn_test_view_detect_get_view(app->view_detect));
    view_dispatcher_add_view(
        app->view_dispatcher,
        FNTestViewProgress,
        fn_test_view_progress_get_view(app->view_progress));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FNAppSceneStart);
#ifdef FURI_DEBUG
    //fn_register_tests();
#endif
    FURI_LOG_D(WORKER_TAG, "End AppAlloc");
    return app;
}

void fn_test_app_free(FNApp* app) {
    furi_assert(app);
#ifdef FURI_DEBUG
    //fn_unregister_tests();
#endif
    free(app->fn_info);

    view_dispatcher_remove_view(app->view_dispatcher, FNTestViewSubmenu);
    //view_dispatcher_remove_view(app->view_dispatcher, SPIMemViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, FNTestViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, FNTestViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, FNTestViewDetect);
    view_dispatcher_remove_view(app->view_dispatcher, FNTestViewProgress);
    //view_dispatcher_remove_view(instance->view_dispatcher, SPIMemViewProgress);
    //view_dispatcher_remove_view(instance->view_dispatcher, SPIMemViewDetect);
    //view_dispatcher_remove_view(instance->view_dispatcher, SPIMemViewTextInput);
    submenu_free(app->submenu);
    widget_free(app->widget);
    fn_test_view_detect_free(app->view_detect);
    fn_test_view_progress_free(app->view_progress);

    fn_worker_free(app->worker);

    // Views
    //view_dispatcher_remove_view(app->view_dispatcher, BarCodeAppViewWork);
    //bc_scanner_free(app->bc_scanner_view);

    // Custom Widget
    //view_dispatcher_remove_view(app->view_dispatcher, BarCodeAppViewError);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    popup_free(app->popup);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);

    //furi_string_free(app->file_path);

    free(app);
    FURI_LOG_D(WORKER_TAG, "App Free");
}

int32_t fn_test_app(void* p) {
    UNUSED(p);
    FURI_LOG_D(WORKER_TAG, "Start App");
    FNApp* fn_test_app = fn_test_app_alloc();

    view_dispatcher_run(fn_test_app->view_dispatcher);

    fn_test_app_free(fn_test_app);
    return 0;
}
