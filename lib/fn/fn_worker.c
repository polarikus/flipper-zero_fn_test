//
// Created by Игорь Данилов on 04.03.2023.
//
#include "fn_worker_i.h"
#define TAG "FN WORKER"
typedef enum {
    FNEventStopThread = (1 << 0),
    FNEventRxDone = (1 << 1),
    FNEventRxEmpty = (1 << 2),
    FNEventRxStreamError = (1 << 3),
    FNEventDetect = (1 << 4),
    FNEventGetLifeInfo = (1 << 5),
    FNEventFlashMGM = (1 << 6),
    FNEventAll =
        (FNEventStopThread | FNEventRxDone | FNEventRxEmpty| FNEventRxStreamError | FNEventDetect | FNEventGetLifeInfo
         | FNEventFlashMGM),
    FNEventRxAll =
        (FNEventRxDone | FNEventRxEmpty | FNEventRxStreamError)
} SPIMemEventEventType;

static int32_t fn_worker_thread(void* thread_context);

void uart_thread_cb(uint8_t* buff, size_t len ,void * ctx)
{
    FURI_LOG_I(TAG, "uart_thread_cb, len=%d", len);
    FNWorker* app = ctx;
    FuriThreadId thread_id = furi_thread_get_id(app->thread);
    FuriStatus status = furi_stream_buffer_reset(app->stream);
    FURI_LOG_I(TAG, "uart_thread_cb, status=%d", status);

    if(len > 0)
    {
        size_t send_len = furi_stream_buffer_send(app->stream, buff, len, FuriWaitForever);
        FURI_LOG_I(TAG, "uart_thread_cb, send_len=%d", send_len);
        if(send_len == len && status == FuriStatusOk)
        {
            furi_thread_flags_set(thread_id, FNEventRxDone);
        } else {
            furi_thread_flags_set(thread_id, FNEventRxStreamError);
        }
    } else {
        furi_thread_flags_set(thread_id, FNEventRxEmpty);
    }
}


FNWorker* fn_worker_alloc() {
    FNWorker* worker = malloc(sizeof(FNWorker));
    worker->callback = NULL;
    worker->thread = furi_thread_alloc();
    worker->mode_index = FNWorkerModeIdle;
    worker->uart = NULL;
    worker->stream = furi_stream_buffer_alloc(FN_UART_MAX_PACKAGE_LEN, 1);
    furi_thread_set_name(worker->thread, "FNWorker");
    furi_thread_set_callback(worker->thread, fn_worker_thread);
    furi_thread_set_context(worker->thread, worker);
    furi_thread_set_stack_size(worker->thread, 10240);
    return worker;
}

void fn_worker_free(FNWorker* worker) {
    if(worker->uart && worker->uart->thread){
        if(furi_thread_get_state(worker->uart->thread) != FuriThreadStateStopped)
        {
            fn_uart_stop_thread(worker->uart);
        }
    }
    if(!fn_worker_check_for_stop(worker)){
        fn_worker_stop_thread(worker);
    }
    furi_thread_free(worker->thread);
    
    furi_stream_buffer_free(worker->stream);
    free(worker);
}

bool fn_worker_check_for_stop(FNWorker* worker) {
    UNUSED(worker);
    uint32_t flags = furi_thread_flags_get();
    return (flags & FNEventStopThread);
}

RxStatus fn_worker_wait_uart_cb(FNWorker* worker, uint32_t timeout) {
    UNUSED(worker);
    uint32_t flags = furi_thread_flags_wait(FNEventRxAll, FuriFlagWaitAny, timeout);
    FURI_LOG_D(TAG, "fn_worker_wait_uart_cb, FLAGS=%ld", flags);
    if(flags == (uint32_t)FuriStatusErrorTimeout) return FNRxTimeout;

    if(flags & FNEventRxDone) return FNRxStatusOk;
    if(flags & FNEventRxEmpty) return FNRxEmpty;
    if(flags & FNEventRxStreamError) return FNRxStreamError;

    return FNRxTimeout;
}

static int32_t fn_worker_thread(void* thread_context) {
    FNWorker* worker = thread_context;

    while(true) {
        uint32_t flags = furi_thread_flags_wait(FNEventAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags != (unsigned)FuriFlagErrorTimeout) {
            if(flags & FNEventStopThread) break;
            if(flags & FNEventDetect) worker->mode_index = FNWorkerModeFNDetect;
            if(flags & FNEventGetLifeInfo) worker->mode_index = FNWorkerModeGetLifeInfo;
            if(flags & FNEventFlashMGM) worker->mode_index = FNWorkerModeFlashMGM;
            if(fn_worker_modes[worker->mode_index].process) {
                fn_worker_modes[worker->mode_index].process(worker);
            }
            worker->mode_index = FNWorkerModeIdle;
        }
        /*
        if(flags != (unsigned)FuriFlagErrorTimeout) {
            if(flags & SPIMemEventStopThread) break;
            if(flags & SPIMemEventChipDetect) worker->mode_index = SPIMemWorkerModeChipDetect;
            if(flags & SPIMemEventRead) worker->mode_index = SPIMemWorkerModeRead;
            if(flags & SPIMemEventVerify) worker->mode_index = SPIMemWorkerModeVerify;
            if(flags & SPIMemEventErase) worker->mode_index = SPIMemWorkerModeErase;
            if(flags & SPIMemEventWrite) worker->mode_index = SPIMemWorkerModeWrite;
            if(spi_mem_worker_modes[worker->mode_index].process) {
                spi_mem_worker_modes[worker->mode_index].process(worker);
            }
            worker->mode_index = SPIMemWorkerModeIdle;
            */
    }
    return 0;
}

void fn_worker_start_thread(FNWorker * worker) {
    worker->uart = fn_uart_app_alloc();
    fn_uart_set_rx_callback(worker->uart, uart_thread_cb, worker);
    furi_thread_start(worker->thread);
}

void fn_worker_stop_thread(FNWorker * worker) {
    furi_thread_flags_set(furi_thread_get_id(worker->thread), FNEventStopThread);
    furi_thread_join(worker->thread);
    if (worker->uart)
    {
        fn_uart_app_free(worker->uart);
    }
}

void fn_worker_fn_detect_start(
    FNInfo* fn_info,
    FNWorker* worker,
    FNWorkerCallback callback,
    void* context)
{
    furi_check(worker->mode_index == FNWorkerModeIdle, TAG" fn_worker_fn_detect_start");
    worker->callback = callback;
    worker->cb_ctx = context;
    worker->fn_info = fn_info;
    furi_thread_flags_set(furi_thread_get_id(worker->thread), FNEventDetect);
}

void fn_worker_get_life_info_start(
    FNLifeInfo* fn_life_info,
    FNWorker* worker,
    FNWorkerCallback callback,
    void* context)
{
    furi_check(worker->mode_index == FNWorkerModeIdle, TAG" fn_worker_get_life_info_start");
    worker->callback = callback;
    worker->cb_ctx = context;
    worker->fn_answer_data = fn_life_info;
    furi_thread_flags_set(furi_thread_get_id(worker->thread), FNEventGetLifeInfo);
}

void fn_worker_flash_MGM_start(
    FNWorker* worker,
    FNWorkerCallback callback,
    void* context)
{
    furi_check(worker->mode_index == FNWorkerModeIdle, TAG" fn_worker_flash_MGM_start");
    worker->callback = callback;
    worker->cb_ctx = context;

    furi_thread_flags_set(furi_thread_get_id(worker->thread), FNEventFlashMGM);
}
