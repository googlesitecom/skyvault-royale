// ============================================================================
//  SkyVault Engine - ECS propio (Entity Component System)
//  Sparse sets densos por tipo de componente, handles con generacion.
// ============================================================================
#pragma once

#include "core/Core.h"
#include <functional>
#include <memory>
#include <vector>
#include <utility>

namespace sv::ecs {

using Entity = u32;
constexpr Entity InvalidEntity = 0;
constexpr u32   EntityIndexMask = 0x00FFFFFFu;
constexpr u32   EntityGenShift  = 24;

[[nodiscard]] constexpr u32 entityIndex(Entity e) { return e & EntityIndexMask; }
[[nodiscard]] constexpr u32 entityGen(Entity e)   { return e >> EntityGenShift; }
[[nodiscard]] constexpr Entity makeEntity(u32 index, u32 gen) {
    return (gen << EntityGenShift) | (index & EntityIndexMask);
}

namespace detail {
    inline u32 nextTypeIndex() {
        static u32 counter = 1; // 0 reservado
        return counter++;
    }
}
template <class C>
[[nodiscard]] u32 componentTypeIndex() {
    static const u32 idx = detail::nextTypeIndex();
    return idx;
}

class Registry {
public:
    Registry() = default;
    ~Registry() { clear(); }
    SV_NO_COPY(Registry);

    // ---------------- entidades ----------------
    [[nodiscard]] Entity create();
    void destroy(Entity e);
    [[nodiscard]] bool valid(Entity e) const;
    [[nodiscard]] usize size() const { return aliveCount; }
    void clear();

    // ---------------- componentes ----------------
    template <class C, class... Args>
    [[nodiscard]] C& emplace(Entity e, Args&&... args);
    template <class C> [[nodiscard]] bool has(Entity e) const;
    template <class C> [[nodiscard]] C& get(Entity e);
    template <class C> [[nodiscard]] const C& get(Entity e) const;
    template <class C> [[nodiscard]] C* find(Entity e);
    template <class C> void remove(Entity e);
    template <class C> [[nodiscard]] usize count() const;
    template <class C> void clearComponent();

    // ---------------- iteracion ----------------
    template <class C, class F> void each(F&& fn);
    template <class C1, class C2, class F> void each2(F&& fn);
    template <class C1, class C2, class C3, class F> void each3(F&& fn);
    template <class C> [[nodiscard]] const std::vector<Entity>* entitiesOf() const;

private:
    struct PoolBase {
        virtual ~PoolBase() = default;
        virtual void onDestroy(Entity e) = 0;
        virtual void grow() = 0;
    };

    template <class C>
    struct Pool final : PoolBase {
        static constexpr u32 NoPos = 0xFFFFFFFFu;
        std::vector<Entity> entities;
        std::vector<C> components;
        std::vector<u32> sparse;

        void grow() override { if (sparse.size() < 4096) sparse.resize(4096, NoPos); }
        void ensureSparse(u32 idx) { if (idx >= sparse.size()) sparse.resize(idx + 1, NoPos); }
        [[nodiscard]] bool has(Entity e) const {
            const u32 i = entityIndex(e);
            return i < sparse.size() && sparse[i] != NoPos;
        }
        [[nodiscard]] C& get(Entity e) { return components[sparse[entityIndex(e)]]; }
        void insert(Entity e, C&& c) {
            const u32 i = entityIndex(e);
            ensureSparse(i);
            sparse[i] = static_cast<u32>(entities.size());
            entities.push_back(e);
            components.push_back(std::move(c));
        }
        void remove(Entity e) {
            const u32 i = entityIndex(e);
            if (i >= sparse.size() || sparse[i] == NoPos) return;
            const u32 pos = sparse[i];
            const u32 last = static_cast<u32>(entities.size() - 1);
            if (pos != last) {
                entities[pos] = entities[last];
                components[pos] = std::move(components[last]);
                sparse[entityIndex(entities[pos])] = pos;
            }
            entities.pop_back();
            components.pop_back();
            sparse[i] = NoPos;
        }
        void onDestroy(Entity e) override { remove(e); }
    };

    template <class C>
    [[nodiscard]] Pool<C>* poolFor(bool createIfMissing) {
        const u32 ti = componentTypeIndex<C>();
        if (pools.size() <= ti) pools.resize(ti + 1);
        if (!pools[ti] && createIfMissing) pools[ti] = std::make_unique<Pool<C>>();
        return static_cast<Pool<C>*>(pools[ti].get());
    }

