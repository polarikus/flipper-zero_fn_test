//
// Created by Игорь Данилов on 29.08.2023.
//
#include "unit_tests/minunit.h"
#include "uart/fn_uart_i.h"
#include "fn_commands.h"
#include "fn_helpers.h"

#define MAX_BUFF_SIZE 40

UARTApp* fn_uart;

bool is_rx_cmplt = false;

size_t rx_len = 0;
uint8_t rx_buff[MAX_BUFF_SIZE];
/*
static int32_t fn_uart_test_app(void *p) {

    return 0;
};
*/
static void test_setup(void) {
    fn_uart = fn_uart_app_alloc();
    rx_buff[0] = 0;
}

static void test_teardown(void) {
    fn_uart_app_free(fn_uart);
}

static void uart_thread_cb_test(uint8_t* buff, size_t len, void* ctx) {
    UNUSED(ctx);
    is_rx_cmplt = true;
    rx_len = len;
   
   for(size_t i = 0; i < len; ++i) {
       if(len < MAX_BUFF_SIZE){
           rx_buff[i] = buff[i];
       }
   }

}

MU_TEST(fn_uart_alloc_test) {
    mu_assert_not_null(fn_uart);
    mu_assert(fn_uart->state == UARTModeIdle, "Uart state not UARTModeIdle!!!");

    mu_assert_not_null(fn_uart->thread);

    mu_assert(
        furi_thread_get_state(fn_uart->thread) == FuriThreadStateStopped,
        "FN UART thread started!");

    mu_assert_int_eq(115200, fn_uart->baudrate);
    mu_assert_int_eq(0, fn_uart->timeout);
}

MU_TEST(fn_uart_start_and_stop_thread_test) {
    fn_uart_start_thread(fn_uart);
    mu_assert(
        furi_thread_get_state(fn_uart->thread) == FuriThreadStateRunning, "FN UART not started!");
#ifndef FURI_DEBUG
    mu_assert(fn_uart->serial_id == FuriHalSerialIdUsart, "FN UART ID is not FuriHalUartIdUSART1!!!");
#else
    mu_assert(
        fn_uart->serial_id == FuriHalSerialIdLpuart, "FN UART ID is not FuriHalUartIdLPUART1!!!");
#endif
    fn_uart_stop_thread(fn_uart);
    mu_assert(
        furi_thread_get_state(fn_uart->thread) == FuriThreadStateStopped, "FN UART not stopped!");
}

MU_TEST(fn_uart_set_rx_callback_test) {
    fn_uart_set_rx_callback(fn_uart, uart_thread_cb_test, NULL);
    mu_assert_null(fn_uart->cb_ctx);
    mu_assert_pointers_eq(fn_uart->rx_cmlt_cb, uart_thread_cb_test);
}

MU_TEST(fn_uart_trx_test) {
    size_t test_answer_size =
        36; //Ожидаемый размер ответа от ФН. Для этой команды он фиксированный
    uint8_t test_fn_answer_len[] = {
        0x1F, 0x00}; //Ожидаемая Длина сообщения ФН, формат LE (СМ документацию к ФН)

    fn_uart_set_rx_callback(fn_uart, uart_thread_cb_test, NULL);
    fn_uart_start_thread(fn_uart);
    uint8_t tx_test[] = {0x04, 0x01, 0x00, FN_CMDGetFNStatus, 0xFF, 0xCD}; //Тестовый запрос в ФН

    fn_uart_trx(fn_uart, tx_test, COUNT_OF(tx_test), 800);
    while(fn_uart->state != UARTModeIdle) {
        furi_delay_ms(10); //Ждём пока завершиться приём по UART
    }
    fn_uart_stop_thread(fn_uart);
    mu_assert(is_rx_cmplt, "RX Not Done!!"); //Не отработал коллбек uart_thread_cb_test
    if(rx_len != test_answer_size) {
        printf("[fn_uart_trx_test] rx_len: %d\n", rx_len);
    }
    mu_assert(
        rx_len == test_answer_size,
        "rx_len != 36, is fn connected?"); //Коллбек отработал, но не получен ответ от ФН
    mu_assert_int_eq(rx_buff[0], 0x04); //Признак начала сообщения
    for(size_t i = 0; i < COUNT_OF(test_fn_answer_len); ++i) {
        mu_assert_int_eq(
            test_fn_answer_len[i], rx_buff[i + 1]); //Сравниваем длину пакета с ожидаемой
    }
    uint8_t crc[2];
    crc[0] = rx_buff[rx_len - 1];
    crc[1] = rx_buff[rx_len - 2];
    mu_assert(
        check_crc(byte_array_to_uint32t_LE(crc), rx_buff, 1, rx_len - 2),
        "Wrong CRC!!!"); //Проверяем целостность пакета с помощью контрольной суммы
}

MU_TEST_SUITE(fn_uart_suite) {
    MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

    MU_RUN_TEST(fn_uart_alloc_test);
    MU_RUN_TEST(fn_uart_start_and_stop_thread_test);
    MU_RUN_TEST(fn_uart_set_rx_callback_test);
    MU_RUN_TEST(fn_uart_trx_test);
}

int run_minunit_test_fn_uart() {
    MU_RUN_SUITE(fn_uart_suite);
    return MU_EXIT_CODE;
}
