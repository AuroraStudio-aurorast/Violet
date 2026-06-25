#include "pch.h"
#include "CWndMain.h"
#include "CApp.h"

// 从旧Violet项目里偷的，仅供测试
#pragma comment(lib, "Rpcrt4.lib")
static std::wstring generateUUID() 
{
    UUID uuid;
    UuidCreate(&uuid);
    RPC_WSTR uuidStr = NULL;
    UuidToString(&uuid, &uuidStr);
    std::wstring result = reinterpret_cast<wchar_t*>(uuidStr);
    RpcStringFree(&uuidStr);
    return result;
}

void CPageOptions::SwitchPage(int index)
{
    m_TBLOpt.SelectItemForClick(index);
}

void CPageOptions::UpdateTitle()
{
    m_lpTitle[0] = L"通用";
    m_lpTitle[1] = m_AppLang.LANG_ID_SETTINGS_APPEARANCE;
    m_lpTitle[2] = m_AppLang.LANG_ID_SETTINGS_ABOUT;
}

void CPageOptions::UpdateImg()
{
    m_pImg[0] = ((CWndMain*)GetWnd())->RealizeImage(GImg::Settings);
    m_pImg[1] = ((CWndMain*)GetWnd())->RealizeImage(GImg::Settings);
    m_pImg[2] = ((CWndMain*)GetWnd())->RealizeImage(GImg::About);
}

