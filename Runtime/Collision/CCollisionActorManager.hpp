#pragma once

#include "RetroTypes.hpp"
#include "zeus/CAABox.hpp"
#include "Collision/CJointCollisionDescription.hpp"

namespace urde
{
class CMaterialList;
class CAnimData;
class CCollisionActor;
class CStateManager;
class CCollisionActorManager
{
public:
    enum class EUpdateOptions
    {
        ObjectSpace,
        WorldSpace
    };

private:
    std::vector<CJointCollisionDescription> x0_jointDescriptions;
    TUniqueId x10_ownerId;
    bool x12_active;
    bool x13_destroyed = false;
    bool x14_movable = true;
public:
    CCollisionActorManager(CStateManager& mgr, TUniqueId owner, TAreaId area,
                           const std::vector<CJointCollisionDescription>& descs, bool active);

    void Update(float dt, CStateManager& mgr, CCollisionActorManager::EUpdateOptions opts);
    void Destroy(CStateManager& mgr) const;
    void SetActive(CStateManager& mgr, bool active);
    void AddMaterial(CStateManager& mgr, const CMaterialList& list);
    void SetMovable(CStateManager& mgr, bool movable);

    u32 GetNumCollisionActors() const { return x0_jointDescriptions.size(); }
    const CJointCollisionDescription& GetCollisionDescFromIndex(u32 i) const { return x0_jointDescriptions[i]; }
    static zeus::CTransform GetWRLocatorTransform(const CAnimData& animData, CSegId id,
                                                  const zeus::CTransform& worldXf,
                                                  const zeus::CTransform& localXf);

};
}