    struct EntRecord { u32 gen = 1; bool alive = false; };
    std::vector<EntRecord> records;
    std::vector<u32>       freeIndices;
    std::vector<std::unique_ptr<PoolBase>> pools;
    usize aliveCount = 0;
};

// ---------------------------------------------------------------------------
// Implementacion template
// ---------------------------------------------------------------------------
template <class C, class... Args>
C& Registry::emplace(Entity e, Args&&... args) {
    SV_ASSERT(valid(e));
    Pool<C>* pool = poolFor<C>(true);
    if (pool->has(e)) pool->get(e) = C{std::forward<Args>(args)...};
    else pool->insert(e, C{std::forward<Args>(args)...});
    return pool->get(e);
}

template <class C>
bool Registry::has(Entity e) const {
    const u32 ti = componentTypeIndex<C>();
    if (ti >= pools.size() || !pools[ti]) return false;
    return static_cast<Pool<C>*>(pools[ti].get())->has(e);
}

template <class C>
C& Registry::get(Entity e) {
    SV_ASSERT(valid(e));
    Pool<C>* pool = poolFor<C>(true);
    SV_ASSERT(pool->has(e));
    return pool->get(e);
}

template <class C>
const C& Registry::get(Entity e) const {
    const u32 ti = componentTypeIndex<C>();
    SV_ASSERT(ti < pools.size() && pools[ti]);
    return static_cast<const Pool<C>*>(pools[ti].get())->get(e);
}

template <class C>
C* Registry::find(Entity e) {
    const u32 ti = componentTypeIndex<C>();
    if (ti >= pools.size() || !pools[ti]) return nullptr;
    auto* pool = static_cast<Pool<C>*>(pools[ti].get());
    return pool->has(e) ? &pool->get(e) : nullptr;
}

template <class C>
void Registry::remove(Entity e) {
    Pool<C>* pool = poolFor<C>(false);
    if (pool) pool->remove(e);
}

template <class C>
usize Registry::count() const {
    const u32 ti = componentTypeIndex<C>();
    if (ti >= pools.size() || !pools[ti]) return 0;
    return static_cast<const Pool<C>*>(pools[ti].get())->entities.size();
}

template <class C>
void Registry::clearComponent() {
    Pool<C>* pool = poolFor<C>(false);
    if (pool) { pool->entities.clear(); pool->components.clear(); pool->sparse.clear(); }
}

template <class C, class F>
void Registry::each(F&& fn) {
    Pool<C>* pool = poolFor<C>(false);
    if (!pool) return;
    for (usize i = pool->entities.size(); i-- > 0;) {
        const Entity e = pool->entities[i];
        fn(e, pool->components[i]);
    }
}

template <class C1, class C2, class F>
void Registry::each2(F&& fn) {
    Pool<C1>* p1 = poolFor<C1>(false);
    Pool<C2>* p2 = poolFor<C2>(false);
    if (!p1 || !p2) return;
    const bool iterateFirst = p1->entities.size() <= p2->entities.size();
    if (iterateFirst) {
        for (usize i = p1->entities.size(); i-- > 0;) {
            const Entity e = p1->entities[i];
            if (p2->has(e)) fn(e, p1->components[i], p2->get(e));
        }
    } else {
        for (usize i = p2->entities.size(); i-- > 0;) {
            const Entity e = p2->entities[i];
            if (p1->has(e)) fn(e, p1->get(e), p2->components[i]);
        }
    }
}

template <class C1, class C2, class C3, class F>
void Registry::each3(F&& fn) {
    Pool<C1>* p1 = poolFor<C1>(false);
    Pool<C2>* p2 = poolFor<C2>(false);
    Pool<C3>* p3 = poolFor<C3>(false);
    if (!p1 || !p2 || !p3) return;
    for (usize i = p1->entities.size(); i-- > 0;) {
        const Entity e = p1->entities[i];
        if (p2->has(e) && p3->has(e)) fn(e, p1->components[i], p2->get(e), p3->get(e));
    }
}

template <class C>
const std::vector<Entity>* Registry::entitiesOf() const {
    const u32 ti = componentTypeIndex<C>();
    if (ti >= pools.size() || !pools[ti]) return nullptr;
    return &static_cast<const Pool<C>*>(pools[ti].get())->entities;
}

} // namespace sv::ecs
