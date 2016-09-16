#include "logvisor/logvisor.hpp"
#include "CTweaks.hpp"
#include "CResFactory.hpp"
#include "CResLoader.hpp"
#include "GameGlobalObjects.hpp"
#include "Editor/ProjectManager.hpp"
#include "Editor/ProjectResourceFactoryMP1.hpp"
#include "DataSpec/DNAMP1/Tweaks/CTweakGame.hpp"
#include "DataSpec/DNAMP1/Tweaks/CTweakPlayer.hpp"
#include "DataSpec/DNAMP1/Tweaks/CTweakPlayerControl.hpp"
#include "DataSpec/DNAMP1/Tweaks/CTweakGunRes.hpp"
#include "DataSpec/DNAMP1/Tweaks/CTweakPlayerRes.hpp"
#include "DataSpec/DNAMP1/Tweaks/CTweakSlideShow.hpp"

namespace urde
{

namespace MP1
{

static logvisor::Module Log("MP1::CTweaks");

static const SObjectTag& IDFromFactory(CResFactory& factory, const char* name)
{
    const SObjectTag* tag = factory.GetResourceIdByName(name);
    if (!tag)
        Log.report(logvisor::Fatal, "Tweak Asset not found when loading... '%s'", name);
    return *tag;
}

void CTweaks::RegisterTweaks()
{
    ProjectResourceFactoryMP1& factory = ProjectManager::g_SharedManager->resourceFactoryMP1();
    std::experimental::optional<CMemoryInStream> strm;

    SObjectTag tag = factory.ProjectResourceFactoryBase::TagFromPath(_S("MP1/Tweaks/SlideShow.yaml"));
    strm.emplace(factory.LoadResourceSync(tag).release(), factory.ResourceSize(tag));
    g_tweakSlideShow = new DataSpec::DNAMP1::CTweakSlideShow(*strm);
}

void CTweaks::RegisterResourceTweaks()
{
    ProjectResourceFactoryMP1& factory = ProjectManager::g_SharedManager->resourceFactoryMP1();
    std::experimental::optional<CMemoryInStream> strm;
    
    SObjectTag tag = factory.ProjectResourceFactoryBase::TagFromPath(_S("MP1/Tweaks/GunRes.yaml"));
    strm.emplace(factory.LoadResourceSync(tag).release(), factory.ResourceSize(tag));
    g_tweakGunRes = new DataSpec::DNAMP1::CTweakGunRes(*strm);
    
    tag = factory.ProjectResourceFactoryBase::TagFromPath(_S("MP1/Tweaks/PlayerRes.yaml"));
    strm.emplace(factory.LoadResourceSync(tag).release(), factory.ResourceSize(tag));
    g_tweakPlayerRes = new DataSpec::DNAMP1::CTweakPlayerRes(*strm);
}

}
}
