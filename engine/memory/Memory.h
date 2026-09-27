// ============================================================================
//  SkyVault Engine - Gestion de memoria: arenas lineales, pools, frame arena
//  Usados en rutas calientes: generacion de chunks, particulas, streaming.
// ============================================================================
#pragma once

#include "core/Core.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <new>

namespace sv {

// Arena lineal (bump allocator) con almacenamiento propio
class LinearArena {
public:
    LinearArena() = default;
    explicit LinearArena(usize bytes) { init(bytes); }
    ~LinearArena() { shutdown(); }
    SV_NO_COPY(LinearArena);

    void init(usize bytes) {
        shutdown();
        capacity = bytes;
        base = static_cast<u8*>(std::malloc(bytes ? bytes : 1));
        SV_ASSERT(base && "malloc fallo en LinearArena");
        offset = 0;
    }
    void shutdown() {
        std::free(base);
        base = nullptr;
        offset = capacity = 0;
    }
    void reset() { offset = 0; }
    [[nodiscard]] usize used() const { return offset; }
    [[nodiscard]] usize total() const { return capacity; }

    [[nodiscard]] void* alloc(usize size, usize align = 16) {
        const usize mask = align - 1;
        const usize aligned = (offset + mask) & ~mask;
        if (aligned + size > capacity) return nullptr; // sin excepciones: el llamador decide
        offset = aligned + size;
        return base + aligned;
    }
    template <class T>
    [[nodiscard]] T* allocArray(usize count) {
        return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }

private:
    u8*    base = nullptr;
    usize  offset = 0;
    usize  capacity = 0;
};

// Pool de bloques de tamano fijo (free-list intrusiva)
class PoolAllocator {
public:
    PoolAllocator() = default;
    ~PoolAllocator() { shutdown(); }
    SV_NO_COPY(PoolAllocator);

    void init(usize blockSize_, usize blockCount_) {
        shutdown();
        blockSize = std::max(blockSize_, sizeof(void*));
        const usize total = blockSize * blockCount_;
        base = static_cast<u8*>(std::malloc(total ? total : 1));
        SV_ASSERT(base && "malloc fallo en PoolAllocator");
        freeList = nullptr;
        for (usize i = blockCount_; i > 0; --i) {
            u8* block = base + (i - 1) * blockSize;
            *reinterpret_cast<void**>(block) = freeList;
            freeList = block;
        }
        liveBlocks = 0;
        maxBlocks = blockCount_;
    }
    void shutdown() {
        std::free(base);
        base = nullptr;
        freeList = nullptr;
    }
    [[nodiscard]] void* obtain() {
        if (!freeList) return nullptr;
        u8* block = static_cast<u8*>(freeList);
        freeList = *reinterpret_cast<void**>(block);
        ++liveBlocks;
        return block;
    }
    void release(void* p) {
        if (!p || !base) return;
        SV_ASSERT(static_cast<u8*>(p) >= base && static_cast<u8*>(p) < base + blockSize * maxBlocks);
        *reinterpret_cast<void**>(p) = freeList;
        freeList = p;
        --liveBlocks;
    }
    [[nodiscard]] usize live() const { return liveBlocks; }
    template <class T>
    [[nodiscard]] T* create() {
        static_assert(sizeof(T) <= 4096, "usa un pool mas grande o otro allocator");
        void* mem = obtain();
        return mem ? new (mem) T() : nullptr;
    }
    template <class T>
    void destroy(T* obj) {
        if (!obj) return;
        obj->~T();
        release(obj);
    }

private:
    u8*   base = nullptr;
    void* freeList = nullptr;
    usize blockSize = 0;
    usize liveBlocks = 0;
    usize maxBlocks = 0;
};

// Frame arena de doble buffer: cada frame alterna; el hilo de render puede
// leer el buffer del frame anterior mientras la logica escribe el actual.
class FrameArena {
public:
    void init(usize bytesPerBuffer) {
        arenas[0].init(bytesPerBuffer);
        arenas[1].init(bytesPerBuffer);
        current = 0;
    }
    void flip() {
        current ^= 1;
        arenas[current].reset();
    }
    [[nodiscard]] void* alloc(usize size, usize align = 16) { return arenas[current].alloc(size, align); }
    template <class T>
    [[nodiscard]] T* allocArray(usize count) { return arenas[current].allocArray<T>(count); }
    [[nodiscard]] LinearArena& front() { return arenas[current]; }        // buffer de escritura
    [[nodiscard]] LinearArena& back() const { return const_cast<FrameArena*>(this)->arenas[current ^ 1]; }
    [[nodiscard]] usize used() const { return arenas[current].used(); }

private:
    LinearArena arenas[2];
    u32 current = 0;
};

} // namespace sv
