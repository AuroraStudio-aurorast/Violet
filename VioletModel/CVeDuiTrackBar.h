#pragma once
#include "CApp.h"
#include "Utils.h"
class CVeTrackBar :public Dui::CElem
{
private:
    BITBOOL m_bVertical : 1{};
    BITBOOL m_bLBtnDown : 1{};
    BITBOOL m_bHover : 1{};

    BITBOOL m_bGenEventWhenDragging : 1{};

    bool m_bFixedTop = false;
    bool m_useHighlightColor = true;
    bool m_bRoundedRectMask = false;
    D2D1_RECT_F roundedRectMask;
    float m_roundedRectMaskBorderRadius = 0.f;

    float m_fPos{};
    float m_fMin{};
    float m_fMax{ 100.0f };

    float m_fDragPos{};

    float m_cxyTrack{};

    eck::CEasingCurve* m_pec{};
    ID2D1SolidColorBrush* m_BgBrush{};

    float GetCxyTrack()
    {
        if (m_pec->IsActive())
            return  m_cxyTrack / 2.f + m_cxyTrack / 2.f * m_pec->GetCurrValue();
        else
            return (m_bHover ? m_cxyTrack : m_cxyTrack / 2.f);
    }

    float GetTrackRect(D2D1_RECT_F& rc)
    {
        rc = GetViewRectF();
        const float cxyTrack = GetCxyTrack();
        const float fRadius = GetTrackSpacing();
        if (m_bVertical)
        {
            rc.left += (rc.right - rc.left - cxyTrack) / 2.f;
            rc.right = rc.left + cxyTrack;
            rc.top += fRadius;
            rc.bottom -= fRadius;
        }
        else
        {
            if (m_bFixedTop) {
                rc.top += (rc.bottom - rc.top - m_cxyTrack / 2.f) / 2.f;
                rc.bottom = rc.top + cxyTrack;
                rc.left += fRadius;
                rc.right -= fRadius;
            }
            else {
                rc.top += (rc.bottom - rc.top - cxyTrack) / 2.f;
                rc.bottom = rc.top + cxyTrack;
                rc.left += fRadius;
                rc.right -= fRadius;
            }

        }
        return cxyTrack;
    }

    void GetTrackHitTestRect(D2D1_RECT_F& rc) {
        rc = GetViewRectF();
        const float fRadius = GetTrackSpacing();
        if (m_bVertical)
        {
            rc.left += (rc.right - rc.left - m_cxyTrack) / 2.f;
            rc.right = rc.left + m_cxyTrack;
            rc.top += fRadius;
            rc.bottom -= fRadius;
        }
        else
        {
            rc.top += (rc.bottom - rc.top - m_cxyTrack) / 2.f;
            rc.bottom = rc.top + m_cxyTrack;
            rc.left += fRadius;
            rc.right -= fRadius;
        }
    }

    void GetThumbRect(float cxyTrack, const D2D1_RECT_F& rcTrack, D2D1_RECT_F& rc)
    {
        const float cxy = cxyTrack * m_pec->GetCurrValue();
        if (m_bVertical)
        {
            rc.left = (rcTrack.left + rcTrack.right) / 2.f - cxy;
            rc.top = rcTrack.bottom - cxy;
        }
        else
        {
            rc.left = rcTrack.right - cxy;
            rc.top = (rcTrack.top + rcTrack.bottom) / 2.f - cxy;
        }
        rc.right = rc.left + cxy * 2;
        rc.bottom = rc.top + cxy * 2;
    }

    void GetThumbRect(D2D1_RECT_F& rc)
    {
        const auto cxy = GetTrackRect(rc);
        const float fScale = (GetTrackPos() - m_fMin) / (m_fMax - m_fMin);
        if (m_bVertical)
            rc.bottom = rc.top + (rc.bottom - rc.top) * fScale;
        else
            rc.right = rc.left + (rc.right - rc.left) * fScale;
        GetThumbRect(cxy, rc, rc);
    }

