// ============================================================================
//  SkyVault Engine - Job System: pool de hilos con fork/join y parallelFor
//  Usado para: generacion de meshes de terreno, vegetacion y streaming.
//  En WebAssembly se ejecuta en serie (sin hilos) con la misma API.
// ============================================================================
#pragma once

#include "core/Core.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace sv {

#if !defined(SKYVAULT_WEB)
class JobSystem {
public:
    using JobFn = void (*)(void* user, u32 index);

    static JobSystem& get() {
        static JobSystem inst;
        return inst;
    }

    void init(i32 workerCount = -1);  // -1 = nproc-1 (min 1)
    void shutdown();
    [[nodiscard]] bool isRunning() const { return running.load(); }
    [[nodiscard]] i32 workerCount() const { return static_cast<i32>(workers.size()); }
    [[nodiscard]] u32 threadIndex(); // 0 = hilo principal, 1..N = workers

    // Ejecuta count trabajos en lotes y espera a que todos terminen (fork/join)
    void parallelFor(u32 count, u32 batch, JobFn fn, void* user);
    // Version con lambda (sin estado capturado peligroso: debe ser thread-safe)
    void parallelForLambda(u32 count, const std::function<void(u32)>& fn);

private:
    struct Group {
        std::atomic<u32> next{0};      // dispensador de tickets
        std::atomic<u32> remaining{0}; // lotes pendientes de completar
        u32 count = 0;
        u32 batch = 1;
        JobFn fn = nullptr;
        void* user = nullptr;
    };

    void runTickets(Group* g);
    void workerLoop(i32 index);
    [[nodiscard]] Group* acquireGroup();
    void releaseGroup(Group* g);

    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<Group>> pool;
    std::vector<usize> freeIndices;
    std::vector<Group*> queue;
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::condition_variable doneCv;
    std::atomic<bool> running{false};
    static thread_local u32 tlsThreadIndex;
};

#else
// Version serial para WebAssembly (misma API)
class JobSystem {
public:
    using JobFn = void (*)(void* user, u32 index);
    static JobSystem& get() { static JobSystem inst; return inst; }
    void init(i32 = -1) {}
    void shutdown() {}
    [[nodiscard]] bool isRunning() const { return true; }
    [[nodiscard]] i32 workerCount() const { return 1; }
    [[nodiscard]] u32 threadIndex() const { return 0; }
    void parallelFor(u32 count, u32, JobFn fn, void* user) {
        for (u32 i = 0; i < count; ++i) fn(user, i);
    }
    void parallelForLambda(u32 count, const std::function<void(u32)>& fn) {
        for (u32 i = 0; i < count; ++i) fn(i);
    }
};
#endif

} // namespace sv
