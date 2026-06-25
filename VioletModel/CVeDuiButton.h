#pragma once
#include "CApp.h"
#include "Utils.h"
class CVeButton :public Dui::CElem
{
private:
    ID2D1SolidColorBrush* m_pBrush{};
    IDWriteTextLayout* m_pLayout{};

    ID2D1Bitmap* m_pImg{};
    D2D1_SIZE_F m_sizeImg{};
    float m_cxText{};
    float m_cyText{};

    BITBOOL m_bHot : 1{};
    BITBOOL m_bLBtnDown : 1{};

    BITBOOL m_bAutoScale : 1{ TRUE };

    eck::CEasingCurve* m_EasingBtn{};
    bool m_bAnimation = true;
    float bgAlpha = 0.f;

    float m_imgPadding = -1;

    bool m_showBorder = false;
    bool m_blurBkg = false;

    constexpr float CalcImageWidth() const
    {
        const float Padding = GetTheme()->GetMetrics(Dui::Metrics::Padding);
        if (m_pImg)
            if (m_bAutoScale)
                return (GetHeightF() - Padding * 2) * m_sizeImg.width / m_sizeImg.height;
            else
                return m_sizeImg.width;
        else
            return 0.f;
    }

    void UpdateTextLayout(PCWSTR pszText, int cchText)
    {
        const float Padding = GetTheme()->GetMetrics(Dui::Metrics::Padding);
        const float Padding2 = GetTheme()->GetMetrics(Dui::Metrics::SmallPadding);

        SafeRelease(m_pLayout);
        eck::g_pDwFactory->CreateTextLayout(pszText, cchText,
            GetTextFormat(),
            GetWidthF() - CalcImageWidth() - Padding * 2 - Padding2,
            GetHeightF() - Padding * 2,
            &m_pLayout);
        if (m_pLayout)
        {
            DWRITE_TEXT_METRICS tm;
            m_pLayout->GetMetrics(&tm);
            m_cxText = tm.width;
            m_cyText = tm.height;
        }
        else
        {
            m_cxText = 0.f;
            m_cyText = 0.f;
        }
    }

    void UpdateTextLayout()
    {
        UpdateTextLayout(GetText().Data(), GetText().Size());
    }
public:

    void setBackVisible(bool Visible) {
        if (m_bAnimation)
        {
            float alphaNormal = 0;
            if (Visible) {
                alphaNormal = 0.1;
            }
            else {
                alphaNormal = 0;
            }
            if (!m_EasingBtn)
            {
                m_EasingBtn = new eck::CEasingCurve{};
                GetWnd()->RegisterTimeLine(m_EasingBtn);
                m_EasingBtn->SetParam((LPARAM)this);
                m_EasingBtn->SetDuration(100);
                m_EasingBtn->SetAnProc(eck::Easing::Linear);
                //m_pecPage->SetCallBack(&PageAnCallback);

                m_EasingBtn->SetCallBack([](float fCurrValue, float fOldValue, LPARAM lParam)
                    {
                        const auto p = (CVeButton*)lParam;
                        p->bgAlpha = fCurrValue;

                        if (p->m_EasingBtn->IsStop()) {
                            //p->bgAlpha = ;
                   
                        }
                        p->InvalidateRect();
                    });


            }
            m_EasingBtn->Begin(bgAlpha, alphaNormal);
            GetWnd()->WakeRenderThread();
        }
    }

    void setImgPadding(float padding) {
        m_imgPadding = padding;
    }

    LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        switch (uMsg)
        {
        case WM_PAINT:
        {
            Dui::ELEMPAINTSTRU ps;
            BeginPaint(ps, wParam, lParam);
            
            Dui::State eState;
            if (m_bLBtnDown)
                eState = Dui::State::Selected;
            else if (m_bHot)
                eState = Dui::State::Hot;
            else
                eState = Dui::State::Normal;

            D2D1_RECT_F viewRect = GetViewRectF();


            if (m_blurBkg) {

                m_pDC->Flush();
                const D2D1_RECT_F rcTop{ 0.f,0.f,GetWidthF(),GetHeightF()};
                D2D1_RECT_F rcTopSample;
                if (eck::IntersectRect(rcTopSample, rcTop, ps.rcfClipInElem))
                {
                    D2D1_MATRIX_3X2_F Mat;
                    m_pDC->GetTransform(&Mat);
                    auto rcSampleInBmp{ rcTopSample };
                    eck::OffsetRect(rcSampleInBmp, Mat.dx, Mat.dy);
                    BlurD2dDC(m_pDC, eck::g_pD2DFactory, rcSampleInBmp,
                        { rcTopSample.left,rcTopSample.top }, 10 - 10 * bgAlpha * 3, 8);
                }


            }

            if (m_showBorder) {
                m_pBrush->SetColor(D2D1::ColorF(backgroundColor, 0.5 + bgAlpha * 0.3));
                m_pDC->FillRoundedRectangle(D2D1::RoundedRect(viewRect, 8, 8), m_pBrush);
                m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.15 - bgAlpha));
                m_pDC->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(viewRect.left + 0.5, viewRect.top + 0.5, viewRect.right - 0.5, viewRect.bottom - 0.5), 8, 8), m_pBrush, 1.0f);
            }

            if (eState == Dui::State::Selected) {
                m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.2));
                m_pDC->FillRoundedRectangle(D2D1::RoundedRect(viewRect, 8, 8), m_pBrush);
            }
            if (eState == Dui::State::Hot) {
                m_pBrush->SetColor(D2D1::ColorF(foregroundColor, bgAlpha));
                m_pDC->FillRoundedRectangle(D2D1::RoundedRect(viewRect, 8, 8), m_pBrush);
            }
            if (m_bAnimation) {
                if (eState == Dui::State::Normal) {
                    m_pBrush->SetColor(D2D1::ColorF(foregroundColor, bgAlpha));
                    m_pDC->FillRoundedRectangle(D2D1::RoundedRect(viewRect, 8, 8), m_pBrush);
                }
            }
            //GetTheme()->DrawBackground(Part::Button, eState, GetViewRectF(), nullptr);

            const float Padding = GetTheme()->GetMetrics(Dui::Metrics::Padding);
            float PaddingImg;
            if (m_imgPadding == -1) {
                PaddingImg = Padding;
            }
            else {
                PaddingImg = m_imgPadding;
            }
            const float Padding2 = GetTheme()->GetMetrics(Dui::Metrics::SmallPadding);
            if (m_pImg)
            {
                D2D1_RECT_F rcImg;
                if (m_bAutoScale)
                {
                    const auto cy = GetHeightF() - PaddingImg * 2;
                    const auto cx = cy * m_sizeImg.width / m_sizeImg.height;
                    rcImg.left = PaddingImg;
                    rcImg.top = PaddingImg;
                    rcImg.right = rcImg.left + cx;
                    rcImg.bottom = rcImg.top + cy;
                }
                else
                {
                    rcImg.left = (GetWidthF() - PaddingImg * 2 - Padding2 -
                        m_sizeImg.width - m_cxText) / 2.f;
                    rcImg.top = (GetHeightF() - m_sizeImg.height) / 2.f;
                    rcImg.right = rcImg.left + m_sizeImg.width;
                    rcImg.bottom = rcImg.top + m_sizeImg.height;
                }
                //m_pDC->DrawBitmap(m_pImg, rcImg);
                m_pDC->DrawBitmap(m_pImg, rcImg, 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            }

            if (m_pLayout)
            {
                D2D1_COLOR_F cr;
                GetTheme()->GetSysColor(Dui::SysColor::Text, cr);
                m_pBrush->SetColor(cr);
                m_pDC->DrawTextLayout({ Padding + CalcImageWidth(),Padding },
                    m_pLayout, m_pBrush, Dui::DrawTextLayoutFlags);
            }


            ECK_DUI_DBG_DRAW_FRAME;
            EndPaint(ps);
        }
        return 0;

        case WM_MOUSEMOVE:
        {
            if (!m_bHot)
            {
                m_bHot = TRUE;
                setBackVisible(m_bHot);
                InvalidateRect();
            }
        }
        return 0;

        case WM_MOUSELEAVE:
        {
            if (m_bHot)
            {
                m_bHot = FALSE;
                setBackVisible(m_bHot);
                InvalidateRect();
            }
        }
        return 0;

        case WM_LBUTTONDBLCLK:// 连击修正
        case WM_LBUTTONDOWN:
        {
            SetFocus();
            m_bLBtnDown = TRUE;
            SetCapture();
            InvalidateRect();
        }
        return 0;

        case WM_LBUTTONUP:
        {
            if (m_bLBtnDown)
            {
                m_bLBtnDown = FALSE;
                ReleaseCapture();
                InvalidateRect();
                if (eck::PtInRect(GetViewRectF(), POINT ECK_GET_PT_LPARAM(lParam)))
                {
                    Dui::DUINMHDR nm{ Dui::EE_COMMAND };
                    GenElemNotify(&nm);
                }
            }
        }
        return 0;

        case WM_SIZE:
            UpdateTextLayout();
            return 0;

        case WM_SETTEXT:
            UpdateTextLayout((PCWSTR)lParam, (int)wParam);
            InvalidateRect();
            return 0;

        case WM_SETFONT:
            UpdateTextLayout();
            if (lParam)
                InvalidateRect();
            return 0;

        case WM_CREATE:
            m_pDC->CreateSolidColorBrush({}, &m_pBrush);
            return 0;

        case WM_DESTROY:
        {
            SafeRelease(m_pBrush);
            SafeRelease(m_pLayout);
            SafeRelease(m_pImg);
            m_bHot = FALSE;
            m_bLBtnDown = FALSE;
        }
        return 0;
        }
        return CElem::OnEvent(uMsg, wParam, lParam);
    }

    void SetBitmap(ID2D1Bitmap* pImg)
    {
        ECK_DUILOCK;
        std::swap(m_pImg, pImg);
        if (m_pImg)
        {
            m_pImg->AddRef();
            m_sizeImg = m_pImg->GetSize();
        }
        else
            m_sizeImg = {};
        UpdateTextLayout();
        if (pImg)
            pImg->Release();
    }

    void showBorder(bool showBorder) {
        m_showBorder = showBorder;
    }

    void blurBkg(bool blurbkg) {
        m_blurBkg = blurbkg;
    }
};