LRESULT CPageOptions::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        const auto pWnd = (CWndMain*)GetWnd();
        size_t cardIndex = 0;
        size_t pageIndex = 0;

        UpdateTitle();
        UpdateImg();

        m_TBLOpt.Create(nullptr, Dui::DES_VISIBLE, 0,
            0, 0, CxListFileList, 0, this, GetWnd());
        m_TBLOpt.SetItemCount(3);
        m_TBLOpt.SetBottomExtraSpace(CyPlayPanel);
        m_TBLOpt.ReCalc();
        m_LytSidebar.Add(&m_TBLOpt, {}, eck::LF_FILL, 1);
        m_Lyt.Add(&m_LytSidebar, { .cxRightWidth = (int)CxPageIntPadding, .cyTopHeight = (int)(CyPageTitle + DTopPageTitle + CxPageIntPadding) },
            eck::LF_FIX_WIDTH | eck::LF_FILL_HEIGHT);

        // 通用
        m_GenOpt.Create(nullptr, 0, 0, 0, 0, 0, 0, this);
        m_GenOpt.SetOffset(0, CyPlayPanel);
        m_Pages[pageIndex] = &m_GenOpt;
        cardIndex = 0;

        m_GenOpt.AddCard(0);
        m_GenOpt.SetCardTitle(cardIndex, L"语言", L"界面显示的语言");
        m_GenOpt.SetCardIcon(cardIndex, pWnd->RealizeImage(GImg::Language));
        cardIndex++;

        pageIndex++;

        // 外观
        m_SkinOpt.Create(nullptr, 0, 0, 0, 0, 0, 0, this);
        m_SkinOpt.SetOffset(0, CyPlayPanel);
        m_Pages[pageIndex] = &m_SkinOpt;
        cardIndex = 0;

        m_SkinOpt.AddCard(0);
        m_SkinOpt.SetCardTitle(cardIndex, L"主题", L"设置主题模式");
        m_SkinOpt.SetCardIcon(cardIndex, pWnd->RealizeImage(GImg::Personalize));
        cardIndex++;
        m_SkinOpt.AddCard(0);
        m_SkinOpt.SetCardTitle(cardIndex, L"背景", L"窗口背景类型");
        m_SkinOpt.SetCardIcon(cardIndex, pWnd->RealizeImage(GImg::Window));
        cardIndex++;

        pageIndex++;

        // 关于
        m_AboutOpt.Create(nullptr, 0, 0, 0, 0, 0, 0, this);
        m_AboutOpt.SetOffset(0, CyPlayPanel);
        m_Pages[pageIndex] = &m_AboutOpt;
        cardIndex = 0;

        m_AboutOpt.AddCard(-1);
        m_AboutOpt.SetCardFoldConHeight(cardIndex, 180);
        m_AboutOptLogo.Create(nullptr, Dui::DES_VISIBLE, 0,
            0, 0, 0, 180, m_AboutOpt.GetCardFoldCon(cardIndex));
        m_AboutOptLogo.SetBorderRadius(8, 8);
        m_AboutOptLogo.SetBitmap(pWnd->RealizeImage(GImg::AboutBg));
        cardIndex++;
        m_AboutOpt.AddCard(0);
        m_AboutOpt.SetCardToolConWidth(cardIndex, 125);
        m_AboutOpt.SetCardTitle(cardIndex, L"Violet", L"ortus_prerelease 内部测试");
        m_AboutOpt.SetCardIcon(cardIndex, pWnd->RealizeImage(GImg::BigLogo));
        m_AboutOptStudioLogo.Create(nullptr, Dui::DES_VISIBLE, 0,
            0, 10, 125, 35, m_AboutOpt.GetCardToolCon(cardIndex));
        m_AboutOptStudioLogo.SetBitmap(pWnd->RealizeImage(GImg::Aurorast));
        cardIndex++;
        m_AboutOpt.AddCard(0);
        m_AboutOpt.SetCardToolConWidth(cardIndex, 100);
        m_AboutOpt.SetCardTitle(cardIndex, L"会话 ID", generateUUID());
        m_BTAboutOptRgUUID.Create(L"重新生成", Dui::DES_VISIBLE, 0,
            0, 10, 100, 35, m_AboutOpt.GetCardToolCon(cardIndex));
        m_BTAboutOptRgUUID.showBorder(true);
        cardIndex++;
        m_AboutOpt.AddCard(2);
        m_AboutOpt.SetCardFoldConHeight(cardIndex, 1000);
        m_AboutOpt.SetCardTitle(cardIndex, L"使用的项目", L"");
        cardIndex++;

        pageIndex++;

        m_Lyt.Add(&m_LytPage, { .cxRightWidth = (int)CxPageIntPadding, .cyTopHeight = (int)(CyPageTitle + DTopPageTitle + CxPageIntPadding) }, eck::LF_FILL, 1);
        SwitchPage(0);
        break;
    }
    case WM_SIZE:
    {
        m_Lyt.Arrange((int)GetWidthF(), (int)GetHeightF());
        m_AboutOptLogo.SetSize(m_AboutOptLogo.GetParentElem()->GetWidthF(), 180);
        break;
    }
    case WM_NOTIFY:
    {
        if (wParam == (WPARAM)&m_TBLOpt)
            switch (((Dui::DUINMHDR*)lParam)->uCode)
            {
            case Dui::TBLE_GETDISPINFO:
            {
                const auto p = (Dui::NMTBLDISPINFO*)lParam;
                if (p->uMask & eck::DIM_TEXT)
                {
                    const eck::CRefStrW& rsName = m_lpTitle[p->idx];
                    p->cchText = rsName.CopyTo((PWSTR)p->pszText, p->cchText);
                }
                if (p->uMask & eck::DIM_IMAGE)
                    p->pImage = m_pImg[p->idx];
                return 0;
            }
            case Dui::TBLE_SELCHANGED:
            {
                const auto* const p = (Dui::NMTBLITEMINDEX*)lParam;
                if (p->idx < 0)
                    break;
                m_Pages[m_nPage]->SetVisible(false);
                m_nPage = p->idx;
                m_Pages[m_nPage]->SetVisible(true);
                m_LytPage.Clear();
                m_LytPage.Add(m_Pages[m_nPage], {}, eck::LF_FILL, 1);
                m_Lyt.Arrange((int)GetWidthF(), (int)GetHeightF());
                InvalidateRect();
                return 0;
            }
            }
        else if (wParam == (WPARAM)&m_BTAboutOptRgUUID)
            switch (((Dui::DUINMHDR*)lParam)->uCode)
            {
            case Dui::EE_COMMAND:
            {
                m_AboutOpt.SetCardTitle(2, L"会话 ID", generateUUID());
                InvalidateRect();
                return 0;
            }
            }
        break;
    }
    case WM_SETFONT:
    {
        m_TBLOpt.SetTextFormat(GetTextFormat());
        m_GenOpt.SetTextFormat(GetTextFormat());
        m_SkinOpt.SetTextFormat(GetTextFormat());
        m_AboutOpt.SetTextFormat(GetTextFormat());
        m_BTAboutOptRgUUID.SetTextFormat(GetTextFormat());
        break;
    }
    case WM_THEMECHANGED:
    {
        UpdateImg();
        break;
    }
    case WM_DESTROY:
        break;
    };
    return __super::OnEvent(uMsg, wParam, lParam);
}