    void SetDragPos(float fPos)
    {
        m_fDragPos = fPos;
        if (m_fDragPos < m_fMin)
            m_fDragPos = m_fMin;
        else if (m_fDragPos > m_fMax)
            m_fDragPos = m_fMax;
    }
public:
    LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        switch (uMsg)
        {
        case WM_PAINT:
        {
            Dui::ELEMPAINTSTRU ps;
            BeginPaint(ps, wParam, lParam);

            if (m_bRoundedRectMask) {
                RoundedRectMaskD2dDC(m_pDC, eck::g_pD2DFactory, roundedRectMask, m_roundedRectMaskBorderRadius);
            }

            Dui::DTB_OPT Opt;
            Opt.uFlags = Dui::DTBO_NEW_RADX | Dui::DTBO_NEW_RADY;
            const float cxyTrack = GetTrackRect(Opt.rcClip);
            Opt.fRadX = Opt.fRadY = cxyTrack / 2.f;

            if (m_pec->IsActive() && m_bFixedTop) {
                //D2D1_RECT_F tbRect = GetRectF();

            }

            //GetTheme()->DrawBackground(Part::TrackBar, State::Normal, Opt.rcClip, &Opt);
            m_BgBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
            m_pDC->FillRoundedRectangle(D2D1::RoundedRect(Opt.rcClip, Opt.fRadX, Opt.fRadY), m_BgBrush);

            const float fScale = (GetTrackPos() - m_fMin) / (m_fMax - m_fMin);
            if (m_bVertical)
                Opt.rcClip.bottom = Opt.rcClip.top + (Opt.rcClip.bottom - Opt.rcClip.top) * fScale;
            else
                Opt.rcClip.right = Opt.rcClip.left + (Opt.rcClip.right - Opt.rcClip.left) * fScale;
            //GetTheme()->DrawBackground(Dui::Part::TrackBar, Dui::State::Selected, Opt.rcClip, &Opt);
            if (m_useHighlightColor) {
                m_BgBrush->SetColor(D2D1::ColorF(highlightColor));
            }
            else {
                m_BgBrush->SetColor(D2D1::ColorF(foregroundColor, 0.2));
            }

            m_pDC->FillRoundedRectangle(D2D1::RoundedRect(Opt.rcClip, Opt.fRadX, Opt.fRadY), m_BgBrush);

            if (m_bRoundedRectMask) {
                m_pDC->PopLayer();
            }

            if (m_bHover || m_pec->IsActive())
            {
                const float cxy = cxyTrack * 3.f / 4.f * m_pec->GetCurrValue();
                GetThumbRect(cxyTrack, Opt.rcClip, Opt.rcClip);
                float borderRadius = (Opt.rcClip.right - Opt.rcClip.left) / 2;
                m_BgBrush->SetColor(D2D1::ColorF(backgroundColor));
                m_pDC->FillRoundedRectangle(D2D1::RoundedRect(Opt.rcClip, borderRadius, borderRadius), m_BgBrush);
                m_BgBrush->SetColor(D2D1::ColorF(foregroundColor, 0.15));
                m_pDC->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(Opt.rcClip.left + 0.5, Opt.rcClip.top + 0.5, Opt.rcClip.right - 0.5, Opt.rcClip.bottom - 0.5), borderRadius, borderRadius), m_BgBrush);

                if (m_useHighlightColor) {
                    m_BgBrush->SetColor(D2D1::ColorF(highlightColor));
                }
                else {
                    m_BgBrush->SetColor(D2D1::ColorF(foregroundColor, 0.3));
                }

                float offset = (Opt.rcClip.right - Opt.rcClip.left) / 4;
                D2D1_RECT_F thumbCenterRect = D2D1::RectF(Opt.rcClip.left + offset, Opt.rcClip.top + offset, Opt.rcClip.right - offset, Opt.rcClip.bottom - offset);
                borderRadius = (thumbCenterRect.right - thumbCenterRect.left) / 2;
                m_pDC->FillRoundedRectangle(D2D1::RoundedRect(thumbCenterRect, borderRadius, borderRadius), m_BgBrush);

                //GetTheme()->DrawBackground(Dui::Part::TrackBarThumb, Dui::State::Hot, Opt.rcClip, nullptr);
            }

            ECK_DUI_DBG_DRAW_FRAME;

