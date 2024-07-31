//
// Created by Игорь Данилов on 21.08.2023.
//
#include "test_index.h"
#include <furi.h>
#include "minunit_vars.h"
#include <cli/cli.h>
#include <notification/notification_messages.h>
//#include <toolbox/args.h>

#define TAG "UnitTests"

int run_minunit_test_fn_info();
int run_minunit_test_fn_life_info();
int run_minunit_test_fn_helpers();
int run_minunit_test_fn_uart();
int run_minunit_test_fn_worker();

typedef int (*UnitTestEntry)();

typedef struct {
    const char* name;
    const UnitTestEntry entry;
} UnitTest;

const UnitTest unit_tests[] = {
    {.name = "fn_info", .entry = run_minunit_test_fn_info},
    {.name = "fn_life_info", .entry = run_minunit_test_fn_life_info},
    {.name = "fn_helpers", .entry = run_minunit_test_fn_helpers},
    {.name = "fn_uart", .entry = run_minunit_test_fn_uart},
    {.name = "fn_worker", .entry = run_minunit_test_fn_worker}

};

void minunit_print_progress() {
    UNUSED(run_minunit_test_fn_helpers);
    static const char progress[] = {'\\', '|', '/', '-'};
    static uint8_t progress_counter = 0;
    static uint32_t last_tick = 0;
    uint32_t current_tick = furi_get_tick();
    if(current_tick - last_tick > 20) {
        last_tick = current_tick;
        //printf("[%c]\033[3D", progress[++progress_counter % COUNT_OF(progress)]);
        FURI_LOG_I(TAG, "[%c]\033[3D", progress[++progress_counter % COUNT_OF(progress)]);
        fflush(stdout);
    }
}

void minunit_print_fail(const char* str) {
    //printf(_FURI_LOG_CLR_E "%s\r\n" _FURI_LOG_CLR_RESET, str);
    FURI_LOG_I(TAG, _FURI_LOG_CLR_E "%s\r\n" _FURI_LOG_CLR_RESET, str);
}

uint32_t fn_unit_tests_app(void* p) {
    UNUSED(p);
    FuriString* str = furi_string_alloc();
    fn_run_tests(str, NULL);
    furi_string_free(str);
    return 0;
}

void fn_run_tests(FuriString* args, void* context)
{
    UNUSED(args);
    UNUSED(context);
    minunit_run = 0;
    minunit_assert = 0;
    minunit_fail = 0;
    minunit_status = 0;

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message_block(notification, &sequence_set_only_blue_255);

    uint32_t heap_before = memmgr_get_free_heap();
    uint32_t cycle_counter = furi_get_tick();

    for(size_t i = 0; i < COUNT_OF(unit_tests); i++) {
        /*
        if(cli_cmd_interrupt_received(cli)) {
            break;
        }
         */

        if(furi_string_size(args)) {
            if(furi_string_cmp_str(args, unit_tests[i].name) == 0) {
                unit_tests[i].entry();
            } else {
                FURI_LOG_I(TAG, "Skipping %s reason: test not found\r\n", unit_tests[i].name);
                //printf("Skipping %s reason: test not found\r\n", unit_tests[i].name);
            }
        } else {
            unit_tests[i].entry();
        }
    }

    if(minunit_run != 0) {
        //printf("\r\nFailed tests: %u\r\n", minunit_fail);
        FURI_LOG_I(TAG, "\r\nFailed tests: %u\r\n", minunit_fail);

        // Time report
        cycle_counter = (furi_get_tick() - cycle_counter);
        //printf("Consumed: %lu ms\r\n", cycle_counter);
        FURI_LOG_I(TAG, "Consumed: %lu ms\r\n", cycle_counter);

        // Wait for tested services and apps to deallocate memory
        furi_delay_ms(200);
        uint32_t heap_after = memmgr_get_free_heap();
        //printf("Leaked: %ld\r\n", heap_before - heap_after);
        FURI_LOG_I(TAG, "Leaked: %ld\r\n", heap_before - heap_after);

        // Final Report
        if(minunit_fail == 0) {
            notification_message(notification, &sequence_success);
            //printf("Status: PASSED\r\n");
            FURI_LOG_I(TAG, "Status: PASSED\r\n");
        } else {
            notification_message(notification, &sequence_error);
            //printf("Status: FAILED\r\n");
            FURI_LOG_I(TAG, "Status: FAILED\r\n");
        }
    }
}

static void fn_test_cli(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(args);
    UNUSED(context);
}

void fn_register_tests() {
#ifdef FURI_DEBUG
    Cli* cli = furi_record_open(RECORD_CLI);
    cli_add_command(cli, "fn_test", CliCommandFlagParallelSafe, fn_test_cli, NULL);
    furi_record_close(RECORD_CLI);
#else
    UNUSED(fn_register_tests);
#endif
}

void fn_unregister_tests() {
#ifdef FURI_DEBUG
    Cli* cli = furi_record_open(RECORD_CLI);
    cli_delete_command(cli, "fn_test");
    furi_record_close(RECORD_CLI);
#else
    UNUSED(fn_unregister_tests);
    UNUSED(fn_test_cli);
#endif
}
