#pragma once

#include <Arduino.h>
#include <UploadSink.h>

#include <atomic>

// -DMP3_UPLOAD_RING=1 liga o anel. Desligado por padrao: no aparelho, com o anel, o ESP reiniciou (POWERON_RESET, sem panico)
// em varios envios seguidos; sem ele 3 envios de 5 MB passaram. Suspeita: pico de corrente do SD junto com o TX do Wi-Fi.
#ifndef MP3_UPLOAD_RING
#define MP3_UPLOAD_RING 0
#endif

// Desacopla a rede do SD: write() copia a fatia num anel e uma task grava no UploadSink, entao o
// WebServer volta a esvaziar o socket (janela TCP aberta) enquanto o cartao grava. Mesma API do sink;
// os erros da gravacao aparecem no write() seguinte ou no finish(). O sink so e tocado pela task, e
// start/finish/abort esperam o anel esvaziar antes de toca-lo. Sem memoria para o anel, grava direto.
class UploadPipe {
public:
    using Error = mrm::UploadSink::Error;

    explicit UploadPipe(const mrm::UploadSink::Config& config)
        : sink_(config) {}

    void begin(); // portal abriu: aloca anel e task
    void end();   // portal fechou: devolve a memoria

    Error start(fs::FS& fs, const mrm::UploadSink::Target& target);
    Error write(const uint8_t* data, size_t size);
    Error finish();
    void abort();

    uint32_t written() const { return pushed_; } // aceito no anel; o SD pode estar ate 16 KB atras
    uint32_t stackFree() const;

private:
    static constexpr uint32_t kRing = 16384; // potencia de 2: os contadores correm livres
    static constexpr uint32_t kStackBytes = 5120;

    static void taskMain(void* self);
    void run();
    void drainRing();
    Error waitIdle();
    uint32_t used() const { return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire); }

    mrm::UploadSink sink_;
    uint8_t* ring_ = nullptr;
    TaskHandle_t task_ = nullptr;
    std::atomic<uint32_t> head_{0}, tail_{0}; // head: loop, tail: task
    std::atomic<Error> error_{Error::None};
    std::atomic<bool> quit_{false}, exited_{false};
    uint32_t pushed_ = 0;
};
