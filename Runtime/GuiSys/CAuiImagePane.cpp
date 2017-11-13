#include "CAuiImagePane.hpp"
#include "CSimplePool.hpp"
#include "Graphics/CTexture.hpp"
#include "CGuiWidgetDrawParms.hpp"

namespace urde
{

CAuiImagePane::CAuiImagePane(const CGuiWidgetParms& parms, CSimplePool* sp, CAssetId tex0, CAssetId tex1,
                             rstl::reserved_vector<zeus::CVector3f, 4>&& coords,
                             rstl::reserved_vector<zeus::CVector2f, 4>&& uvs, bool initTex)
: CGuiWidget(parms), xc8_tex0(tex0), xcc_tex1(tex1), xe0_coords(std::move(coords)),
  x114_uvs(std::move(uvs))
{
    if (initTex)
        SetTextureID0(tex0, sp);
}

std::shared_ptr<CGuiWidget> CAuiImagePane::Create(CGuiFrame* frame, CInputStream& in, CSimplePool* sp)
{
    CGuiWidgetParms parms = ReadWidgetHeader(frame, in);
    in.readUint32Big();
    in.readUint32Big();
    in.readUint32Big();
    u32 coordCount = in.readUint32Big();
    rstl::reserved_vector<zeus::CVector3f, 4> coords;
    for (int i=0 ; i<coordCount ; ++i)
        coords.push_back(zeus::CVector3f::ReadBig(in));
    u32 uvCount = in.readUint32Big();
    rstl::reserved_vector<zeus::CVector2f, 4> uvs;
    for (int i=0 ; i<uvCount ; ++i)
        uvs.push_back(zeus::CVector2f::ReadBig(in));
    std::shared_ptr<CGuiWidget> ret = std::make_shared<CAuiImagePane>(parms, sp, -1, -1,
                                                                      std::move(coords), std::move(uvs), true);
    ret->ParseBaseInfo(frame, in, parms);
    return ret;
}

void CAuiImagePane::Update(float dt)
{
    xd0_uvBias0.x = std::fmod(xd0_uvBias0.x, 1.f);
    xd0_uvBias0.y = std::fmod(xd0_uvBias0.y, 1.f);
    if (x138_tileSize != zeus::CVector2f::skZero && xb8_tex0Tok.IsLoaded())
    {
        zeus::CVector2f tmp = zeus::CVector2f(xb8_tex0Tok->GetWidth(), xb8_tex0Tok->GetHeight()) / x138_tileSize;
        x144_frameTimer = std::fmod(x144_frameTimer + dt * x140_interval, std::floor(tmp.x) * std::floor(tmp.y));
    }

    CGuiWidget::Update(dt);
}

CAuiImagePane::Filters::Filters(TLockedToken<CTexture>& tex)
: m_darkenerQuad(EFilterType::Blend, tex),
  m_flashQuad{{EFilterType::Add, tex}, {EFilterType::Add, tex}},
  m_alphaQuad{{EFilterType::Blend, tex}, {EFilterType::Blend, tex}},
  m_addQuad{{EFilterType::Add, tex}, {EFilterType::Add, tex}}
{
}

void CAuiImagePane::DoDrawImagePane(const zeus::CColor& color, const CTexture& tex,
                                    int frame, float alpha, bool noBlur, CTexturedQuadFilterAlpha& quad) const
{
    zeus::CColor useColor = color;
    useColor.a *= alpha;

    rstl::reserved_vector<zeus::CVector2f, 4> vec;
    const rstl::reserved_vector<zeus::CVector2f, 4>* useUVs;
    if (x138_tileSize != zeus::CVector2f::skZero)
    {
        zeus::CVector2f res(xb8_tex0Tok->GetWidth(), xb8_tex0Tok->GetHeight());
        zeus::CVector2f tmp = res / x138_tileSize;
        zeus::CVector2f tmpRecip = x138_tileSize / res;
        float x0 = tmpRecip.x * (frame % int(tmp.x));
        float x1 = x0 + tmpRecip.x;
        float y0 = tmpRecip.y * (frame % int(tmp.y));
        float y1 = y0 + tmpRecip.y;
        vec.push_back(zeus::CVector2f(x0, y0));
        vec.push_back(zeus::CVector2f(x0, y1));
        vec.push_back(zeus::CVector2f(x1, y0));
        vec.push_back(zeus::CVector2f(x1, y1));
        useUVs = &vec;
    }
    else
    {
        useUVs = &x114_uvs;
    }

    CTexturedQuadFilter::Vert verts[] =
    {
        {xe0_coords[0], (*useUVs)[0] + xd0_uvBias0},
        {xe0_coords[1], (*useUVs)[1] + xd0_uvBias0},
        {xe0_coords[2], (*useUVs)[2] + xd0_uvBias0},
        {xe0_coords[3], (*useUVs)[3] + xd0_uvBias0}
    };

    if (noBlur)
    {
        quad.drawVerts(useColor, verts);
    }
    else if ((x14c_deResFactor == 0.f && alpha == 1.f) || tex.GetNumMips() == 1)
    {
        quad.drawVerts(useColor, verts, 0.f);
    }
    else
    {
        float tmp = (1.f - x14c_deResFactor) * alpha;
        float tmp3 = 1.f - tmp * tmp * tmp;
        float mip = tmp3 * (tex.GetNumMips() - 1);
        quad.drawVerts(useColor, verts, mip);
    }
}

void CAuiImagePane::Draw(const CGuiWidgetDrawParms& params) const
{
    CGraphics::SetModelMatrix(x34_worldXF);
    if (!GetIsVisible() || !xb8_tex0Tok.IsLoaded())
        return;
    GetIsFinishedLoadingWidgetSpecific();
    if (!m_filters)
        const_cast<CAuiImagePane*>(this)->m_filters.emplace(const_cast<CAuiImagePane*>(this)->xb8_tex0Tok);
    Filters& filters = const_cast<Filters&>(*m_filters);
    zeus::CColor color = xa8_color2;
    color.a *= params.x0_alphaMod;
    //SetZUpdate(xac_drawFlags == EGuiModelDrawFlags::Shadeless || xac_drawFlags == EGuiModelDrawFlags::Opaque);
    float blur0 = 1.f;
    float blur1 = 0.f;
    int frame0 = x144_frameTimer;
    int frame1 = 0;
    if (x140_interval < 1.f && x140_interval > 0.f)
    {
        zeus::CVector2f tmp = zeus::CVector2f(xb8_tex0Tok->GetWidth(), xb8_tex0Tok->GetHeight()) / x138_tileSize;
        frame1 = (frame0 + 1) % int(tmp.x * tmp.y);
        if (x148_fadeDuration == 0.f)
            blur1 = 1.f;
        else
            blur1 = std::min(std::fmod(x144_frameTimer, 1.f) / x148_fadeDuration, 1.f);
        blur0 = 1.f - blur1;
    }

    // Alpha blend
    DoDrawImagePane(color * zeus::CColor(0.f, 0.5f), *xb8_tex0Tok, frame0, 1.f, true, filters.m_darkenerQuad);

    if (x150_flashFactor > 0.f)
    {
        // Additive blend
        zeus::CColor color2 = xa8_color2;
        color2.a = x150_flashFactor;
        DoDrawImagePane(color2, *xb8_tex0Tok, frame0, blur0, false, filters.m_flashQuad[0]);
        if (blur1 > 0.f)
            DoDrawImagePane(color2, *xb8_tex0Tok, frame1, blur1, false, filters.m_flashQuad[1]);
    }

    switch (xac_drawFlags)
    {
    case EGuiModelDrawFlags::Shadeless:
    case EGuiModelDrawFlags::Opaque:
        // Opaque blend
        DoDrawImagePane(color, *xb8_tex0Tok, frame0, blur0, false, filters.m_alphaQuad[0]);
        if (blur1 > 0.f)
            DoDrawImagePane(color, *xb8_tex0Tok, frame1, blur1, false, filters.m_alphaQuad[1]);
        break;
    case EGuiModelDrawFlags::Alpha:
        // Alpha blend
        DoDrawImagePane(color, *xb8_tex0Tok, frame0, blur0, false, filters.m_alphaQuad[0]);
        if (blur1 > 0.f)
            DoDrawImagePane(color, *xb8_tex0Tok, frame1, blur1, false, filters.m_alphaQuad[1]);
        break;
    case EGuiModelDrawFlags::Additive:
        // Additive blend
        DoDrawImagePane(color, *xb8_tex0Tok, frame0, blur0, false, filters.m_addQuad[0]);
        if (blur1 > 0.f)
            DoDrawImagePane(color, *xb8_tex0Tok, frame1, blur1, false, filters.m_addQuad[1]);
        break;
    case EGuiModelDrawFlags::AlphaAdditiveOverdraw:
        // Alpha blend
        DoDrawImagePane(color, *xb8_tex0Tok, frame0, blur0, false, filters.m_alphaQuad[0]);
        if (blur1 > 0.f)
            DoDrawImagePane(color, *xb8_tex0Tok, frame1, blur1, false, filters.m_alphaQuad[1]);
        // Full additive blend
        DoDrawImagePane(color, *xb8_tex0Tok, frame0, blur0, false, filters.m_addQuad[0]);
        if (blur1 > 0.f)
            DoDrawImagePane(color, *xb8_tex0Tok, frame1, blur1, false, filters.m_addQuad[1]);
        break;
    default: break;
    }
}

bool CAuiImagePane::GetIsFinishedLoadingWidgetSpecific() const
{
    return !xb8_tex0Tok || xb8_tex0Tok.IsLoaded();
}

void CAuiImagePane::SetTextureID0(CAssetId tex, CSimplePool* sp)
{
    xc8_tex0 = tex;
    if (!sp)
        return;
    if (xc8_tex0.IsValid())
        xb8_tex0Tok = sp->GetObj({FOURCC('TXTR'), xc8_tex0});
    else
        xb8_tex0Tok = TLockedToken<CTexture>();
}

void CAuiImagePane::SetAnimationParms(const zeus::CVector2f& tileSize, float interval, float fadeDuration)
{
    x138_tileSize = tileSize;
    x140_interval = interval;
    x144_frameTimer = 0.f;
    x148_fadeDuration = fadeDuration;
}

}
