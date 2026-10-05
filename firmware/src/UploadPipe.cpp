#include "UploadPipe.h"

#include <cstdlib>
#include <cstring>

void UploadPipe::begin() {
    if (!MP3_UPLOAD_RING || ring_)
        return;
    ring_ = static_cast<uint8_t*>(malloc(kRing)); // lint: ok anel so com o portal aberto (BT desligado)
    if (!ring_)
        return;
    quit_ = exited_ = false;
    head_ = tail_ = 0;
    // Core 0 (Wi-Fi e lwIP) com prioridade baixa: so grava quando a rede esta ociosa.
    if (xTaskCreatePinnedToCore(taskMain, "upsd", kStackBytes, this, 2, &task_, 0) != pdPASS) {
        free(ring_); // lint: ok par do malloc acima
        ring_ = nullptr;
        task_ = nullptr;
    }
}

void UploadPipe::end() {
    if (!task_)
        return;
    waitIdle();
    quit_ = true;
    xTaskNotifyGive(task_);
    while (!exited_)
        vTaskDelay(1);
    task_ = nullptr;
    free(ring_); // lint: ok par do malloc em begin
    ring_ = nullptr;
}

void UploadPipe::taskMain(void* self) {
    static_cast<UploadPipe*>(self)->run();
}

void UploadPipe::run() {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (quit_)
            break;
        drainRing();
    }
    exited_ = true;
    vTaskDelete(nullptr);
}

// Apos um erro o sink ja se abortou: o resto do anel e descartado para o laco nao travar.
void UploadPipe::drainRing() {
    uint32_t tail = tail_.load(std::memory_order_acquire);
    uint32_t head = head_.load(std::memory_order_acquire);
    while (tail != head) {
        const uint32_t offset = tail % kRing;
        const uint32_t n = min(head - tail, kRing - offset);
        if (error_ == Error::None) {
            const Error error = sink_.write(ring_ + offset, n);
            if (error != Error::None)
                error_ = error;
        }
        tail += n;
        tail_.store(tail, std::memory_order_release);
        head = head_.load(std::memory_order_acquire);
    }
}

// Sem prazo: o sink so pode ser tocado com o anel vazio, e a task sempre termina a gravacao em curso.
UploadPipe::Error UploadPipe::waitIdle() {
    while (used() != 0)
        vTaskDelay(1);
    return error_;
}

UploadPipe::Error UploadPipe::start(fs::FS& fs, const mrm::UploadSink::Target& target) {
    waitIdle();
    error_ = Error::None;
    pushed_ = 0;
    return sink_.start(fs, target);
}

UploadPipe::Error UploadPipe::write(const uint8_t* data, size_t size) {
    if (!task_) {
        const Error error = sink_.write(data, size);
        pushed_ += error == Error::None ? static_cast<uint32_t>(size) : 0;
        return error;
    }
    while (kRing - used() < size) {
        if (error_ != Error::None)
            return error_;
        vTaskDelay(1);
    }
    if (error_ != Error::None)
        return error_;
    const uint32_t head = head_.load(std::memory_order_relaxed);
    const uint32_t offset = head % kRing;
    const size_t first = min<size_t>(size, kRing - offset);
    memcpy(ring_ + offset, data, first);
    memcpy(ring_, data + first, size - first);
    head_.store(head + static_cast<uint32_t>(size), std::memory_order_release);
    xTaskNotifyGive(task_);
    pushed_ += static_cast<uint32_t>(size);
    return Error::None;
}

UploadPipe::Error UploadPipe::finish() {
    const Error error = waitIdle();
    if (error != Error::None) {
        sink_.abort();
        return error;
    }
    return sink_.finish();
}

void UploadPipe::abort() {
    error_ = Error::Aborted; // a task so descarta o que resta no anel
    waitIdle();
    sink_.abort();
}

uint32_t UploadPipe::stackFree() const {
    return task_ ? uxTaskGetStackHighWaterMark(task_) : 0;
}
