#include "CBodyStateCmdMgr.hpp"

namespace urde
{

CBodyStateCmdMgr::CBodyStateCmdMgr()
{
    x40_commandTable.push_back(&xb8_getup);
    x40_commandTable.push_back(&xc4_step);
    x40_commandTable.push_back(&xd4_);
    x40_commandTable.push_back(&xdc_knockDown);
    x40_commandTable.push_back(&xf4_knockBack);
    x40_commandTable.push_back(&x10c_meleeAttack);
    x40_commandTable.push_back(&x128_projectileAttack);
    x40_commandTable.push_back(&x144_loopAttack);
    x40_commandTable.push_back(&x154_loopReaction);
    x40_commandTable.push_back(&x160_loopHitReaction);
    x40_commandTable.push_back(&x16c_);
    x40_commandTable.push_back(&x174_);
    x40_commandTable.push_back(&x17c_);
    x40_commandTable.push_back(&x184_);
    x40_commandTable.push_back(&x18c_generate);
    x40_commandTable.push_back(&x1ac_hurled);
    x40_commandTable.push_back(&x1d0_jump);
    x40_commandTable.push_back(&x1f8_slide);
    x40_commandTable.push_back(&x210_taunt);
    x40_commandTable.push_back(&x21c_scripted);
    x40_commandTable.push_back(&x230_cover);
    x40_commandTable.push_back(&x254_wallHang);
    x40_commandTable.push_back(&x260_);
    x40_commandTable.push_back(&x268_);
    x40_commandTable.push_back(&x270_additiveAim);
    x40_commandTable.push_back(&x278_additiveFlinch);
    x40_commandTable.push_back(&x284_additiveReaction);
    x40_commandTable.push_back(&x298_);
}

}
