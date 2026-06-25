#pragma once
#include "CVeDuiListTemplate.h"
#include "Utils.h"


struct NMLEDISPINFO : eck::Dui::DUINMHDR
{
	eck::DispInfoMask uMask;
	BOOL bItem;
	union
	{
		struct ITEM
		{
			int idx;
			int idxSub;
			int idxGroup;
			PCWSTR pszText;
			int cchText;
			int idxImg;
			ID2D1Bitmap* pImg;
		} Item;
		struct GROUP
		{
			int idx;
			PCWSTR pszText;
			int cchText;
			int idxImg;
			ID2D1Bitmap* pImg;
		} Group;
	};
};


class CVeList : public CVeListTemplate
{
private:

	ComPtr<ID2D1LinearGradientBrush> m_pBrFade{};
	bool m_bTopFade = true;
	float m_bTopFadeHeight = 30;

	D2D1_SIZE_F GetImageSize(const NMLEDISPINFO& es)
	{
		if (es.Item.pImg)
			return es.Item.pImg->GetSize();
		else if (m_pImgList && es.Item.idxImg >= 0)
			return m_pImgList->GetImageSize();
		return {};
	}

	void GRPaintGroup(const D2D1_RECT_F& rcPaint, NMLTCUSTOMDRAW& nm, LRESULT r) override
	{
		const auto Padding = GetTheme()->GetMetrics(Dui::Metrics::Padding);
		auto& e = m_Group[nm.idxGroup];

		D2D1_RECT_F rcText, rcGroupImg;
		GetGroupPartRect(ListPart::GroupHeader, -1, nm.idxGroup, rcText);
		GetGroupPartRect(ListPart::GroupImg, -1, nm.idxGroup, rcGroupImg);

		const BOOL bText = !(rcText.bottom <= rcPaint.top ||
			rcText.top >= rcPaint.bottom);
		const BOOL bGroupImg = eck::IsRectsIntersect(rcGroupImg, rcPaint);
		if (!bText && !bGroupImg)
			return;

		NMLEDISPINFO sldi{};
		if (bText)
			sldi.uMask |= eck::DIM_TEXT;
		if (bGroupImg)
			sldi.uMask |= eck::DIM_IMAGE;
		sldi.uCode = eck::Dui::LEE_GETDISPINFO;
		sldi.bItem = FALSE;
		sldi.Group.cchText = -1;
		sldi.Group.idx = nm.idxGroup;
		GenElemNotify(&sldi);

		if (bText)
		{
			if (!e.pLayout.Get())
			{
				eck::g_pDwFactory->CreateTextLayout(sldi.Group.pszText,
					sldi.Group.cchText, m_pTfGroup, GetWidthF() - Padding * 2.f,
					(float)m_cyGroupHeader, &e.pLayout);
			}
			if (e.pLayout.Get())
			{
				const auto Padding2 = GetTheme()->GetMetrics(Dui::Metrics::LargePadding);
				if (nm.bColorText)
					m_pBrush->SetColor(nm.crText);
				else
				{
					D2D1_COLOR_F cr;
					GetTheme()->GetSysColor(Dui::SysColor::MainTitle, cr);
					m_pBrush->SetColor(cr);
				}
				m_pDC->DrawTextLayout({ rcText.left + Padding,rcText.top }, e.pLayout.Get(),
					m_pBrush, eck::Dui::DrawTextLayoutFlags);
				DWRITE_TEXT_METRICS tm;
				e.pLayout->GetMetrics(&tm);
				const float yLine = rcText.top + (float)(m_cyGroupHeader / 2);
				D2D1_POINT_2F pt1{ rcText.left + tm.width + Padding2 * 2.f ,yLine };
				D2D1_POINT_2F pt2{ GetWidthF() - Padding2,yLine };
				if (pt1.x < pt2.x)
					m_pDC->DrawLine(pt1, pt2, m_pBrush, (float)CyGroupLine);
			}
		}

		if (bGroupImg && sldi.Group.pImg)
			m_pDC->DrawBitmap(sldi.Group.pImg, &rcGroupImg);
	}

