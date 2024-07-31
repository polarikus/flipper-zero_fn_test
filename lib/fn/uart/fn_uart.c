//
// Created by Игорь Данилов on 28.02.2023.
//
#include "fn_uart_i.h"
#define TAG "FNUartBus"
#include <cli/cli_vcp.h>
#include <cli/cli.h>


typedef enum {
    UARTThreadEventStop = (1 << 0),
    UARTThreadEventRxStart = (1 << 1),
    UARTThreadEventRxDone = (1 << 2),
    UARTThreadEventAll = UARTThreadEventStop | UARTThreadEventRxStart | UARTThreadEventRxDone
} UARTThreadEvent;


typedef struct {
    FuriStreamBuffer* rx_stream;
    FuriThreadId thread_id;
    FuriHalSerialId serial_id;
} UARTProcess;

static void usb_uart_on_irq_rx_dma_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent ev,
    size_t size,
    void* context) {
    UARTProcess* uart_process = (UARTProcess*)context;

    if(ev & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[FURI_HAL_SERIAL_DMA_BUFFER_SIZE] = {0};
        while(size) {
            size_t ret = furi_hal_serial_dma_rx(
                handle,
                data,
                (size > FURI_HAL_SERIAL_DMA_BUFFER_SIZE) ? FURI_HAL_SERIAL_DMA_BUFFER_SIZE : size);
            furi_stream_buffer_send(uart_process->rx_stream, data, ret, 0);
            size -= ret;
        };
        furi_thread_flags_set(furi_thread_get_id(uart_process->thread_id), UARTThreadEventRxDone);
    }
}

static void timer_timeout(void* ctx) {
    furi_check(ctx, TAG" timer_timeout | ctx");
    UARTProcess* app = ctx;
    FURI_LOG_D(TAG, "TIMEOUT_CB");
    furi_thread_flags_set(app->thread_id, UARTThreadEventRxDone);
}

static void console_disable() {
    Cli* cli = furi_record_open(RECORD_CLI);
    cli_session_close(cli);
    furi_record_close(RECORD_CLI);
}

static void console_enable() {
    Cli* cli = furi_record_open(RECORD_CLI);
    cli_session_open(cli, &cli_vcp);
    furi_record_close(RECORD_CLI);
}

static void fn_init_uart(uint32_t baudrate, UARTApp* app) {
#ifdef FURI_DEBUG
    FuriHalSerialId serial_id = FuriHalSerialIdLpuart;
    UNUSED(console_disable);
#else
    FuriHalSerialId serial_id = FuriHalSerialIdUsart;
    console_disable();
#endif
    furi_check(baudrate > 0, TAG" baudrate > 0");

    furi_check(!app->serial_handle, TAG" !uart_process->serial_handle");
    app->serial_handle = furi_hal_serial_control_acquire(serial_id);
    furi_check(app->serial_handle, TAG" uart_process->serial_handle");

    furi_hal_serial_init(app->serial_handle, baudrate);
}

static void fn_deinit_uart(UARTApp* uart_app)
{
    if(uart_app->serial_handle){
        furi_hal_serial_deinit(uart_app->serial_handle);
        furi_hal_serial_control_release(uart_app->serial_handle);
    }
    uart_app->serial_handle = NULL;
    console_enable();
}

static int32_t uart_process(void* p) {
    UARTApp* uart_app = p;
    FURI_LOG_D(TAG, "START TASK");

    UARTProcess* app = malloc(sizeof(UARTProcess));

    app->thread_id = furi_thread_get_id(furi_thread_get_current());
    app->rx_stream = furi_stream_buffer_alloc(FN_UART_MAX_PACKAGE_LEN, 1);


    furi_hal_serial_dma_rx_start(uart_app->serial_handle,
                                 usb_uart_on_irq_rx_dma_cb, app, true);

    const uint16_t rx_buffer_size = FN_UART_MAX_PACKAGE_LEN;
    uint8_t* rx_buffer = malloc(rx_buffer_size);
    size_t data_len = 0;


    FuriTimer* timer = furi_timer_alloc(timer_timeout, FuriTimerTypePeriodic, app);
    FuriLogLevel old_log_level = furi_log_get_level();//Сохраняем текущие настройки логов
    uint32_t events;
    while(1) {
        events = furi_thread_flags_wait(UARTThreadEventAll, FuriFlagWaitAny, FuriWaitForever);
        //FURI_LOG_D(TAG, "FLAG %ld", events);
        if(events & UARTThreadEventStop) break;
        if(events & UARTThreadEventRxStart)
        {
            furi_log_set_level(FuriLogLevelNone);//Из-за логов теряются байты, поэтому отключаем
            furi_timer_start(timer, furi_ms_to_ticks(uart_app->timeout));
        }
        if(events & UARTThreadEventRxDone){
            furi_log_set_level(old_log_level);//Возвращаем логи
            furi_timer_stop(timer);
            data_len = furi_stream_buffer_receive(app->rx_stream, rx_buffer, rx_buffer_size, uart_app->timeout);
            uart_app->state = UARTModeRxDone;
            if(uart_app->rx_cmlt_cb) uart_app->rx_cmlt_cb(rx_buffer, data_len, uart_app->cb_ctx);
            furi_stream_buffer_reset(app->rx_stream);
            uart_app->state = UARTModeIdle;
        }
        //TODO Ускорить UART.Получаем первые 3 байта, там есть длина. Далее ждём, пока стрим не заполнится на эту длину + CRC. Если заполнился - объеденяем буфер
    }

    furi_hal_serial_dma_rx_stop(uart_app->serial_handle);
    free(rx_buffer);
    furi_timer_free(timer);
    furi_stream_buffer_free(app->rx_stream);
    free(app);
    FURI_LOG_D(TAG, "END TASK");
    return 0;
}

