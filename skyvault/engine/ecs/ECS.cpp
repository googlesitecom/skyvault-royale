// ============================================================================
//  SkyVault Engine - ECS: partes no-template
// ============================================================================
#include "ecs/ECS.h"

namespace sv::ecs {

Entity Registry::create() {
    u32 index;
    if (!freeIndices.empty()) {
        index = freeIndices.back();
        freeIndices.pop_back();
    } else {
        // El indice 0 esta reservado para InvalidEntity: si es la primera creacion,
        // empujamos un registro muerto de marcador para que las entidades reales
        // empiecen en el indice 1 y el handle coincida siempre con su registro.
        if (records.empty()) records.push_back(EntRecord{});
        index = static_cast<u32>(records.size());
        records.push_back(EntRecord{});
        if (records.size() > EntityIndexMask) {
            SV_ASSERT(false && "limite de entidades alcanzado");
            return InvalidEntity;
        }
    }
    EntRecord& rec = records[index];
    rec.alive = true;
    ++aliveCount;
    return makeEntity(index, rec.gen);
}

void Registry::destroy(Entity e) {
    if (!valid(e)) return;
    const u32 index = entityIndex(e);
    EntRecord& rec = records[index];
    rec.alive = false;
    rec.gen = (rec.gen + 1) & 0xFFu; // 8 bits de generacion
    if (rec.gen == 0) rec.gen = 1;
    freeIndices.push_back(index);
    --aliveCount;
    for (auto& pool : pools)
        if (pool) pool->onDestroy(e);
}

bool Registry::valid(Entity e) const {
    if (e == InvalidEntity) return false;
    const u32 index = entityIndex(e);
    if (index == 0 || index >= records.size()) return false;
    const EntRecord& rec = records[index];
    return rec.alive && rec.gen == entityGen(e);
}

void Registry::clear() {
    records.clear();
    freeIndices.clear();
    pools.clear();
    aliveCount = 0;
}

} // namespace sv::ecs
