#include "pch.h"
#include "CVeDuiCard.h"
#include "Utils.h"

LRESULT CVeCardCon::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_NOTIFY:
    {
        CVeCard* cardparent = nullptr;
        CVeCardGroup* cardgroupparent = nullptr;
        Dui::CElem* mainparent = nullptr;
        if (!(cardparent = dynamic_cast<CVeCard*>(GetParentElem())))
            break;
        if (!(cardgroupparent = dynamic_cast<CVeCardGroup*>(cardparent->GetParentElem())))
            break;
        if (!(mainparent = dynamic_cast<Dui::CElem*>(cardgroupparent->GetParentElem())))
            break;
        return mainparent->OnEvent(uMsg, wParam, lParam);
    }
    case WM_MOUSEWHEEL:
    {
        CVeCard* parent = nullptr;
        if (parent = dynamic_cast<CVeCard*>(GetParentElem()))
            parent->OnEvent(uMsg, wParam, lParam);
        break;
    }
    }
    return __super::OnEvent(uMsg, wParam, lParam);
}

LRESULT CVeCard::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        if (m_nType == -1)
            return 0;

        Dui::ELEMPAINTSTRU ps;
        BeginPaint(ps, wParam, lParam);

        D2D1_COLOR_F cr;
        D2D1_RECT_F rcF;
        const float cxElem = GetWidthF();
        const float cyElem = GetHeightF();

        rcF.left = 0.f;
        rcF.top = 0.f;
        rcF.right = rcF.left + cxElem;
        rcF.bottom = rcF.top + cyElem;
        GetTheme()->GetSysColor(Dui::SysColor::Bk, cr);
        cr.a = 0.5;
        m_pBrush->SetColor(D2D1::ColorF(backgroundColor, 0.5));
        m_pDC->FillRoundedRectangle(D2D1::RoundedRect(rcF, 8, 8), m_pBrush);
        m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.15));
        m_pDC->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(rcF.left + 0.5, rcF.top + 0.5,rcF.right - 0.5,rcF.bottom - 0.5), 8, 8), m_pBrush);

        D2D1_RECT_F textrcF = rcF;
        textrcF.bottom = m_fTitleHeight;
        textrcF.left += 12;
        if (m_pBmp)
        {
            D2D1_RECT_F imgrcF;
            imgrcF.left = textrcF.left;
            imgrcF.top = textrcF.top + (m_fTitleHeight - 24) / 2;
            imgrcF.right = textrcF.left + 24;
            imgrcF.bottom = textrcF.top + (m_fTitleHeight - 24) / 2 + 24;
            m_pDC->DrawBitmap(m_pBmp, imgrcF, 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            textrcF.left += 38;
        }
        auto textFormat = GetTextFormat();
        if (textFormat)
        {
            const auto pa = textFormat->GetParagraphAlignment();
            if (!m_lpTitle1.empty())
            {
                GetTheme()->GetSysColor(Dui::SysColor::Text, cr);
                m_pBrush->SetColor(cr);
                textFormat->SetParagraphAlignment(m_lpTitle2.empty() ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_FAR);
                m_pDC->DrawTextW(m_lpTitle1.c_str(), m_lpTitle1.size(), textFormat,
                    { textrcF.left, textrcF.top, textrcF.right, textrcF.top + (m_lpTitle2.empty() ? m_fTitleHeight : m_fTitleHeight / 2) },
                    m_pBrush);
            }
            if (!m_lpTitle2.empty())
            {
                m_pBrush->SetColor({ 0.5, 0.5, 0.5, 1 });
                textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                m_pDC->DrawTextW(m_lpTitle2.c_str(), m_lpTitle2.size(), textFormat,
                    { textrcF.left, textrcF.top + m_fTitleHeight / 2, textrcF.right, textrcF.bottom },
                    m_pBrush);
            }
            textFormat->SetParagraphAlignment(pa);
        }
        if (m_nType == 2) 
        {
            const float arx = textrcF.right - 15 - 16, ary = textrcF.top + (m_fTitleHeight - 16) / 2;
            GetTheme()->GetSysColor(Dui::SysColor::Text, cr);
            m_pBrush->SetColor(cr);
            if (m_bFold)
            {
                m_pDC->DrawLine({ arx, ary + 4 }, { arx + 8, ary + 12 }, m_pBrush, 2.f);
                m_pDC->DrawLine({ arx + 8, ary + 12 }, { arx + 16, ary + 4 }, m_pBrush, 2.f);
            }
            else 
            {
                m_pDC->DrawLine({ arx, ary + 12 }, { arx + 8, ary + 4 }, m_pBrush, 2.f);
                m_pDC->DrawLine({ arx + 8, ary + 4 }, { arx + 16, ary + 12 }, m_pBrush, 2.f);
            }
        }
        if (m_nType == 2 && !m_bFold)
        {
            m_pBrush->SetColor({ 0.5, 0.5, 0.5, 0.75 });
            m_pDC->DrawLine({ 0, textrcF.bottom + 0.5f }, { textrcF.right, textrcF.bottom + 0.5f }, m_pBrush);
        }
        ECK_DUI_DBG_DRAW_FRAME;
        EndPaint(ps);
        return 0;
    }
    case WM_CREATE:
    {
        m_pToolCon.Create(nullptr, Dui::DES_VISIBLE, 0, 0, 0, 0, 0, this);
        m_pFoldCon.Create(nullptr, Dui::DES_VISIBLE, 0, 0, 55, 0, 0, this);
        m_pDC->CreateSolidColorBrush({}, &m_pBrush);
        break;
    }
    case WM_SIZE:
    {
        const float cxElem = GetWidthF();
        const float cyElem = GetHeightF();
        switch (m_nType)
        {
        case -1:
            m_fTitleHeight = 0;
            break; 
        case 0:
        case 1:
        case 2:
            m_fTitleHeight = 55;
            break;
        }
        m_pToolCon.SetPos(cxElem - m_fToolConWidth - 12 - (m_nType == 2 ? 15 + 16 : 0), 0);
        m_pToolCon.SetSize(m_fToolConWidth, m_fTitleHeight);
        m_pFoldCon.SetPos(0, m_fTitleHeight);
        m_pFoldCon.SetSize(cxElem, m_fFoldConHeight);
        break;
    }
    case WM_LBUTTONUP:
    {
        if (m_nType == 1)
        {
            Dui::DUINMHDR nm{ Dui::EE_COMMAND };
            GenElemNotify(&nm);
        }
        else if (m_nType == 2) 
        {
            m_bFold = !m_bFold;
            Dui::DUINMHDR nm{ ELEN_CARD_FOLD };
            if (!GenElemNotify(&nm))
                InvalidateRect();
        }
        break;
    }
    case WM_MOUSEWHEEL:
    {
        /*
        * 由于框架限制（也可能有别的方法我不知道
        * 只有卡片控件自身可以滚轮滚动界面
        * 卡片内的子控件没法滚轮滚动界面
        */
        CVeCardGroup* parent = nullptr;
        if (parent = dynamic_cast<CVeCardGroup*>(GetParentElem()))
            parent->OnEvent(uMsg, wParam, lParam);
        break;
    }
    case WM_DESTROY:
    {
        m_pToolCon.Destroy();
        m_pFoldCon.Destroy();
        SafeRelease(m_pBmp);
        SafeRelease(m_pBrush);
        break;
    }
    }
    return __super::OnEvent(uMsg, wParam, lParam);
}

