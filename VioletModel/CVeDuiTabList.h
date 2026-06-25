#pragma once
#include "CVeDuiListTemplate.h"
#include "eck/CD2DImageList.h"
#include "CApp.h"

struct NMTBLDISPINFO : eck::Dui::DUINMHDR
{
	eck::DispInfoMask uMask;
	int idx;
	PCWSTR pszText;
	int cchText;
	int idxImage;
	ID2D1Bitmap* pImage;
};

struct NMTBLITEMINDEX : eck::Dui::DUINMHDR
{
	int idx;
};

class CVeTabList : public CVeListTemplate
{
public:
	constexpr static float
		CxIndicator = 4,
		CxIndicatorPadding = 0,
		CyIndicatorPadding = 11;
protected:
	eck::CEasingCurve* m_pec1{}, * m_pec2{};
	int m_idxTo{ -1 }, m_idxFrom{ -1 };
	int m_idxLastSel{ -1 };
	D2D1_RECT_F m_rcLastRedraw{};

	void LVPaintSubItem(const D2D1_RECT_F& rcPaint, NMLTCUSTOMDRAW& nm, LRESULT r) override
	{
		NMTBLDISPINFO di{ eck::Dui::TBLE_GETDISPINFO };
		di.uMask = eck::DIM_TEXT | eck::DIM_IMAGE;
		di.idxImage = -1;
		di.idx = nm.idx;
		GetText().Reserve(MAX_PATH);
		di.pszText = GetText().Data();
		di.cchText = MAX_PATH;
		GenElemNotify(&di);

		const float Padding = GetTheme()->GetMetrics(eck::Dui::Metrics::SmallPadding);
		const float Padding2 = 9;//GetTheme()->GetMetrics(eck::Dui::Metrics::LargePadding);
		const float cyImg = m_cyItem - Padding2 * 2.f;
		float x = nm.rc.left + CxIndicatorPadding + CxIndicator;
		D2D1_SIZE_F sizeImg;
		D2D1_RECT_F rc;
		if (di.pImage)
		{
			sizeImg = di.pImage->GetSize();
			sizeImg.width = cyImg / sizeImg.height * sizeImg.width;
			sizeImg.height = cyImg;
			x += (sizeImg.width + Padding + nm.rc.left + (m_cxItem - sizeImg.width) / 2.f);
		}
		else if (di.idxImage >= 0)
		{
			float cx, cy;
			m_pImgList->GetImageSize(cx, cy);
			sizeImg.width = cyImg / cy * cx;
			sizeImg.height = cyImg;
			x += (sizeImg.width + Padding + nm.rc.left + (m_cxItem - sizeImg.width) / 2.f);
		}
		else
			goto SkipDrawImg;
		rc.left = nm.rc.left + (m_cxItem - sizeImg.width) / 2.f;
		rc.top = nm.rc.top + (m_cyItem - sizeImg.height) / 2.f;
		rc.right = rc.left + sizeImg.width;
		rc.bottom = rc.top + sizeImg.height;
		if (eck::IsRectsIntersect(rc, rcPaint))
			if (di.pImage)
				m_pDC->DrawBitmap(di.pImage, rc, 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
			else if (di.idxImage >= 0)
				m_pImgList->Draw(di.idxImage, rc, 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
	SkipDrawImg:
		auto& e = m_vItem[nm.idx];
		if (!e.pLayout.Get() && di.pszText && di.cchText > 0)
		{
			const auto cx = nm.rc.right - nm.rc.left - x;
			eck::g_pDwFactory->CreateTextLayout(di.pszText, di.cchText,
				GetTextFormat(), cx, m_cyItem, &e.pLayout);
		}
		if (e.pLayout.Get())
		{
			if (rcPaint.right >= x)
			{
				if (nm.bColorText)
					m_pBrush->SetColor(nm.crText);
				else
				{
					D2D1_COLOR_F cr;
					GetTheme()->GetSysColor(eck::Dui::SysColor::Text, cr);
					m_pBrush->SetColor(cr);
				}
				m_pDC->DrawTextLayout({ x,nm.rc.top }, e.pLayout.Get(),
					m_pBrush, eck::Dui::DrawTextLayoutFlags);
			}
		}
	}

	void LVPaintItem(const D2D1_RECT_F& rcPaint, NMLTCUSTOMDRAW& nm, LRESULT r)
	{
		nm.idxSub = 0;
		nm.bColorText = FALSE;
		if (m_bGroup)
		{
			GetGroupPartRect(ListPart::Item, nm.idx, nm.idxGroup, nm.rc);
			if ((m_Group[nm.idxGroup].Item[nm.idx].uFlags & eck::Dui::LEIF_SELECTED) ||
				(m_bSingleSel && m_idxSel == nm.idx && m_idxSelItemGroup == nm.idxGroup))
				if (nm.idx == m_idxHot && nm.idxGroup == m_idxHotItemGroup)
					nm.eState = eck::Dui::State::HotSelected;
				else
					nm.eState = eck::Dui::State::Selected;
			else if (nm.idx == m_idxHot && nm.idxGroup == m_idxHotItemGroup)
				nm.eState = eck::Dui::State::Hot;
			else
				nm.eState = eck::Dui::State::None;
			if (r & CDRF_NOTIFYITEMDRAW)
			{
				nm.dwStage = CDDS_ITEMPREPAINT;
				r = GenElemNotify(&nm);
			}
			if (!(r & CDRF_SKIPDEFAULT))
			{
				if (nm.eState != eck::Dui::State::None){
					if (nm.eState == eck::Dui::State::Hot) {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
						m_pDC->FillRoundedRectangle(D2D1::RoundedRect(nm.rc, 8, 8), m_pBrush);
					}
					if (nm.eState == eck::Dui::State::Selected) {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
						m_pDC->FillRoundedRectangle(D2D1::RoundedRect(nm.rc, 8, 8), m_pBrush);
					}
					if (nm.eState == eck::Dui::State::HotSelected) {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.2));
						m_pDC->FillRoundedRectangle(D2D1::RoundedRect(nm.rc, 8, 8), m_pBrush);
					}
				}
					//GetTheme()->DrawBackground(eck::Dui::Part::ListItem,
						//nm.eState, nm.rc, nullptr);
				GetGroupSubItemRect(nm.idx, 0, nm.idxGroup, nm.rc);
				LVPaintSubItem(rcPaint, nm, r);
				if (m_eView == Type::Report)
					for (nm.idxSub = 1; nm.idxSub < m_Header.GetItemCount(); ++nm.idxSub)
					{
						GetGroupSubItemRect(nm.idx, nm.idxSub, nm.idxGroup, nm.rc);
						LVPaintSubItem(rcPaint, nm, r);
					}
			}
		}
		else
		{
			GetItemRect(nm.idx, nm.rc);
			if ((m_vItem[nm.idx].uFlags & eck::Dui::LEIF_SELECTED) ||
				(m_bSingleSel && m_idxSel == nm.idx))
				if (m_idxHot == nm.idx)
					nm.eState = eck::Dui::State::HotSelected;
				else
					nm.eState = eck::Dui::State::Selected;
			else if (m_idxHot == nm.idx)
				nm.eState = eck::Dui::State::Hot;
			else
				nm.eState = eck::Dui::State::None;
			if (r & CDRF_NOTIFYITEMDRAW)
			{
				nm.dwStage = CDDS_ITEMPREPAINT;
				r = GenElemNotify(&nm);
			}
			if (!(r & CDRF_SKIPDEFAULT))
			{
				if (nm.eState != eck::Dui::State::None) {
					if (nm.eState == eck::Dui::State::Hot) {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
						m_pDC->FillRoundedRectangle(D2D1::RoundedRect(nm.rc, 8, 8), m_pBrush);
					}
					if (nm.eState == eck::Dui::State::Selected) {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
						m_pDC->FillRoundedRectangle(D2D1::RoundedRect(nm.rc, 8, 8), m_pBrush);
					}
					if (nm.eState == eck::Dui::State::HotSelected) {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.2));
						m_pDC->FillRoundedRectangle(D2D1::RoundedRect(nm.rc, 8, 8), m_pBrush);
					}

				}
					
					//GetTheme()->DrawBackground(eck::Dui::Part::ListItem,
						//nm.eState, nm.rc, nullptr);
				GetSubItemRect(nm.idx, 0, nm.rc);
				LVPaintSubItem(rcPaint, nm, r);
#if _DEBUG
				if (m_bDbgIndex)
				{
					eck::CRefStrW rs{};
					rs.Format(L"%d", nm.idx);
					if (GetTextFormat())
						m_pDC->DrawTextW(rs.Data(), rs.Size(),
							GetTextFormat(), nm.rc, m_pBrush);
				}
#endif// _DEBUG
				if (m_eView == Type::Report)
					for (nm.idxSub = 1; nm.idxSub < m_Header.GetItemCount(); ++nm.idxSub)
					{
						GetSubItemRect(nm.idx, nm.idxSub, nm.rc);
						LVPaintSubItem(rcPaint, nm, r);
					}
			}
		}
	}

	void PostPaint(eck::Dui::ELEMPAINTSTRU& ps) override
	{
		D2D1_COLOR_F cr;
		if (m_pec2->IsActive())
		{
			D2D1_RECT_F rc;
			rc.left = CxIndicatorPadding;
			rc.right = CxIndicatorPadding + CxIndicator;
			if (m_idxTo > m_idxFrom)// 向下
			{
				const auto d = (m_idxTo - m_idxFrom) * (m_cyItem + m_cyPadding);
				rc.bottom = m_pec1->GetCurrValue() * d + (m_cyItem - CyIndicatorPadding);
				rc.top = m_pec2->GetCurrValue() * d + CyIndicatorPadding;
				eck::OffsetRect(rc, 0.f,
					m_idxFrom * (m_cyItem + m_cyPadding) - m_psvV->GetPos());
			}
			else// 向上
			{
				const auto d = (m_idxFrom - m_idxTo) * (m_cyItem + m_cyPadding);
				rc.top = -(m_pec1->GetCurrValue() * d - CyIndicatorPadding);
				rc.bottom = -(m_pec2->GetCurrValue() * d - (m_cyItem - CyIndicatorPadding));
				eck::OffsetRect(rc, 0.f,
					m_idxFrom * (m_cyItem + m_cyPadding) - m_psvV->GetPos());
			}
			GetTheme()->GetColorizationColor(cr);
			m_pBrush->SetColor(cr);
			float rcRadius = (rc.right - rc.left) / 2;
			m_pDC->FillRoundedRectangle(D2D1::RoundedRect(rc, rcRadius, rcRadius), m_pBrush);
		}
		else if (m_idxSel >= 0)
		{
			D2D1_RECT_F rc;
			GetItemRect(m_idxSel, rc);
			rc.left = CxIndicatorPadding;
			rc.right = CxIndicatorPadding + CxIndicator;
			rc.top += CyIndicatorPadding;
			rc.bottom -= CyIndicatorPadding;
			GetTheme()->GetColorizationColor(cr);
			m_pBrush->SetColor(cr);
			float rcRadius = (rc.right - rc.left) / 2;
			m_pDC->FillRoundedRectangle(D2D1::RoundedRect(rc, rcRadius, rcRadius), m_pBrush);
		}
	}

	static void EasingProc(float fCurrValue, float fOldValue, LPARAM lParam)
	{
		const auto p = (CVeTabList*)lParam;
		D2D1_RECT_F rc, rc2;
		p->GetItemRect(p->m_idxFrom, rc);
		p->GetItemRect(p->m_idxTo, rc2);
		eck::UnionRect(rc, rc, rc2);
		rc.left = CxIndicatorPadding;
		rc.right = CxIndicatorPadding + CxIndicator;
		if (p->m_rcLastRedraw.top < rc.top)
			rc.top = p->m_rcLastRedraw.top;
		if (p->m_rcLastRedraw.bottom > rc.bottom)
			rc.bottom = p->m_rcLastRedraw.bottom;
		p->m_rcLastRedraw = rc;
		p->InvalidateRect(rc);
	}
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		switch (uMsg)
		{
		case WM_CREATE:
			__super::OnEvent(uMsg, wParam, lParam);
			m_pec1 = new eck::CEasingCurve{};
			m_pec1->SetAnProc(eck::Easing::OutExpo);
			m_pec1->SetCallBack([](float fCurrValue, float fOldValue, LPARAM lParam)
				{
				});
			m_pec1->SetDuration(180);
			InitEasingCurve(m_pec1);

			m_pec2 = new eck::CEasingCurve{};
			m_pec2->SetAnProc(eck::Easing::OutExpo);
			m_pec2->SetCallBack(EasingProc);
			m_pec2->SetDuration(600);
			InitEasingCurve(m_pec2);

			SetView(Type::List);
			SetSingleSel(TRUE);
			SetItemNotify(TRUE);
			return 0;
		case WM_DESTROY:
			SafeRelease(m_pec1);
			SafeRelease(m_pec2);
			break;
		}
		return __super::OnEvent(uMsg, wParam, lParam);
	}

	LRESULT OnNotify(eck::Dui::DUINMHDR* pnm, BOOL& bProcessed) override
	{
		if (pnm->uCode == eck::Dui::EE_CLICK)
		{
			const auto p = (eck::Dui::NMLTITEMINDEX*)pnm;
			if (p->idx == m_idxLastSel || p->idx < 0 || m_idxLastSel < 0)
			{
				m_idxLastSel = p->idx;
				return 0;
			}
			m_idxFrom = m_idxLastSel;
			m_idxTo = p->idx;
			m_idxLastSel = p->idx;
			if (!m_pec1->IsActive() || !m_pec2->IsActive())
			{
				GetItemRect(m_idxFrom, m_rcLastRedraw);
				D2D1_RECT_F rc2;
				GetItemRect(m_idxTo, rc2);
				eck::UnionRect(m_rcLastRedraw, m_rcLastRedraw, rc2);
			}
			m_pec1->Begin(0.f, 1.f, FALSE);
			m_pec2->Begin(0.f, 1.f, FALSE);
			m_pec1->SetCurrTime(0.f);
			m_pec2->SetCurrTime(0.f);
			GetWnd()->WakeRenderThread();
		}
		else if (pnm->uCode == eck::Dui::LTE_ITEMCHANED)
		{
			bProcessed = TRUE;
			const auto* const p = (eck::Dui::NMLTITEMCHANGE*)pnm;
			if ((p->uFlagsOld & eck::Dui::LEIF_SELECTED) && !(p->uFlagsNew & eck::Dui::LEIF_SELECTED))
				return TRUE;
			else
			{
				NMTBLITEMINDEX nm{ eck::Dui::TBLE_SELCHANGED,p->idx };
				GenElemNotify(&nm);
			}
		}
		return 0;
	}
};
