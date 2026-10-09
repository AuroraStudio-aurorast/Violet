#include "pch.h"
#include "CVeDuiSpacer.h"
#include "Utils.h"
#include "CApp.h"

LRESULT CVeSpacer::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        Dui::ELEMPAINTSTRU ps;
        const auto cx = GetWidthF();
        const auto cy = GetHeightF();
        BeginPaint(ps, wParam, lParam);
        m_pDC->Flush();
        const D2D1_RECT_F rcTop{ 0.f,0.f,GetWidthF(),GetHeightF() };
        D2D1_RECT_F rcTopSample;
        if (eck::IntersectRect(rcTopSample, rcTop, ps.rcfClipInElem))
        {
            D2D1_MATRIX_3X2_F Mat;
            m_pDC->GetTransform(&Mat);
            auto rcSampleInBmp{ rcTopSample };
            eck::OffsetRect(rcSampleInBmp, Mat.dx, Mat.dy);

            // ===== 内联 BlurD2dDC 实现 =====
            ComPtr<ID2D1Bitmap1> pBmp;
            ComPtr<ID2D1Image>   pTarget;
            m_pDC->GetTarget(&pTarget);
            pTarget->QueryInterface(&pBmp);

            HRESULT hr;
            float dpiX{}, dpiY{};
            m_pDC->GetDpi(&dpiX, &dpiY);
            const double Zoom = dpiX / 96.0;

            m_pDC->Flush();

            const D2D1_SIZE_U szBmp = pBmp->GetPixelSize();

            INT32 l = (INT32)floorf(rcSampleInBmp.left * (float)Zoom);
            INT32 t = (INT32)floorf(rcSampleInBmp.top * (float)Zoom);
            INT32 r = (INT32)ceilf(rcSampleInBmp.right * (float)Zoom);
            INT32 b = (INT32)ceilf(rcSampleInBmp.bottom * (float)Zoom);

            if (l < 0) l = 0;
            if (t < 0) t = 0;
            if (r > (INT32)szBmp.width)  r = (INT32)szBmp.width;
            if (b > (INT32)szBmp.height) b = (INT32)szBmp.height;
            if (r <= l) r = l + 1;
            if (b <= t) b = t + 1;

            const UINT32 W_phys = (UINT32)(r - l);
            const UINT32 H_phys = (UINT32)(b - t);

            const float srcLeftDip = l / (float)Zoom;
            const float srcTopDip = t / (float)Zoom;
            const float W_dip = W_phys / (float)Zoom;
            const float H_dip = H_phys / (float)Zoom;

            const float destX = rcTopSample.left + (srcLeftDip - rcSampleInBmp.left);
            const float destY = rcTopSample.top + (srcTopDip - rcSampleInBmp.top);

            ComPtr<ID2D1Bitmap> pBmpEffect;
            hr = m_pDC->CreateBitmap(
                { W_phys, H_phys },
                NULL, 0,
                D2D1::BitmapProperties(pBmp->GetPixelFormat(), dpiX, dpiY),
                &pBmpEffect);

            const D2D1_RECT_U rcU{ (UINT32)l, (UINT32)t, (UINT32)r, (UINT32)b };
            hr = pBmpEffect->CopyFromBitmap(NULL, pBmp.Get(), &rcU);

            ComPtr<ID2D1Effect> pEffect;
            float fDeviation = 10.f * (bgAlpha / 0.4);
            hr = m_pDC->CreateEffect(CLSID_D2D1GaussianBlur, &pEffect);
            pEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
            pEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, fDeviation); // fDeviation = 10 * (bgAlpha / 0.4)

            ComPtr<ID2D1Effect> saturationEffect;
            hr = m_pDC->CreateEffect(CLSID_D2D1Saturation, &saturationEffect);
            float fSaturation = 1.f + 4.f * (bgAlpha / 0.4);
            saturationEffect->SetValue(D2D1_SATURATION_PROP_SATURATION, fSaturation);

            pEffect->SetInput(0, pBmpEffect.Get());
            saturationEffect->SetInputEffect(0, pEffect.Get());

            const auto iBlend = m_pDC->GetPrimitiveBlend();
            m_pDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

            RoundedRectMaskXYD2dDC(m_pDC, eck::g_pD2DFactory,
                D2D1::RectF(destX, destY, destX + W_dip, destY + H_dip),
                m_roundedRectMaskBorderRadiusX, m_roundedRectMaskBorderRadiusY); // borderRadius = 0
            m_pDC->Clear(D2D1::ColorF(0x000000, 0));
            m_pDC->DrawImage(saturationEffect.Get(), D2D1::Point2F(destX, destY));

            m_pDC->PopLayer();

            m_pDC->SetPrimitiveBlend(iBlend);
        }

        if (m_roundedRectMaskBorderRadiusX > 1) {

            if (m_isMaskFlip) {
                D2D1_RECT_F rc = D2D1::RectF(roundedRectMask.left, cy - (roundedRectMask.bottom - roundedRectMask.top), roundedRectMask.right, cy);

                RoundedRectMaskXYD2dDC(m_pDC, eck::g_pD2DFactory, rc, m_roundedRectMaskBorderRadiusX, m_roundedRectMaskBorderRadiusY);
            }
            else {
                RoundedRectMaskXYD2dDC(m_pDC, eck::g_pD2DFactory, roundedRectMask, m_roundedRectMaskBorderRadiusX, m_roundedRectMaskBorderRadiusY);
            }

            m_pBrush->SetColor(D2D1::ColorF(backgroundColor, bgAlpha));
            m_pDC->FillRoundedRectangle(D2D1::RoundedRect(GetViewRectF(), 0, 0), m_pBrush);

            m_pDC->PopLayer();
        }
        else {
            m_pBrush->SetColor(D2D1::ColorF(backgroundColor, bgAlpha));
            m_pDC->FillRoundedRectangle(D2D1::RoundedRect(GetViewRectF(), 0, 0), m_pBrush);
        }

        ECK_DUI_DBG_DRAW_FRAME;
        EndPaint(ps);
    }
    return 0;
    case WM_CREATE: 
    {
        m_pDC->CreateSolidColorBrush({}, &m_pBrush);
    }
    break;
    case WM_DESTROY:
    {
        SafeRelease(m_pBrush);
    }
    break;
    }
    return __super::OnEvent(uMsg, wParam, lParam);
}