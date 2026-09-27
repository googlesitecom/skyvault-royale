// ============================================================================
//  SkyVault Engine - implementacion del Job System
//
//  Diseno: fork/join con dispensador de tickets atomicos.
//  - parallelFor(count, batch, fn, user) divide [0,count) en lotes de 'batch'.
//  - El hilo llamador TAMBIEN ejecuta trabajo mientras espera (ayuda).
//  - Los Groups se reciclan desde un pool y solo se liberan en shutdown
//    (evita use-after-free con workers que aun sostengan el puntero).
// ============================================================================
#include "threading/JobSystem.h"

#if !defined(SKYVAULT_WEB)
#include <algorithm>

namespace sv {

thread_local u32 JobSystem::tlsThreadIndex = 0;

void JobSystem::init(i32 workerCount) {
    if (running.exchange(true)) return;
    i32 count = workerCount;
    if (count <= 0) {
        const unsigned hc = std::thread::hardware_concurrency();
        count = hc > 1 ? static_cast<i32>(hc) - 1 : 1;
    }
    count = std::max(count, 1);
    for (i32 i = 0; i < count; ++i)
        workers.emplace_back(&JobSystem::workerLoop, this, i + 1);
}

void JobSystem::shutdown() {
    if (!running.exchange(false)) return;
    queueCv.notify_all();
    for (auto& t : workers)
        if (t.joinable()) t.join();
    workers.clear();
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.clear();
        pool.clear();
        freeIndices.clear();
    }
}

u32 JobSystem::threadIndex() { return tlsThreadIndex; }

JobSystem::Group* JobSystem::acquireGroup() {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (!freeIndices.empty()) {
        const usize idx = freeIndices.back();
        freeIndices.pop_back();
        return pool[idx].get();
    }
    pool.push_back(std::make_unique<Group>());
    return pool.back().get();
}

void JobSystem::releaseGroup(Group* g) {
    std::lock_guard<std::mutex> lock(queueMutex);
    for (usize i = 0; i < pool.size(); ++i)
        if (pool[i].get() == g) { freeIndices.push_back(i); return; }
}

void JobSystem::runTickets(JobSystem::Group* g) {
    for (;;) {
        const u32 start = g->next.fetch_add(g->batch);
        if (start >= g->count) return;
        const u32 end = std::min(start + g->batch, g->count);
        for (u32 i = start; i < end; ++i) g->fn(g->user, i);
        if (g->remaining.fetch_sub(1) == 1) {
            // lock/deslock antes de notify: garantiza visibilidad del predicado
            { std::lock_guard<std::mutex> lk(queueMutex); }
            doneCv.notify_all();
        }
    }
}

void JobSystem::parallelFor(u32 count, u32 batch, JobFn fn, void* user) {
    if (count == 0) return;
    batch = std::max(batch, 1u);

    if (!running.load() || workers.empty() || count <= batch) {
        for (u32 i = 0; i < count; ++i) fn(user, i);
        return;
    }

    Group* g = acquireGroup();
    g->fn = fn;
    g->user = user;
    g->count = count;
    g->batch = batch;
    g->next.store(0);
    g->remaining.store((count + batch - 1) / batch);

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push_back(g);
    }
    queueCv.notify_all();

    // El hilo llamador ayuda mientras espera
    runTickets(g);

    {
        std::unique_lock<std::mutex> lock(queueMutex);
        doneCv.wait(lock, [&] { return g->remaining.load() == 0; });
        for (usize i = 0; i < queue.size(); ++i)
            if (queue[i] == g) { queue.erase(queue.begin() + static_cast<isize>(i)); break; }
    }
    releaseGroup(g);
}

void JobSystem::parallelForLambda(u32 count, const std::function<void(u32)>& fn) {
    struct Ctx { const std::function<void(u32)>* fn; };
    Ctx ctx{&fn};
    parallelFor(count, 256, [](void* user, u32 index) {
        const auto* c = static_cast<Ctx*>(user);
        (*c->fn)(index);
    }, &ctx);
}

void JobSystem::workerLoop(i32 index) {
    tlsThreadIndex = static_cast<u32>(index);
    for (;;) {
        Group* g = nullptr;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCv.wait(lock, [&] { return !running.load() || !queue.empty(); });
            if (!running.load() && queue.empty()) return;
            if (!queue.empty()) g = queue.front();
        }
        if (!g) continue;

        runTickets(g);
        // grupo drenado: el dueno lo retirara de la cola; vuelve a esperar
        std::this_thread::yield();
    }
}

} // namespace sv
#endif // !SKYVAULT_WEB
