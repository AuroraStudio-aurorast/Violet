#include "pch.h"
#include "CVeVolumeBar.h"
#include "CApp.h"
#include "Utils.h"

LRESULT CVeVolumeBar::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_PAINT:
	{
		Dui::ELEMPAINTSTRU ps;
		BeginPaint(ps, wParam, lParam);

		const D2D1_RECT_F rcTop{ 0.f,0.f,GetWidthF(),GetHeightF() };
		D2D1_RECT_F rcTopSample;
		if (eck::IntersectRect(rcTopSample, rcTop, ps.rcfClipInElem))
		{
			D2D1_MATRIX_3X2_F Mat;
			m_pDC->GetTransform(&Mat);
			auto rcSampleInBmp{ rcTopSample };
			eck::OffsetRect(rcSampleInBmp, Mat.dx, Mat.dy);
			BlurD2dDC(m_pDC, eck::g_pD2DFactory, rcSampleInBmp,
				{ rcTopSample.left,rcTopSample.top }, 15 * m_opacity, 8);
		}

		m_pBrush->SetColor(App->GetColor(GPal::VolBarBk));
		m_pDC->FillRoundedRectangle(D2D1::RoundedRect(ps.rcfClipInElem, 8, 8), m_pBrush);

		m_pBrush->SetColor(App->GetColor(GPal::VolBarBorder));
		m_pDC->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(ps.rcfClipInElem.left + 0.5, ps.rcfClipInElem.top + 0.5, ps.rcfClipInElem.right - 0.5, ps.rcfClipInElem.bottom - 0.5), 8, 8), m_pBrush, 1.0f);
		EndPaint(ps);
	}
	return 0;
	case WM_SETFONT:
		m_LAVol.SetTextFormat(GetTextFormat());
		break;
	case WM_CREATE:
	{
		m_pecShowing = new eck::CEasingCurve{};
		InitEasingCurve(m_pecShowing);
		m_pecShowing->SetDuration(500);
		m_pecShowing->SetAnProc(eck::Easing::OutExpo);
		m_pecShowing->SetCallBack([](float fCurrValue, float fOldValue, LPARAM lParam)
			{
				const auto pThis = (CVeVolumeBar*)lParam;
				pThis->m_pPageAn->Opacity = fCurrValue;
				pThis->m_opacity = fCurrValue;
				pThis->m_pPageAn->Dy = (1.f - fCurrValue) * (float)DVolAn;
				const D2D1_RECT_F rcOld = pThis->GetWholeRectInClient();
				pThis->CompReCalcCompositedRect();
				pThis->InvalidateRect(FALSE);
				pThis->GetWnd()->IrUnion(rcOld);
				if (pThis->m_pecShowing->IsStop())
				{
					pThis->SetCompositor(nullptr);
					if (!pThis->m_bShow)
						pThis->SetStyle(pThis->GetStyle() & ~Dui::DES_VISIBLE);
				}
			});
		m_pPageAn = new Dui::CCompositorPageAn{};
		m_pPageAn->InitAsTranslationOpacity();

		m_pDC->CreateSolidColorBrush({}, &m_pBrush);
		const auto cx = GetWidthF();
		const auto cy = GetHeightF();
		m_LAVol.Create(L"100", Dui::DES_VISIBLE | Dui::DES_PARENT_COMP, 0,
			CxVolBarPadding, 0, CxVolLabel, cy, this);
		const auto x = CxVolBarPadding * 2 + CxVolLabel;
		m_TrackBar.Create(nullptr, Dui::DES_VISIBLE |
			Dui::DES_PARENT_COMP | Dui::DES_NOTIFY_TO_WND, 0,
			x, 0, cx - x - CxVolBarPadding, cy, this, nullptr, ELEID_VOLBAR_TRACK);
		m_TrackBar.SetRange(0, 200);
		m_TrackBar.SetTrackPos(100);
		m_TrackBar.SetTrackSize(CyVolTrack);
	}
	break;
	case WM_DESTROY:
		GetWnd()->UnregisterTimeLine(m_pecShowing);

		SafeRelease(m_pBrush);
		SafeRelease(m_pPageAn);
		SafeRelease(m_pecShowing);
		break;
	}
	return __super::OnEvent(uMsg, wParam, lParam);
}

void CVeVolumeBar::ShowAnimation()
{
	ECK_DUILOCK;
	ECKBOOLNOT(m_bShow);
	SetStyle(GetStyle() | Dui::DES_VISIBLE);
	SetCompositor(m_pPageAn);
	if (m_bShow)
		m_pecShowing->Begin(0.f, 1.f);
	else
		m_pecShowing->Begin(1.f, 0.f);
	GetWnd()->WakeRenderThread();
}

void CVeVolumeBar::OnVolChanged(float fVol)
{
	WCHAR szVol[eck::CchI32ToStrBufNoRadix2];
	swprintf(szVol, L"%d", int(fVol));
	m_LAVol.SetText(szVol);
	m_LAVol.InvalidateRect();
}