            EndPaint(ps);
        }
        return 0;

        case WM_NCHITTEST:
        {
            POINT pt ECK_GET_PT_LPARAM(lParam);
            ClientToElem(pt);

            D2D1_RECT_F rcTrack;
            GetTrackHitTestRect(rcTrack);
            if (!eck::PtInRect(rcTrack, pt))
                return HTTRANSPARENT;
        }
        return HTCLIENT;

        case WM_MOUSEMOVE:
        {
            if (m_bLBtnDown)
            {
                POINT pt ECK_GET_PT_LPARAM(lParam);
                SetDragPos(PtToPos(pt));

                if (m_bGenEventWhenDragging)
                {
                    Dui::DUINMHDR nm{ Dui::TBE_POSCHANGED };
                    GenElemNotify(&nm);
                }
                InvalidateRect();
            }
            else if (!m_bHover)
            {
                m_bHover = TRUE;
                m_pec->Begin(0.f, 1.f);
                GetWnd()->WakeRenderThread();
            }
        }
        return 0;

        case WM_MOUSELEAVE:
        {
            if (!m_bLBtnDown && m_bHover)
            {
                m_bHover = FALSE;
                m_pec->Begin(1.f, 0.f);
                GetWnd()->WakeRenderThread();
            }
        }
        return 0;

        case WM_LBUTTONDOWN:
        {
            POINT pt ECK_GET_PT_LPARAM(lParam);
            D2D1_RECT_F rcThumb;
            GetThumbRect(rcThumb);
            if (eck::PtInRect(rcThumb, eck::MakeD2DPointF(pt)))
            {
                m_bLBtnDown = TRUE;
                SetCapture();
                m_fDragPos = PtToPos(pt);
            }
            else
            {
                SetTrackPos(PtToPos(pt));
                InvalidateRect();
                Dui::DUINMHDR nm{ Dui::TBE_POSCHANGED };
                GenElemNotify(&nm);
            }
        }
        return 0;

        case WM_LBUTTONUP:
        {
            if (m_bLBtnDown)
            {
                POINT pt ECK_GET_PT_LPARAM(lParam);
                ClientToElem(pt);

                m_bLBtnDown = FALSE;
                ReleaseCapture();
                SetTrackPos(PtToPos(pt));
                InvalidateRect();
                Dui::DUINMHDR nm{ Dui::TBE_POSCHANGED };
                GenElemNotify(&nm);

                if (m_bHover)
                {
                    m_bHover = FALSE;
                    m_pec->Begin(1.f, 0.f);
                }
            }
        }
        return 0;

        case WM_CREATE:
        {
            m_pec = new eck::CEasingCurve{};
            InitEasingCurve(m_pec);
            m_pec->SetDuration(200);
            m_pec->SetAnProc(eck::Easing::OutSine);
            m_pec->SetCallBack([](float fCurrValue, float fOldValue, LPARAM lParam)
                {
                    auto pElem = (CElem*)lParam;
                    pElem->InvalidateRect();
                });
            m_pDC->CreateSolidColorBrush(D2D1::ColorF(backgroundColor, 0.2f), &m_BgBrush);

        }
        return 0;

        case WM_DESTROY:
            SafeRelease(m_pec);
            SafeRelease(m_BgBrush);
            return 0;
        }
        return CElem::OnEvent(uMsg, wParam, lParam);
    }

    void SetRange(float fMin, float fMax)
    {
        m_fMin = fMin;
        m_fMax = fMax;
        if (m_fPos < m_fMin)
            m_fPos = m_fMin;
        else if (m_fPos > m_fMax)
            m_fPos = m_fMax;
    }

    void SetTrackPos(float fPos)
    {
        m_fPos = fPos;
        if (m_fPos < m_fMin)
            m_fPos = m_fMin;
        else if (m_fPos > m_fMax)
            m_fPos = m_fMax;
    }

    EckInline float GetTrackPos() const
    {
        return m_bLBtnDown ? m_fDragPos : m_fPos;
    }

    EckInline void SetVertical(BOOL bVertical)
    {
        m_bVertical = bVertical;
    }

    EckInline BOOL IsVertical() const
    {
        return m_bVertical;
    }

    EckInline void SetTrackSize(float cxyTrack)
    {
        m_cxyTrack = cxyTrack;
    }

    EckInline float GetTrackSize() const
    {
        return m_cxyTrack;
    }

    EckInline void SetGenEventWhenDragging(BOOL bGenEventWhenDragging)
    {
        m_bGenEventWhenDragging = bGenEventWhenDragging;
    }

    float PtToPos(POINT pt)
    {
        D2D1_RECT_F rcTrack;
        GetTrackRect(rcTrack);

        if (m_bVertical)
        {
            const float fScale = (pt.y - rcTrack.top) / (rcTrack.bottom - rcTrack.top);
            return m_fMin + (m_fMax - m_fMin) * fScale;
        }
        else
        {
            const float fScale = (pt.x - rcTrack.left) / (rcTrack.right - rcTrack.left);
            return m_fMin + (m_fMax - m_fMin) * fScale;
        }
    }

    void setFixedTop(bool fixedTop) {
        m_bFixedTop = fixedTop;
    }

    void setUseHighlightColor(bool useHighlightColor) {
        m_useHighlightColor = useHighlightColor;
    }

    void setRoundedRectMaskEnabled(bool enable) {
        m_bRoundedRectMask = enable;
    }

    void setRoundedRectMask(D2D1_RECT_F rect, float borderRadius) {
        roundedRectMask = rect;
        m_roundedRectMaskBorderRadius = borderRadius;
    }

    float GetTrackSpacing() const noexcept
    {
        return m_cxyTrack * 3.f / 4.f;
        //return m_cxyTrack / 2.f;
    }
};