UARTApp* fn_uart_app_alloc() {
    FURI_LOG_D(TAG, "UART ALLOC");
    UARTApp* uart_app = malloc(sizeof(UARTApp));
    uart_app->timeout = 0;
    uart_app->baudrate = 115200;
#ifndef FURI_DEBUG
    uart_app->serial_id = FuriHalSerialIdUsart;
#else
    uart_app->serial_id = FuriHalSerialIdLpuart;
#endif
    fn_init_uart(uart_app->baudrate, uart_app);

    uart_app->thread = furi_thread_alloc_ex("FN UART thread", 1 * 1024, uart_process, uart_app);
    uart_app->state = UARTModeIdle;
    furi_thread_set_priority(uart_app->thread, FuriThreadPriorityIsr);
    return uart_app;
}

void fn_uart_start_thread(UARTApp* app)
{
    furi_check(app, TAG" app null");
    furi_check(app->thread, TAG" app->thread");
    furi_check(furi_thread_get_state(app->thread) != FuriThreadStateRunning, TAG" Thread nr");
    furi_thread_start(app->thread);
}

void fn_uart_stop_thread(UARTApp* app)
{
    furi_check(app, TAG" fn_uart_stop_thread | app");
    furi_check(app->thread, TAG" fn_uart_stop_thread | app->thread");
    furi_thread_flags_set(furi_thread_get_id(app->thread), UARTThreadEventStop);
    furi_thread_join(app->thread);
    FURI_LOG_D(TAG, "uart thread stopped");
}

static bool fn_uart_check_for_stop(UARTApp* app) {
    UNUSED(app);
    uint32_t flags = furi_thread_flags_get();
    return (flags & UARTThreadEventStop);
}

void fn_uart_app_free(UARTApp* app) {
    UNUSED(fn_uart_check_for_stop);
    furi_check(app, TAG" fn_uart_app_free | app");
    fn_deinit_uart(app);
    if(app->thread){
        fn_uart_stop_thread(app);
        furi_thread_free(app->thread);
    }
    free(app);
}

bool fn_uart_trx(UARTApp* app, uint8_t* tx_buff, size_t tx_buff_size, uint32_t timeout) {
    FURI_LOG_D(TAG, "START TRX");
    furi_check(app, TAG" fn_uart_trx | app");
    furi_check(tx_buff_size > 0, TAG"fn_uart_trx | tx_buff_size");
    furi_check(timeout > 0, TAG" fn_uart_trx | timeout");
    furi_check(app->serial_handle, TAG" fn_uart_trx | app->serial_handle");
    bool result = true;
    FURI_LOG_D(TAG, "tx_buff[3] = %x", tx_buff[3]);
    if(app->state != UARTModeIdle)
    {
        FURI_LOG_D(TAG, "!UARTModeIdle. MODE=%d", app->state);
        return false;
    }
    if(timeout != app->timeout) app->timeout = timeout;

    app->state = UARTModeTx;
    if(furi_thread_flags_set(furi_thread_get_id(app->thread), UARTThreadEventRxStart) != FuriStatusOk ) result = false;
    FURI_LOG_D(TAG, "Start tx");
    furi_hal_serial_tx(app->serial_handle, tx_buff, tx_buff_size);
    FURI_LOG_D(TAG, "Stop tx");
    app->state = UARTModeRx;
    FURI_LOG_D(TAG, "END TRX %d", result);

    return result;
}

void fn_uart_set_rx_callback(UARTApp* app, void (*rx_cb)(uint8_t* buf, size_t len, void* context), void* context) {
    furi_check(app, TAG" fn_uart_set_rx_callback | app ");
    app->rx_cmlt_cb = rx_cb;
    app->cb_ctx = context;
}