	void LVPaintSubItem(const D2D1_RECT_F& rcPaint, NMLTCUSTOMDRAW& nm, LRESULT r) override
	{
		NMLEDISPINFO es{ eck::Dui::LEE_GETDISPINFO };
		es.bItem = TRUE;
		es.Item.idx = nm.idx;
		es.Item.idxSub = nm.idxSub;
		es.Item.idxGroup = nm.idxGroup;
		es.uMask = eck::DIM_TEXT | eck::DIM_IMAGE;
		GenElemNotify(&es);

		const auto sizeImg = GetImageSize(es);
		const auto Padding = GetTheme()->GetMetrics(Dui::Metrics::SmallPadding);
		const auto Padding2 = GetTheme()->GetMetrics(Dui::Metrics::Padding);
		auto& e = nm.idxGroup < 0 ? m_vItem[nm.idx] : m_Group[nm.idxGroup].Item[nm.idx];
		auto& pTl = (nm.idxSub ? e.vSubItem[nm.idxSub - 1].pLayout : e.pLayout);
		if (!pTl.Get() && es.Item.pszText && es.Item.cchText > 0)
		{
			eck::g_pDwFactory->CreateTextLayout(es.Item.pszText, es.Item.cchText,
				GetTextFormat(),
				nm.rc.right - nm.rc.left - Padding * 3 - sizeImg.width,
				(float)m_cyItem, &pTl);
		}

		const float xImage = (float)((m_cyItem - sizeImg.height) / 2);
		const float yImage = (float)((m_cyItem - sizeImg.height) / 2);
		float xText;
		if (es.Item.pImg || (m_pImgList && es.Item.idxImg >= 0))
		{
			auto rc{ nm.rc };
			rc.left += xImage;
			rc.right = rc.left + sizeImg.width;
			rc.top += yImage;
			rc.bottom = rc.top + sizeImg.height;
			xText = rc.right + Padding2 * 2;
			/*
			eck::g_pD2dFactory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(rc, 6, 6), &rrect_);
			layer_param = D2D1::LayerParameters(D2D1::InfiniteRect(), (ID2D1Geometry*)rrect_.Get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), (1.0F), (ID2D1Brush*)0, D2D1_LAYER_OPTIONS_NONE);
			m_pDC->PushLayer(&layer_param, NULL);
			*/
			RoundedRectMaskD2dDC(m_pDC, eck::g_pD2DFactory, rc, 6);

			if (!(rc.right <= rcPaint.left || rc.left >= rcPaint.right))
				if (es.Item.pImg)
					m_pDC->DrawBitmap(es.Item.pImg, rc);
				else/* if (m_pImgList && es.idxImg >= 0)*/
					m_pImgList->Draw(es.Item.idxImg, rc);

			m_pDC->PopLayer();
		}
		else
			xText = nm.rc.left + Padding;
		if (pTl.Get())
		{
			DWRITE_TEXT_METRICS tm;
			pTl->GetMetrics(&tm);
			if (!(xText + tm.width <= rcPaint.left || xText >= rcPaint.right))
			{
				if (nm.bColorText)
					m_pBrush->SetColor(nm.crText);
				else
				{
					D2D1_COLOR_F cr;
					GetTheme()->GetSysColor(eck::Dui::SysColor::Text, cr);
					m_pBrush->SetColor(cr);
				}
				m_pDC->DrawTextLayout({ xText, nm.rc.top }, pTl.Get(), m_pBrush,
					eck::Dui::DrawTextLayoutFlags);
			}
		}
	}

