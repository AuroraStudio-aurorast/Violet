#include "pch.h"
#include "CPagePlaying.h"
#include "CApp.h"
#include "CWndMain.h"
#include "CLrGeometryRealization.h"

void CPagePlaying::UpdateBlurredCover()
{
	ECK_DUILOCK;
	const auto cxElem = Log2PhyF(GetWidthF());
	const auto cyElem = Log2PhyF(GetHeightF());
	if (!cxElem || !cyElem)
		return;
	ComPtr<IWICBitmap> pWicCover;
	App->GetPlayer().GetCover(pWicCover.RefOf());
	if (!pWicCover.Get())
		pWicCover = App->GetImg(GImg::DefaultCover);
	ComPtr<ID2D1Image> pOldTarget;
	m_pDC->GetTarget(&pOldTarget);
	m_pDC->SetTarget(m_pBmpBlurredCover);
	m_pDC->SetTransform(D2D1::Matrix3x2F::Identity());
	m_pDC->BeginDraw();
	m_pDC->Clear(D2D1::ColorF(D2D1::ColorF::White));// TODO:主题色

	UINT cx0, cy0;	// 原始大小
	float cyRgn;	// 截取区域高
	float cx;		// 缩放后图片宽
	float cy;		// 缩放后图片高
	D2D_POINT_2F pt;// 画出位置
	// 情况一，客户区宽为最大边    cxClient / cxPic = cyClient / cyRgn
	// 情况二，客户区高为最大边    cyClient / cyPic = cxClient / cxRgn
	pWicCover->GetSize(&cx0, &cy0);
	cyRgn = cyElem / cxElem * (float)cx0;
	if (cyRgn < cy0)// 情况一
	{
		cx = cxElem;
		cy = cx * cy0 / cx0;
		pt = { 0.f,(cyElem - cy) / 2 };
	}
	else// 情况二
	{
		cy = (float)cyElem;
		cx = cx0 * cy / cy0;
		pt = { (cxElem - cx) / 2,0.f };
	}
	//---缩放
	ComPtr<IWICBitmap> pWicBmpScaled;
	eck::ScaleWicBitmap(pWicCover.Get(), pWicBmpScaled.RefOf(), (int)cx, (int)cy,
		WICBitmapInterpolationModeNearestNeighbor);
	SafeRelease(m_pBmpCover);
	m_pDC->CreateBitmapFromWicBitmap(pWicBmpScaled.Get(), &m_pBmpCover);
	m_Cover.SetBitmap(m_pBmpCover);
	m_CoverImg.SetBitmap(m_pBmpCover);
	//---模糊 
	ComPtr<ID2D1Effect> pEffect;
	ComPtr<ID2D1Effect> saturationEffect;
	m_pDC->CreateEffect(CLSID_D2D1GaussianBlur, &pEffect);
	pEffect->SetInput(0, m_pBmpCover);
	pEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 40.f);
	pEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
	pEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION,
		D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
	m_pDC->CreateEffect(CLSID_D2D1Saturation, &saturationEffect);
	saturationEffect->SetValue(D2D1_SATURATION_PROP_SATURATION, 4.f);
	saturationEffect->SetInputEffect(0, pEffect.Get());

	GetWnd()->Phy2Log(pt);
	m_pDC->DrawImage(saturationEffect.Get(), pt);
	//---半透明遮罩
	m_pBrBkg->SetColor(App->GetColor(GPal::PlayPageOverlay));
	m_pDC->FillRectangle(GetViewRectF(), m_pBrBkg);

	m_pDC->EndDraw();
	m_pDC->SetTarget(pOldTarget.Get());
}

void CPagePlaying::OnPlayEvent(const PLAY_EVT_PARAM& e)
{
	switch (e.eEvent)
	{
	case PlayEvt::CommTick:
	{
		m_Lrc.LrcSetCurrentLine(App->GetPlayer().GetCurrLrcIdx());
	}
	break;
	case PlayEvt::Play:
	{
		UpdateBlurredCover();
		InvalidateRect();
		const auto& mi = App->GetPlayer().GetMusicInfo();
		m_LATitle.SetText(mi.rsTitle.Data());
		PCWSTR album = L"💿";
		PCWSTR albumName = mi.rsAlbum.Data();
		std::wstring albumResult = std::wstring(album) + std::wstring(albumName);
		m_LAAlbum.SetText(albumResult.c_str());
		PCWSTR artist = L"👤";
		PCWSTR artistName = mi.slArtist.FrontData();
		std::wstring artistResult = std::wstring(artist) + std::wstring(artistName);
		m_LAArtist.SetText(artistResult.c_str());

		ComPtr<Lyric::CLyric> pLyric;
		App->GetPlayer().GetLrc(pLyric.RefOf());
		m_Lrc.LrcInit(pLyric.Get());
		m_Lrc.UpdateUI();
	}
	break;
	case PlayEvt::Stop:
	{
		m_Lrc.LrcClear();
		SetEmptyText();
	}
	break;
	}
}

