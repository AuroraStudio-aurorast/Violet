#pragma once
#include "CApp.h"
#include "Utils.h"
class CVeScrollBar :public Dui::CElem
{
public:
    enum class SbPart
    {
        Button1,
        Button2,
        Track,
        Thumb,
    };
private:
    eck::CInertialScrollView* m_psv{};
    eck::CEasingCurve* m_pec{};
    ID2D1SolidColorBrush* m_BgBrush{};
    BITBOOL m_bHot : 1{};
    BITBOOL m_bLBtnDown : 1{};
    BITBOOL m_bDragThumb : 1{};

    BITBOOL m_bHorizontal : 1{};
    BITBOOL m_bTransparentTrack : 1{ TRUE };
    bool m_blurBkg = true;
public:
    LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        switch (uMsg)
        {
        case WM_NCHITTEST:
        {
            
            if (m_bHot)
                return HTCLIENT;
            else
            {
                if (m_bTransparentTrack)
                {
                    D2D1_POINT_2F pt{ eck::MakeD2DPointF(ECK_GET_PT_LPARAM(lParam)) };
                    ClientToElem(pt);
                    D2D1_RECT_F rc;
                    GetPartRect(rc, SbPart::Thumb);
                    if (rc.top == rc.bottom) {
                        return HTTRANSPARENT;
                    }
                    /*
                    if (!eck::PtInRect(rc, pt))
                        return HTCLIENT;
                    */
                }
                return HTCLIENT;
            }
            

        }
        break;
        case WM_MOUSEMOVE:
        {
            if (!m_bHot)
            {
                m_bHot = TRUE;
                m_pec->Begin(0.f, 1.f);
                GetWnd()->WakeRenderThread();
            }
            if (m_bDragThumb)
            {
                const POINT pt ECK_GET_PT_LPARAM(lParam);
                m_psv->OnMouseMove(m_bHorizontal ? pt.x - GetHeightF() : pt.y - GetWidthF());
                Dui::DUINMHDR nm{ m_bHorizontal ? Dui::EE_HSCROLL : Dui::EE_VSCROLL };
                if (!GenElemNotify(&nm))
                    InvalidateRect();
            }
        }
        return 0;
        case WM_MOUSELEAVE:
        {
            if (m_bHot && !m_bDragThumb)
            {
                m_bHot = FALSE;
                m_pec->Begin(1.f, 0.f);
                GetWnd()->WakeRenderThread();
            }
        }
        return 0;
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        {
            SetCapture();
            m_bLBtnDown = TRUE;
            const POINT pt ECK_GET_PT_LPARAM(lParam);
            const D2D1_POINT_2F ptf{ eck::MakeD2DPointF(pt) };
            D2D1_RECT_F rcf;
            GetPartRect(rcf, SbPart::Thumb);
            if (eck::PtInRect(rcf, ptf))
            {
                m_bDragThumb = TRUE;
                m_psv->OnLButtonDown((int)(m_bHorizontal ? pt.x - GetHeightF() : pt.y - GetWidthF()));
            }
            else
            {
                if (m_bHorizontal)
                    m_psv->SetPos(ptf.x < rcf.left ?
                        m_psv->GetPos() - m_psv->GetPage() :
                        m_psv->GetPos() + m_psv->GetPage());
                else
                    m_psv->SetPos(ptf.y < rcf.top ?
                        m_psv->GetPos() - m_psv->GetPage() :
                        m_psv->GetPos() + m_psv->GetPage());
                Dui::DUINMHDR nm{ m_bHorizontal ? Dui::EE_HSCROLL : Dui::EE_VSCROLL };
                if (!GenElemNotify(&nm))
                    InvalidateRect();
            }
        }
        return 0;
        case WM_CAPTURECHANGED:
        case WM_LBUTTONUP:
        {
            if (m_bLBtnDown)
            {
                m_bLBtnDown = FALSE;
                ReleaseCapture();
                if (m_bDragThumb)
                {
                    m_psv->OnLButtonUp();
                    m_bDragThumb = FALSE;
                    Dui::DUINMHDR nm{ m_bHorizontal ? Dui::EE_HSCROLL : Dui::EE_VSCROLL };
                    if (!GenElemNotify(&nm))
                        InvalidateRect();
                }
            }
        }
        return 0;
        case WM_SIZE:
        {
            const auto cx = GetWidthF(), cy = GetHeightF();
            if (m_bHorizontal)
                m_psv->SetViewSize(cx - 2 * cy);
            else
                m_psv->SetViewSize(cy - 2 * cx);
        }
        break;
        return 0;

        case WM_PAINT:
        {
            Dui::ELEMPAINTSTRU ps;
            BeginPaint(ps, wParam, lParam);

            D2D1_RECT_F rc;
            GetPartRect(rc, SbPart::Thumb);
            float cxyLeave, cxyMin;
            if (m_bHorizontal)
            {
                cxyLeave = (rc.bottom - rc.top) / 3 * 2;
                cxyMin = (rc.bottom - rc.top) - cxyLeave;
            }
            else
            {
                cxyLeave = (rc.right - rc.left) / 3 * 2;
                cxyMin = (rc.right - rc.left) - cxyLeave;
            }
            if (m_pec->IsActive())
            {
                Dui::DTB_OPT Opt;
                Opt.uFlags = Dui::DTBO_NEW_OPACITY;
                Opt.fOpacity = m_pec->GetCurrValue();
                D2D1_RECT_F ViewRect = GetViewRectF();
                int rectRadius = (ViewRect.right - ViewRect.left) / 2;
                //GetTheme()->DrawBackground(Part::ScrollBar, State::Hot, GetViewRectF(), &Opt);

                if (m_blurBkg) {

                    const D2D1_RECT_F rcTop{ 0.f,0.f,GetWidthF(),GetHeightF() };
                    D2D1_RECT_F rcTopSample;
                    if (eck::IntersectRect(rcTopSample, rcTop, ps.rcfClipInElem))
                    {
                        D2D1_MATRIX_3X2_F Mat;
                        m_pDC->GetTransform(&Mat);
                        auto rcSampleInBmp{ rcTopSample };
                        eck::OffsetRect(rcSampleInBmp, Mat.dx, Mat.dy);
                        BlurD2dDC(m_pDC, eck::g_pD2DFactory, rcSampleInBmp,
                            { rcTopSample.left,rcTopSample.top }, 10 * m_pec->GetCurrValue(), rectRadius);
                    }

                }

                m_BgBrush->SetColor(D2D1::ColorF(backgroundColor, 0.2 * m_pec->GetCurrValue()));
                m_pDC->FillRoundedRectangle(D2D1::RoundedRect(ViewRect, rectRadius, rectRadius), m_BgBrush);
                m_BgBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1 * m_pec->GetCurrValue()));
                m_pDC->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(ViewRect.left + 0.5, ViewRect.top + 0.5, ViewRect.right - 0.5, ViewRect.bottom - 0.5), rectRadius, rectRadius), m_BgBrush, 1.f);
                if (m_bHorizontal)
                    rc.top = rc.bottom - cxyMin - cxyLeave * m_pec->GetCurrValue();
                else
                    rc.left = rc.right - cxyMin - cxyLeave * m_pec->GetCurrValue();
            }
            else
            {
                if (!m_bHot)
                {
                    if (m_bHorizontal)
                        rc.top += cxyLeave;
                    else
                        rc.left += cxyLeave;
                }
                else
                    //GetTheme()->DrawBackground(Part::ScrollBar, State::Hot, GetViewRectF(), nullptr);
                {
                    D2D1_RECT_F ViewRect = GetViewRectF();
                    int rectRadius = (ViewRect.right - ViewRect.left) / 2;


                    if (m_blurBkg) {

                        const D2D1_RECT_F rcTop{ 0.f,0.f,GetWidthF(),GetHeightF() };
                        D2D1_RECT_F rcTopSample;
                        if (eck::IntersectRect(rcTopSample, rcTop, ps.rcfClipInElem))
                        {
                            D2D1_MATRIX_3X2_F Mat;
                            m_pDC->GetTransform(&Mat);
                            auto rcSampleInBmp{ rcTopSample };
                            eck::OffsetRect(rcSampleInBmp, Mat.dx, Mat.dy);
                            BlurD2dDC(m_pDC, eck::g_pD2DFactory, rcSampleInBmp,
                                { rcTopSample.left,rcTopSample.top }, 10, rectRadius);
                        }

                    }


                    m_BgBrush->SetColor(D2D1::ColorF(backgroundColor, 0.2));
                    m_pDC->FillRoundedRectangle(D2D1::RoundedRect(ViewRect, rectRadius, rectRadius), m_BgBrush);
                    m_BgBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
                    m_pDC->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(ViewRect.left + 0.5, ViewRect.top + 0.5, ViewRect.right - 0.5, ViewRect.bottom - 0.5), rectRadius, rectRadius), m_BgBrush, 1.f);


                }

            }

            //GetTheme()->DrawBackground(Part::ScrollThumb, State::Normal, rc, nullptr);
            D2D1_RECT_F ThumbRect = rc;
            int rectRadius = (ThumbRect.right - ThumbRect.left) / 2;
            m_BgBrush->SetColor(D2D1::ColorF(foregroundColor, 0.3));
            m_pDC->FillRoundedRectangle(D2D1::RoundedRect(ThumbRect, rectRadius, rectRadius), m_BgBrush);


            EndPaint(ps);
        }
        return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        {
            const auto p = GetParentElem();
            if (p)
                return p->CallEvent(uMsg, wParam, lParam);
        }
        break;

        case WM_CREATE:
        {
            m_pec = new eck::CEasingCurve{};
            InitEasingCurve(m_pec);
            m_pec->SetCallBack([](float fOld, float f, LPARAM lParam)
                {
                    auto p = (CVeScrollBar*)lParam;
                    p->InvalidateRect();
                });
            m_pec->SetAnProc(eck::Easing::OutSine);
            m_pec->SetDuration(160);

            m_psv = new eck::CInertialScrollView{};
            GetWnd()->RegisterTimeLine(m_psv);

            m_pDC->CreateSolidColorBrush(D2D1::ColorF(backgroundColor, 0.2f), &m_BgBrush);
        }
        return 0;

        case WM_DESTROY:
        {
            GetWnd()->UnregisterTimeLine(m_pec);
            GetWnd()->UnregisterTimeLine(m_psv);
            SafeRelease(m_pec);
            SafeRelease(m_psv);
            SafeRelease(m_BgBrush);
        }
        return 0;
        }
        return CElem::OnEvent(uMsg, wParam, lParam);
    }

    EckInlineNdCe auto GetScrollView() const noexcept { return m_psv; }

    void GetPartRect(D2D1_RECT_F& rc, SbPart eType)
    {
        const auto cx = GetWidthF(),
            cy = GetHeightF();
        if (m_bHorizontal)
            switch (eType)
            {
            case SbPart::Button1:
                rc = { 0,0,cy,cy };
                return;
            case SbPart::Button2:
                rc = { cx - cy,0,cx,cy };
                return;
            case SbPart::Track:
                rc = { cy,0,cx - cy,cy };
                return;
            case SbPart::Thumb:
            {
                const int cyThumb = (int)GetTheme()->GetMetrics(Dui::Metrics::CyHThumb);
                const int cxyThumb = m_psv->GetThumbSize();
                rc.left = cy + m_psv->GetThumbPos(cxyThumb);
                rc.top = (cy - cyThumb) / 2;
                rc.right = rc.left + cxyThumb;
                rc.bottom = rc.top + cyThumb;
            }
            return;
            }
        else
            switch (eType)
            {
            case SbPart::Button1:
                rc = { 0,0,cx,cx };
                return;
            case SbPart::Button2:
                rc = { 0,cy - cx,cx,cy };
                return;
            case SbPart::Track:
                rc = { 0,cx,cx,cy - cx };
                return;
            case SbPart::Thumb:
            {
                const int cxThumb = (int)GetTheme()->GetMetrics(Dui::Metrics::CxVThumb);
                const int cxyThumb = m_psv->GetThumbSize();
                rc.left = (cx - cxThumb) / 2;
                rc.top = cx + m_psv->GetThumbPos(cxyThumb);
                rc.right = rc.left + cxThumb;
                rc.bottom = rc.top + cxyThumb;
            }
            return;
            }
        ECK_UNREACHABLE;
    }

    EckInlineCe void SetHorizontal(BOOL b) noexcept { m_bHorizontal = b; }
    EckInlineNdCe BOOL GetHorizontal() const noexcept { return m_bHorizontal; }

    EckInlineCe void SetTransparentTrack(BOOL b) noexcept { m_bTransparentTrack = b; }
    EckInlineNdCe BOOL GetTransparentTrack() const noexcept { return m_bTransparentTrack; }
};