void CVeCardGroup::CardUpdatePos()
{
    m_fListPos = m_psv->GetPos();
    const float cxElem = GetWidthF(), cyElem = GetHeightF();
    float cardTop = m_fTopOffset - m_fListPos;
    for (auto& card : m_pCards)
    {
        const float cardHeight = card->GetCardHeight();
        card->SetPos(0, cardTop);
        cardTop += cardHeight + 10;
    }
    InvalidateRect();
}

void CVeCardGroup::MouseWheel(WPARAM iWheelDelta)
{
    ECK_DUILOCK;
    m_psv->OnMouseWheel2(-GET_WHEEL_DELTA_WPARAM(iWheelDelta) / WHEEL_DELTA);
    CardUpdatePos();
}

LRESULT CVeCardGroup::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        m_SB.Create(nullptr, 0, 0,
            0, 0, GetTheme()->GetMetrics(Dui::Metrics::CxVScroll), 0,
            this);
        m_SB.SetVisible(true);
        m_psv = m_SB.GetScrollView();
        m_psv->AddRef();
        m_psv->SetMinThumbSize(Dui::CxyMinScrollThumb);
        m_psv->SetCallBack([](float fPos, float fPrevPos, LPARAM lParam)
            {
                ((CVeCardGroup*)lParam)->CardUpdatePos();
            }, (LPARAM)this);
        m_psv->SetDelta(80);
        break;
    }
    case WM_SIZE:
    {
        const float cxElem = GetWidthF(), cyElem = GetHeightF();
        D2D1_RECT_F sbrc;
        sbrc.left = cxElem - m_SB.GetWidthF();
        sbrc.top = m_fTopOffset;
        sbrc.right = sbrc.left + m_SB.GetWidthF();
        sbrc.bottom = cyElem - m_fBottomOffset;
        m_SB.SetRect(sbrc);
        float cardTop = 0;
        for (auto& card : m_pCards)
        {
            const float cardHeight = card->GetCardHeight();
            card->SetSize(cxElem - m_SB.GetWidthF() - 10, cardHeight);
            cardTop += cardHeight + 10;
        }
        m_fListHeight = cardTop;
        m_psv->SetPage(cyElem - (m_fTopOffset + m_fBottomOffset));
        m_psv->SetRange(0, m_fListHeight);
        CardUpdatePos();
        break;
    }
    case WM_MOUSEWHEEL:
    {
        MouseWheel(wParam);
        break;
    }
    case WM_SETFONT:
    {
        for (auto& card : m_pCards)
        {
            card->SetTextFormat(GetTextFormat());
        }
        break;
    }
    case WM_NOTIFY:
    {
        ECK_DUILOCK;
        if ((wParam == (WPARAM)&m_SB) &&
            (((eck::Dui::DUINMHDR*)lParam)->uCode == eck::Dui::EE_VSCROLL))
        {
            CardUpdatePos();
            return TRUE;
        }
        else 
        {
            for (auto& card : m_pCards)
            {
                if ((wParam == (WPARAM)card) &&
                    (((eck::Dui::DUINMHDR*)lParam)->uCode == ELEN_CARD_FOLD))
                {
                    OnEvent(WM_SIZE, 0, 0);
                    break;
                }
                else if ((wParam == (WPARAM)card) &&
                    (((eck::Dui::DUINMHDR*)lParam)->uCode == Dui::EE_COMMAND))
                {
                    Dui::CElem* mainparent = nullptr;
                    if (mainparent = dynamic_cast<Dui::CElem*>(GetParentElem()))
                        return mainparent->OnEvent(uMsg, wParam, lParam);
                }
            }
        }
        break;
    }
    case WM_DESTROY:
    {
        for (auto& card : m_pCards)
        {
            card->Destroy();
            delete card;
        }
        m_pCards.clear();
        break;
    }
    }
    return __super::OnEvent(uMsg, wParam, lParam);
}