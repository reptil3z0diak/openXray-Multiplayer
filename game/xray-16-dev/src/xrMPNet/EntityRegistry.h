#pragma once

#include "NetEntity.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace xrmp::rep
{
class EntityRegistry
{
public:
    EntityRegistry() = default;

    NetEntityId allocateId();

    template <typename TEntity = NetEntity, typename... Args> TEntity& create(NetEntityId id, Args&&... args)
    {
        static_assert(std::is_base_of_v<NetEntity, TEntity>, "EntityRegistry only stores NetEntity instances");

        if (id == InvalidNetEntityId)
            throw std::invalid_argument("EntityRegistry::create requires a non-zero NetEntityId");
        if (entitiesById_.find(id) != entitiesById_.end())
            throw std::runtime_error("EntityRegistry::create received a duplicate NetEntityId");

        auto entity = std::make_unique<TEntity>(id, std::forward<Args>(args)...);
        TEntity& reference = *entity;
        entitiesById_.emplace(id, std::move(entity));
        idsByType_[std::type_index(typeid(TEntity))].push_back(id);
        return reference;
    }

    NetEntity* find(NetEntityId id);
    const NetEntity* find(NetEntityId id) const;

    template <typename TEntity> TEntity* findAs(NetEntityId id)
    {
        return dynamic_cast<TEntity*>(find(id));
    }

    template <typename TEntity> const TEntity* findAs(NetEntityId id) const
    {
        return dynamic_cast<const TEntity*>(find(id));
    }

    bool destroy(NetEntityId id);
    void clear();

    std::size_t size() const { return entitiesById_.size(); }
    bool empty() const { return entitiesById_.empty(); }

    void forEach(const std::function<void(NetEntity&)>& visitor);
    void forEach(const std::function<void(const NetEntity&)>& visitor) const;

    template <typename TEntity, typename TVisitor> void forEachOfType(TVisitor&& visitor)
    {
        const auto bucket = idsByType_.find(std::type_index(typeid(TEntity)));
        if (bucket == idsByType_.end())
            return;

        for (const NetEntityId id : bucket->second)
        {
            TEntity* entity = dynamic_cast<TEntity*>(find(id));
            if (entity)
                visitor(*entity);
        }
    }

private:
    std::unordered_map<NetEntityId, std::unique_ptr<NetEntity>> entitiesById_;
    std::unordered_map<std::type_index, std::vector<NetEntityId>> idsByType_;
    NetEntityId nextId_ = 1;
};
} // namespace xrmp::rep
