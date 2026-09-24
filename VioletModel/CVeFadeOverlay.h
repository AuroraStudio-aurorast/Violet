#pragma once
#include "CApp.h"
#include "Utils.h"

class CVeFadeOverlay : public eck::Dui::CElem
{
private:
    ComPtr<ID2D1LinearGradientBrush> m_pBrFade{};
    bool  m_bEnabled = true;
    float m_fTopFadeHeight = 30.f;
    float m_fTopOffset = 0.f;    // 内容起点（Group 那边同步过来）
    UINT  m_cxReserveRight = 0;      // 右侧预留（给滚动条宽度）

    void ReCreateFadeBrush()
    {
        m_pBrFade.Clear();
        if (!m_bEnabled)
            return;

        const D2D1_GRADIENT_STOP Stop[]
        {
            { 0.f, {.a = 0.f } },
            { 1.f, {.a = 1.f } },
        };

        ComPtr<ID2D1GradientStopCollection> pStopCollection;
        m_pDC->CreateGradientStopCollection(EckArrAndLen(Stop), &pStopCollection);

        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES Prop;
        Prop.startPoint = {};
        Prop.endPoint = { 0.f, m_fTopFadeHeight };
        m_pDC->CreateLinearGradientBrush(Prop, pStopCollection.Get(), &m_pBrFade);
    }

public:
    void SetEnabled(bool b) { m_bEnabled = b;              InvalidateRect(); }
    void SetTopFadeHeight(float h) { m_fTopFadeHeight = h;        InvalidateRect(); }
    void SetTopOffset(float y) { m_fTopOffset = y;            InvalidateRect(); }
    void SetReserveRight(UINT cx) { m_cxReserveRight = cx;       InvalidateRect(); }

    LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        switch (uMsg)
        {
        case WM_PAINT:
        {
            if (!m_bEnabled)
                return 0;

            Dui::ELEMPAINTSTRU ps;
            BeginPaint(ps, wParam, lParam);

            ReCreateFadeBrush();
            if (m_pBrFade.Get())
            {
                const float cx = GetWidthF() - m_cxReserveRight;
                const D2D1_RECT_F rcMask{
                    0.f,
                    m_fTopOffset,
                    cx,
                    m_fTopOffset + m_fTopFadeHeight
                };

                D2D1_RECT_F rcDummy;
                if (eck::IntersectRect(rcDummy, rcMask, ps.rcfClipInElem))
                {
                    D2D1_MATRIX_3X2_F Mat;
                    m_pDC->GetTransform(&Mat);

                    auto rcInBmp{ rcMask };
                    eck::OffsetRect(rcInBmp, Mat.dx, Mat.dy);

                    ProgressiveBlurD2dDC(
                        m_pDC,
                        eck::g_pD2DFactory,
                        rcInBmp,
                        { rcMask.left, rcMask.top },
                        10.0f,
                        BlurDirection::TopToBottom,
                        0.f);
                }

                D2D1_RECT_F rcSample;
                if (eck::IntersectRect(rcSample, rcMask, ps.rcfClipInElem))
                {
                    D2D1_MATRIX_3X2_F Mat;
                    m_pDC->GetTransform(&Mat);
                    auto rcSampleInBmp{ rcSample };
                    eck::OffsetRect(rcSampleInBmp, Mat.dx, Mat.dy);
                    OpacityMaskD2dDC(m_pDC, eck::g_pD2DFactory, rcSampleInBmp,
                        { rcSample.left, rcSample.top }, m_pBrFade.Get(), 0);
                }

            }
            EndPaint(ps);
            return 0;
        }
        }
        return __super::OnEvent(uMsg, wParam, lParam);
    }
};