	void IVPaintItem(const D2D1_RECT_F& rcPaint, NMLTCUSTOMDRAW& nm, LRESULT r) override
	{
		NMLEDISPINFO es{ eck::Dui::LEE_GETDISPINFO };
		es.bItem = TRUE;
		es.Item.idx = nm.idx;
		es.uMask = eck::DIM_TEXT | eck::DIM_IMAGE;
		GenElemNotify(&es);

		D2D1_SIZE_F sizeImg = GetImageSize(es);
		auto& e = m_vItem[nm.idx];

		D2D1_RECT_F rc;
		GetItemRect(nm.idx, rc);

		eck::Dui::State eState;
		if ((e.uFlags & eck::Dui::LEIF_SELECTED) || (m_bSingleSel && m_idxSel == nm.idx))
			if (m_idxHot == nm.idx)
				eState = eck::Dui::State::HotSelected;
			else
				eState = eck::Dui::State::Selected;
		else if (m_idxHot == nm.idx)
			eState = eck::Dui::State::Hot;
		else
			eState = eck::Dui::State::None;

		if (eState != eck::Dui::State::None) {
			if (eState == eck::Dui::State::Hot) {
				m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
				m_pDC->FillRoundedRectangle(D2D1::RoundedRect(rc, 8, 8), m_pBrush);
			}
			if (eState == eck::Dui::State::Selected) {
				m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
				m_pDC->FillRoundedRectangle(D2D1::RoundedRect(rc, 8, 8), m_pBrush);
			}
			if (eState == eck::Dui::State::HotSelected) {
				m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.2));
				m_pDC->FillRoundedRectangle(D2D1::RoundedRect(rc, 8, 8), m_pBrush);
			}
		}
			//GetTheme()->DrawBackground(eck::Dui::Part::ListItem, eState, rc, nullptr);

		const float Padding = GetTheme()->GetMetrics(Dui::Metrics::SmallPadding);
		D2D1_RECT_F rcImg;
		rcImg.left = rc.left + (m_cxItem - sizeImg.width) / 2.f;
		rcImg.top = rc.top + Padding;
		rcImg.right = rcImg.left + sizeImg.width;
		rcImg.bottom = rcImg.top + sizeImg.height;

		if (!(rcImg.right <= rcPaint.left || rcImg.left >= rcPaint.right))
			if (es.Item.pImg)
				m_pDC->DrawBitmap(es.Item.pImg, rcImg, 1.f, D2D1_INTERPOLATION_MODE_LINEAR);
			else if (m_pImgList && es.Item.idxImg >= 0)
				m_pImgList->Draw(es.Item.idxImg, rcImg);

		if (!e.pLayout.Get() && es.Item.pszText)
		{
			EckAssert(es.Item.cchText > 0);
			eck::g_pDwFactory->CreateTextLayout(es.Item.pszText, es.Item.cchText, GetTextFormat(),
				(float)m_cxItem, float(rc.bottom - rcImg.bottom), &e.pLayout);

			if (e.pLayout.Get())
			{
				e.pLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
				e.pLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
				e.pLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
			}
		}

		if (e.pLayout.Get())
		{
			if (nm.bColorText)
				m_pBrush->SetColor(nm.crText);
			else
			{
				D2D1_COLOR_F cr;
				GetTheme()->GetSysColor(eck::Dui::SysColor::Text, cr);
				m_pBrush->SetColor(cr);
			}
			m_pDC->DrawTextLayout({ rc.left, rcImg.bottom + Padding },
				e.pLayout.Get(), m_pBrush, eck::Dui::DrawTextLayoutFlags);
		}
	}

	void ReCreateFadeBrush() {
		m_pBrFade.Clear();
		if (!m_bTopFade)
			return;

		const D2D1_GRADIENT_STOP Stop[]
		{
			{ 0.f, {.a = 0.f } },
			{ 1.f, {.a = 1.f } },
		};

		ComPtr<ID2D1GradientStopCollection> pStopCollection;
		m_pDC->CreateGradientStopCollection(EckArrAndLen(Stop), &pStopCollection);
		D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES Prop;
		Prop.startPoint = {};
		Prop.endPoint = { 0.f, m_bTopFadeHeight };
		m_pDC->CreateLinearGradientBrush(Prop, pStopCollection.Get(), &m_pBrFade);


	}

	void PrePaint(eck::Dui::ELEMPAINTSTRU& ps) override {
		if (m_bTopFade) {
			ReCreateFadeBrush();
			D2D1_LAYER_PARAMETERS1 LyParam{ D2D1::LayerParameters1() };
			LyParam.contentBounds = { 0.f,0.f,GetWidthF(),GetHeightF() };
			LyParam.opacityBrush = m_pBrFade.Get();
			//m_pDC->PushLayer(LyParam, nullptr);
		}
	}
	void PostPaint(eck::Dui::ELEMPAINTSTRU& ps) override {
		if (m_bTopFade) {

			const D2D1_RECT_F rcTop{ 0.f,0.f,GetWidthF(),m_bTopFadeHeight };
			D2D1_RECT_F rcTopSample;
			if (eck::IntersectRect(rcTopSample, rcTop, ps.rcfClipInElem))
			{
				D2D1_MATRIX_3X2_F Mat;
				m_pDC->GetTransform(&Mat);
				auto rcSampleInBmp{ rcTopSample };
				eck::OffsetRect(rcSampleInBmp, Mat.dx, Mat.dy);
				OpacityMaskD2dDC(m_pDC, eck::g_pD2DFactory, rcSampleInBmp,
					{ rcTopSample.left,rcTopSample.top }, m_pBrFade.Get(), 0);
			}

		}	//m_pDC->PopLayer();

	}

public:
	void SetTopFade(bool bTopFade) {
		m_bTopFade = bTopFade;
	}

	void SetTopFadeHeight(float bTopFadeHeight) {
		m_bTopFadeHeight = bTopFadeHeight;
	}


};