void CPagePlaying::SetEmptyText()
{
	m_LATitle.SetText(L"Violet Player");
	m_LAArtist.SetText(L" ");
	m_LAAlbum.SetText(L"AuroraStudio");
}

void CPagePlaying::OnColorSchemeChanged()
{
	D2D1_COLOR_F crText;
	GetTheme()->GetSysColor(Dui::SysColor::Text, crText);

	m_LATitle.SetColor(crText);
	m_LATitle.UpdateFadeColor();
	m_LAAlbum.SetColor(D2D1::ColorF(foregroundColor, 0.7));
	m_LAAlbum.UpdateFadeColor();
	m_LAArtist.SetColor(D2D1::ColorF(foregroundColor, 0.7));
	m_LAArtist.UpdateFadeColor();

	m_BTBack.SetBitmap(((CWndMain*)GetWnd())->RealizeImage(GImg::PlayPageDown));

	const D2D1_COLOR_F crLrc[CVeLrc::CriMax]
	{
		App->GetColor(GPal::LrcTextNormal),
		App->GetColor(GPal::LrcTextHighlight),
	};
	m_Lrc.LrcSetColor(crLrc);
}

LRESULT CPagePlaying::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_PAINT:
	{
		Dui::ELEMPAINTSTRU ps;
		BeginPaint(ps, wParam, lParam);
		RoundedRectMaskXYD2dDC(m_pDC, eck::g_pD2DFactory, ps.rcfClip, radiusX, radiusY);

		m_pDC->DrawBitmap(m_pBmpBlurredCover, ps.rcfClipInElem,
			1.f, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, ps.rcfClipInElem);

		m_pDC->PopLayer();
		EndPaint(ps);
	}
	return 0;

	case WM_SIZE:
	{
		const auto cxF = GetWidthF();
		const auto cyF = GetHeightF();
		if (m_pBmpBlurredCover)
		{
			const auto size = m_pBmpBlurredCover->GetSize();
			if (!(size.width < cxF || size.width / 2.f > cxF ||
				size.height < cyF || size.height / 2.f > cyF))
				goto Update;
			SafeRelease(m_pBmpBlurredCover);
			GetWnd()->BmpNewLogSize(cxF, cyF, m_pBmpBlurredCover);
		}
		else
			GetWnd()->BmpNewLogSize(cxF, cyF, m_pBmpBlurredCover);
	Update:;
		UpdateBlurredCover();
		const auto cx = GetWidthF();
		const auto cy = GetHeightF();
		const auto cxMinGap = cx * 1.f / 20.f;

		D2D1_RECT_F rcCover;
		rcCover.left = cx * 1 / 11;
		rcCover.top = cy * 19 / 100;
		const auto cxyCover = std::min(
			cx * 10 / 20 - cxMinGap - rcCover.left,
			cy * 7 / 20);
		rcCover.right = rcCover.left + cxyCover;
		rcCover.bottom = rcCover.top + cxyCover;
		m_Cover.SetRect(rcCover);

		const D2D1_RECT_F rcLrc{ rcCover.right + rcCover.left, DLrcTop, cx - 50, cy - DLrcBottom };
		m_Lrc.SetRect(rcLrc);

		D2D1_RECT_F rcLabel{ rcCover };
		rcLabel.top = rcCover.bottom + CyPlayPageLabelCoverPadding;
		rcLabel.bottom = rcLabel.top + CyPlayPageLabel;
		m_LATitle.SetRect(rcLabel);
		rcLabel.top = rcLabel.bottom + CyPlayPageLabelPadding;
		rcLabel.bottom = rcLabel.top + CyPlayPageLabel;
		m_LAAlbum.SetRect(rcLabel);
		rcLabel.top = rcLabel.bottom + CyPlayPageLabelPadding;
		rcLabel.bottom = rcLabel.top + CyPlayPageLabel;
		m_LAArtist.SetRect(rcLabel);

		const auto dBackBtnMar = (CxyMiniCover - CxyBackBtn) / 2;
		D2D1_RECT_F rcBackBtn;
		rcBackBtn.left = DLeftMiniCover + dBackBtnMar;
		rcBackBtn.top = cy - CyPlayPanel + DTopMiniCover + dBackBtnMar;
		rcBackBtn.right = rcBackBtn.left + CxyBackBtn;
		rcBackBtn.bottom = rcBackBtn.top + CxyBackBtn;
		m_BTBack.SetRect(rcBackBtn);

		m_CoverImg.SetRect(D2D1::RectF(0.f, 0.f, cx, cy));
	}
	break;
	case WM_DPICHANGED:
	{
		const auto cx = GetWidthF();
		const auto cy = GetHeightF();
		SafeRelease(m_pBmpBlurredCover);
		GetWnd()->BmpNewLogSize(cx, cy, m_pBmpBlurredCover);
		UpdateBlurredCover();
	}
	break;
	case WM_SETFONT:
	{
		const auto pTf = GetTextFormat();
		m_LATitle.SetTextFormat(pTf);
		m_LAAlbum.SetTextFormat(pTf);
		m_LAArtist.SetTextFormat(pTf);
	}
	break;
	case Dui::EWM_COLORSCHEMECHANGED:
	{
		OnColorSchemeChanged();
		UpdateBlurredCover();
		InvalidateRect();
	}
	break;
	case WM_CREATE:
	{
		App->GetPlayer().GetSignal().Connect(this, &CPagePlaying::OnPlayEvent);
		/*
		App->GetFontFactory().NewFont(pTfTitle.RefOf(), eck::Align::Near,
			eck::Align::Center, (float)CyFontPlayPageTitle);
		App->GetFontFactory().NewFont(pTfSubtitle.RefOf(), eck::Align::Near,
			eck::Align::Center, (float)CyFontPlayPageSubtitle);
		pTfTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		pTfSubtitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		*/
		m_Cover.Create(nullptr, Dui::DES_VISIBLE, 0,
			50, 50, 200, 200, this);
		const auto pLrcRenderer = new CLrGeometryRealization{};
        m_Lrc.LrcSetRenderer(pLrcRenderer);
		pLrcRenderer->Release();
		m_Lrc.Create(nullptr, Dui::DES_VISIBLE, 0,
			50, 260, 200, 100, this);

		m_LATitle.Create(nullptr, Dui::DES_VISIBLE, 0,
			0, 0, 0, 0, this);
		m_LATitle.SetFade(TRUE);
		m_LATitle.SetTextFormat(pTfTitle.Get());

		m_LAAlbum.Create(nullptr, Dui::DES_VISIBLE, 0,
			0, 0, 0, 0, this);
		m_LAAlbum.SetFade(TRUE);
		m_LAAlbum.SetColor(D2D1::ColorF(foregroundColor, 0.5));
		m_LAAlbum.SetTextFormat(pTfSubtitle.Get());

		m_LAArtist.Create(nullptr, Dui::DES_VISIBLE, 0,
			0, 0, 0, 0, this);
		m_LAArtist.SetFade(TRUE);
		m_LAArtist.SetColor(D2D1::ColorF(foregroundColor, 0.5));
		m_LAArtist.SetTextFormat(pTfSubtitle.Get());

		m_BTBack.Create(nullptr, Dui::DES_VISIBLE | Dui::DES_NOTIFY_TO_WND, 0,
			100, 100, 70, 20, this);
		m_BTBack.SetID(ELEID_PLAYPAGE_BACK);
		//m_BTBack.SetTheme(((CWndMain*)GetWnd())->GetVioletTheme());

		m_CoverImg.Create(nullptr, Dui::DES_VISIBLE, 0,
			50, 50, 200, 200, this);

		OnColorSchemeChanged();

		ComPtr<IDWriteTextFormat> pTfLrc;
		auto& FontFactory = App->GetFontFactory();;
		FontFactory.NewFont(pTfLrc.RefOfClear(),
			eck::Align::Near, eck::Align::Near, 20, 700);
		m_Lrc.SetTextFormat(pTfLrc.Get());
		FontFactory.NewFont(pTfLrc.RefOfClear(),
			eck::Align::Near, eck::Align::Near, 18, 500);
		m_Lrc.SetTextFormatTrans(pTfLrc.Get());
		//m_Lrc.GetScrollBar().SetTheme(((CWndMain*)GetWnd())->GetVioletTheme());
		
		SetEmptyText();

		m_pDC->CreateSolidColorBrush({}, &m_pBrBkg);
	}
	break;
	case WM_DESTROY:
	{
		SafeRelease(m_pBmpBlurredCover);
		SafeRelease(m_pBmpCover);
		SafeRelease(m_pBrBkg);
	}
	break;
	}
	return __super::OnEvent(uMsg, wParam, lParam);
}