#include "pch.h"
#include "CVeDuiImage.h"
#include "Utils.h"

LRESULT CVeImage::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        Dui::ELEMPAINTSTRU ps;
        BeginPaint(ps, wParam, lParam);

        if (m_pBmp)
        {
            float f;
            const auto size = m_pBmp->GetSize();
            const float cx0 = size.width, cy0 = size.height;
            D2D1_RECT_F rcF;

            const float cxElem = GetWidthF();
            const float cyElem = GetHeightF();
            /*
            if (cxElem / cyElem > cx0 / cy0)// y¶ÔÆë
            {
                f = cx0 * cyElem / cy0;
                rcF.left = (cxElem - f) / 2.f;
                rcF.right = rcF.left + f;
                rcF.top = 0.f;
                rcF.bottom = rcF.top + cyElem;
            }
            else// x¶ÔÆë
            {
                f = cxElem * cy0 / cx0;
                rcF.left = 0.f;
                rcF.right = rcF.left + cxElem;
                rcF.top = (cyElem - f) / 2.f;
                rcF.bottom = rcF.top + f;
            }
            */
            rcF.left = 0.f;
            rcF.top = 0.f;
            rcF.right = rcF.left + cxElem;
            rcF.bottom = rcF.top + cyElem;
            RoundedRectMaskXYD2dDC(m_pDC, eck::g_pD2DFactory, rcF, radiusX, radiusY);
            m_pDC->DrawBitmap(m_pBmp, &rcF, imageOpacity);
            m_pDC->PopLayer();
        }

        ECK_DUI_DBG_DRAW_FRAME;
        EndPaint(ps);
    }
    return 0;
    case WM_NCHITTEST: {
        return HTTRANSPARENT;
    }
    case WM_DESTROY:
    {
        SafeRelease(m_pBmp);
    }
    break;
    }

    return __super::OnEvent(uMsg, wParam, lParam);
}