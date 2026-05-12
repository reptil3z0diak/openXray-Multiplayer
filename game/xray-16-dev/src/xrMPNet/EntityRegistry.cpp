#include "EntityRegistry.h"

#include <algorithm>

namespace xrmp::rep
{
NetEntityId EntityRegistry::allocateId()
{
    while (entitiesById_.find(nextId_) != entitiesById_.end())
        ++nextId_;

    return nextId_++;
}

NetEntity* EntityRegistry::find(NetEntityId id)
{
    const auto found = entitiesById_.find(id);
    return found == entitiesById_.end() ? nullptr : found->second.get();
}

const NetEntity* EntityRegistry::find(NetEntityId id) const
{
    const auto found = entitiesById_.find(id);
    return found == entitiesById_.end() ? nullptr : found->second.get();
}

bool EntityRegistry::destroy(NetEntityId id)
{
    auto found = entitiesById_.find(id);
    if (found == entitiesById_.end())
        return false;

    const std::type_index type = std::type_index(typeid(*found->second));
    auto bucket = idsByType_.find(type);
    if (bucket != idsByType_.end())
    {
        auto& ids = bucket->second;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        if (ids.empty())
            idsByType_.erase(bucket);
    }

    entitiesById_.erase(found);
    return true;
}

void EntityRegistry::clear()
{
    entitiesById_.clear();
    idsByType_.clear();
    nextId_ = 1;
}

void EntityRegistry::forEach(const std::function<void(NetEntity&)>& visitor)
{
    for (auto& pair : entitiesById_)
        visitor(*pair.second);
}

void EntityRegistry::forEach(const std::function<void(const NetEntity&)>& visitor) const
{
    for (const auto& pair : entitiesById_)
        visitor(*pair.second);
}
} // namespace xrmp::rep
