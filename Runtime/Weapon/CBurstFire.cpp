#include "CBurstFire.hpp"

namespace urde
{
CBurstFire::CBurstFire(SBurst** bursts, s32 firstIndex)
    : x10_(firstIndex)
{
    SBurst** burst = bursts;
    while (1)
    {
        if (!*burst)
            break;

        x18_bursts.push_back(*burst);
        ++burst;
    }
}

void CBurstFire::Update(CStateManager&, float)
{

}

zeus::CVector3f CBurstFire::GetDistanceCompensatedError(float, float) const
{
    return {};
}

void CBurstFire::SetFirstBurst(bool)
{

}

bool CBurstFire::IsBurstSet() const
{
    return false;
}

void CBurstFire::SetTimeToNextShot(float)
{

}

bool CBurstFire::ShouldFire() const
{
    return false;
}

void CBurstFire::Start(CStateManager&)
{

}

void CBurstFire::GetError(float, float) const
{

}

float CBurstFire::GetMaxXError() const
{
    return 0;
}

float CBurstFire::GetMaxZError() const
{
    return 0;
}

void CBurstFire::GetError() const
{

}

void CBurstFire::SetFirstBurstIndex(s32)
{

}
}
