#pragma once
#include "eck/DuiBase.h"

class CVeTitleBar :public eck::Dui::CElem
{
protected:
	eck::CDwmWndPartMgr m_DwmPartMgr{};
	ID2D1Bitmap1* m_pBmpDwmWndAtlas{};
	float m_cxClose{};
	float m_cxMax{};
	float m_cxMin{};
	float m_cyBtn{};
	eck::DwmWndPart m_idxHot{ eck::DwmWndPart::Invalid };
	eck::DwmWndPart m_idxPressed{ eck::DwmWndPart::Invalid };
	BOOLEAN m_bMaximized{};
	BYTE m_eInterMode{ (BYTE)D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR };

	eck::DwmWPartState GetPartState(eck::DwmWndPart idx) const
	{
		if (m_idxPressed == idx)
			return eck::DwmWPartState::Pressed;
		else if (m_idxHot == idx)
			return eck::DwmWPartState::Hot;
		return eck::DwmWPartState::Normal;
	}

	LRESULT OnWndMsg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, eck::SlotCtx&)
	{
		if (uMsg == WM_WINDOWPOSCHANGED)
			m_bMaximized = IsZoomed(hWnd);
		return 0;
	}

	static constexpr BOOL IsNeedRedraw(eck::DwmWndPart ePart)
	{
        return ePart != eck::DwmWndPart::Invalid && ePart != eck::DwmWndPart::Extra;
	}
	static constexpr BOOL IsNeedRedraw(eck::DwmWndPart ePartOle, eck::DwmWndPart ePartNew)
	{
		return (ePartOle != ePartNew) && (IsNeedRedraw(ePartOle) || IsNeedRedraw(ePartNew));
	}
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		switch (uMsg)
		{
		case WM_PAINT:
		{
			eck::Dui::ELEMPAINTSTRU eps;
			BeginPaint(eps, wParam, lParam);

			D2D1_RECT_F rcDst
			{
				GetWidthF() - m_cxClose,
				0,
				GetWidthF(),
				(float)m_cyBtn
			};
			D2D1_RECT_F rcTemp;

			RECT rc, rcBkg;
			eck::DWMW_GET_PART_EXTRA Extra;
			const auto bDarkMode = ShouldAppsUseDarkMode();
			const auto iUserDpi = GetWnd()->GetUserDpi();
			const auto dMargin = eck::DpiScaleF(1.f, 96, iUserDpi);
			const auto eInterMode = (D2D1_INTERPOLATION_MODE)m_eInterMode;

			m_pDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			if (m_DwmPartMgr.GetPartRect(rc, rcBkg, eck::DwmWndPart::Close,
				GetPartState(eck::DwmWndPart::Close), bDarkMode, TRUE, iUserDpi, &Extra))
			{
				rcDst.left += dMargin;
				eck::DrawImageFromGrid(m_pDC, m_pBmpDwmWndAtlas, rcDst,
					eck::MakeD2DRectF(rcBkg), eck::MarginsToD2DRectF(Extra.pBkg->mgSizing),
					(D2D1_INTERPOLATION_MODE)m_eInterMode);

				rcTemp = eck::MakeD2DRectF(rc);
				GetWnd()->Phy2Log(rcTemp);
				eck::CenterRect(rcTemp, rcDst);

				rcTemp.left = std::round(rcTemp.left);
				rcTemp.top = std::round(rcTemp.top);
				rcTemp.right = std::round(rcTemp.right);
				rcTemp.bottom = std::round(rcTemp.bottom);

				m_pDC->DrawBitmap(m_pBmpDwmWndAtlas, rcTemp, 1.f,
					D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, eck::MakeD2DRectF(rc));
				//rcDst.left -= dMargin;
			}

			rcDst.left -= m_cxMax;
			rcDst.right = rcDst.left + m_cxMax;

			if (m_DwmPartMgr.GetPartRect(rc, rcBkg,
				m_bMaximized ? eck::DwmWndPart::Restore : eck::DwmWndPart::Max,
				GetPartState(eck::DwmWndPart::Max), bDarkMode, TRUE, iUserDpi, &Extra))
			{
				rcDst.left += dMargin;
				eck::DrawImageFromGrid(m_pDC, m_pBmpDwmWndAtlas, rcDst,
					eck::MakeD2DRectF(rcBkg), eck::MarginsToD2DRectF(Extra.pBkg->mgSizing),
					(D2D1_INTERPOLATION_MODE)m_eInterMode);

				rcTemp = eck::MakeD2DRectF(rc);
				GetWnd()->Phy2Log(rcTemp);
				eck::CenterRect(rcTemp, rcDst);

				rcTemp.left = std::round(rcTemp.left);
				rcTemp.top = std::round(rcTemp.top);
				rcTemp.right = std::round(rcTemp.right);
				rcTemp.bottom = std::round(rcTemp.bottom);

				m_pDC->DrawBitmap(m_pBmpDwmWndAtlas, rcTemp, 1.f,
					D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, eck::MakeD2DRectF(rc));
				rcDst.left -= dMargin;
			}

			rcDst.left -= m_cxMin;
			rcDst.right = rcDst.left + m_cxMin;

			if (m_DwmPartMgr.GetPartRect(rc, rcBkg, eck::DwmWndPart::Min,
				GetPartState(eck::DwmWndPart::Min), bDarkMode, TRUE, iUserDpi, &Extra))
			{
				rcDst.left += (dMargin * 2);
				eck::DrawImageFromGrid(m_pDC, m_pBmpDwmWndAtlas, rcDst,
					eck::MakeD2DRectF(rcBkg), eck::MarginsToD2DRectF(Extra.pBkg->mgSizing),
					(D2D1_INTERPOLATION_MODE)m_eInterMode);

				rcTemp = eck::MakeD2DRectF(rc);
				GetWnd()->Phy2Log(rcTemp);
				eck::CenterRect(rcTemp, rcDst);

				rcTemp.left = std::round(rcTemp.left);
				rcTemp.top = std::round(rcTemp.top);
				rcTemp.right = std::round(rcTemp.right);
				rcTemp.bottom = std::round(rcTemp.bottom);

				m_pDC->DrawBitmap(m_pBmpDwmWndAtlas, rcTemp, 1.0f,
					D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, eck::MakeD2DRectF(rc));
			}
			m_pDC->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

			ECK_DUI_DBG_DRAW_FRAME;
			EndPaint(eps);
		}
		return 0;

		case WM_NCHITTEST:
		{
			POINT pt ECK_GET_PT_LPARAM(lParam);
			ClientToElem(pt);
			const eck::DwmWndPart ePart = HitTest(pt);
			switch (ePart)
			{
			case eck::DwmWndPart::Close:
				return HTCLOSE;
			case eck::DwmWndPart::Max:
				return HTMAXBUTTON;
			case eck::DwmWndPart::Min:
				return HTMINBUTTON;
			case eck::DwmWndPart::Extra:
				if (!m_bMaximized && pt.y < eck::DaGetSystemMetrics(SM_CYFRAME, 96))
					return HTTOP;
				else
					return HTCAPTION;
			}
		}
		return HTTRANSPARENT;

		case WM_NCLBUTTONDOWN:
		{
			POINT pt ECK_GET_PT_LPARAM(lParam);
			ScreenToClient(GetWnd()->HWnd, &pt);
			ClientToElem(pt);
			GetWnd()->Phy2Log(pt);
            const auto idxOld = m_idxPressed;
			m_idxPressed = HitTest(pt);
			if (IsNeedRedraw(idxOld, m_idxPressed))
				InvalidateBtnRect();
		}
		return 0;

		case WM_NCLBUTTONUP:
		{
			if (m_idxPressed != eck::DwmWndPart::Invalid)
			{
				POINT pt ECK_GET_PT_LPARAM(lParam);
				ScreenToClient(GetWnd()->HWnd, &pt);
				ClientToElem(pt);
				GetWnd()->Phy2Log(pt);
				const auto idx = HitTest(pt);
				if (m_idxPressed == idx)
					switch (idx)
					{
					case eck::DwmWndPart::Close:
						GetWnd()->PostMsg(WM_SYSCOMMAND, SC_CLOSE, 0);
						break;
					case eck::DwmWndPart::Max:
						if (m_bMaximized)
							GetWnd()->PostMsg(WM_SYSCOMMAND, SC_RESTORE, 0);
						else
							GetWnd()->PostMsg(WM_SYSCOMMAND, SC_MAXIMIZE, 0);
						break;
					case eck::DwmWndPart::Min:
						GetWnd()->PostMsg(WM_SYSCOMMAND, SC_MINIMIZE, 0);
						break;
					}
                const auto idxOld = m_idxPressed;
				m_idxPressed = eck::DwmWndPart::Invalid;
				if (IsNeedRedraw(idxOld, eck::DwmWndPart::Invalid))
                    InvalidateBtnRect();
			}
		}
		return 0;

		case WM_NCMOUSEMOVE:
		{
			POINT pt ECK_GET_PT_LPARAM(lParam);
			ScreenToClient(GetWnd()->HWnd, &pt);
			ClientToElem(pt);
			GetWnd()->Phy2Log(pt);
			const auto idxOld = m_idxHot;
			m_idxHot = HitTest(pt);
			if (IsNeedRedraw(m_idxHot, idxOld))
				InvalidateBtnRect();
		}
		return 0;

		case WM_MOUSELEAVE:
		{
			if (m_idxHot != eck::DwmWndPart::Invalid)
			{
                const auto idxOld = m_idxHot;
				m_idxHot = eck::DwmWndPart::Invalid;
				if (IsNeedRedraw(idxOld, eck::DwmWndPart::Invalid))
					InvalidateBtnRect();
			}
		}
		return 0;

		case WM_CAPTURECHANGED:
		{
			if (m_idxPressed != eck::DwmWndPart::Invalid)
			{
				const auto idxOld = m_idxPressed;
				m_idxPressed = eck::DwmWndPart::Invalid;
				if (IsNeedRedraw(idxOld, eck::DwmWndPart::Invalid))
					InvalidateBtnRect();
			}
		}
		return 0;

		case WM_CREATE:
		{
			GetWnd()->GetSignal().Connect(this, &CVeTitleBar::OnWndMsg, eck::MHI_DUI_TITLEBAR);
			m_bMaximized = IsZoomed(GetWnd()->HWnd);
			UpdateTitleBarInfo(TRUE);
			UpdateMetrics();
		}
		break;

		case WM_DESTROY:
			GetWnd()->GetSignal().Disconnect(eck::MHI_DUI_TITLEBAR);
			SafeRelease(m_pBmpDwmWndAtlas);
			break;
		}
		return CElem::OnEvent(uMsg, wParam, lParam);
	}

	void UpdateTitleBarInfo(BOOL bForceUpdate = FALSE)
	{
		if (bForceUpdate || !m_DwmPartMgr.GetHTheme())
		{
			m_DwmPartMgr.AnalyzeDefaultTheme();
			PCVOID pData;
			DWORD cbData;
			m_DwmPartMgr.GetData(&pData, &cbData);
			const auto pStream = new eck::CStreamView(pData, cbData);
			IWICBitmapDecoder* pDecoder;
			IWICBitmap* pBitmap{};
			eck::CreateWicBitmapDecoder(pStream, pDecoder);
			eck::CreateWicBitmap(pBitmap, pDecoder);
			m_pDC->CreateBitmapFromWicBitmap(pBitmap, &m_pBmpDwmWndAtlas);
			pDecoder->Release();
			pBitmap->Release();
			pStream->LeaveRelease();
		}
	}

	void UpdateMetrics()
	{
		if (eck::g_NtVer.uBuild >= eck::WINVER_11_21H2)
		{
			const auto iWndDpi = GetWnd()->GetDpiValue();
			//----计算高度
			m_cyBtn = (float)eck::DaGetSystemMetrics(SM_CYSIZE, iWndDpi);
			m_cyBtn += float(eck::DaGetSystemMetrics(SM_CXPADDEDBORDER, iWndDpi) +
				eck::DaGetSystemMetrics(SM_CYFRAME, iWndDpi) +
				eck::DaGetSystemMetrics(SM_CYBORDER, iWndDpi));
			m_cyBtn = m_cyBtn * 96.f / iWndDpi;
			// 高度为SM_CYSIZE + 通常模式下的客户区上边距
			//----计算宽度
			int nSys = eck::DaGetSystemMetrics(SM_CYSIZE, iWndDpi);
			nSys = (int)floorf(nSys * 0.95454544f + 0.5f);
			// 对于标准的4个按钮（关闭、最大化、最小化、帮助）
			// 在两边的使用2.2272727，中间的使用2.1818182
			// 若只有关闭按钮，使用1.6363636
			m_cxClose = m_cxMin = floorf(nSys * 2.2272727f + 0.5f)
				* 96.f / iWndDpi;
			m_cxMax = floorf(nSys * 2.1818182f + 0.5f)
				* 96.f / iWndDpi;
			return;
		}

		if (eck::g_NtVer.uMajor == 6 && (eck::g_NtVer.uMinor == 2 || eck::g_NtVer.uMinor == 3))
		{
			m_cxClose = 46;
			m_cxMax = m_cxMin = m_cxClose * 80 / 150;
			m_cyBtn = 21;
		}
		else
		{
			m_cxClose = 46;
			m_cxMax = m_cxMin = m_cxClose;
			m_cyBtn = 31;
		}
	}

	eck::DwmWndPart HitTest(POINT ptClient) const
	{
		const auto cx = GetWidthF();
		const auto cy = GetHeightF();
		if (ptClient.x < 0 || ptClient.x > cx || ptClient.y < 0 || ptClient.y > cy ||
			ptClient.y > m_cyBtn)
			return eck::DwmWndPart::Invalid;
		if (ptClient.x > cx - m_cxClose)
			return eck::DwmWndPart::Close;
		else if (ptClient.x > cx - m_cxMax - m_cxClose)
			return eck::DwmWndPart::Max;
		else if (ptClient.x > cx - m_cxMin * 2 - m_cxClose)
			return eck::DwmWndPart::Min;
		else
			return eck::DwmWndPart::Extra;
	}

	void InvalidateBtnRect()
	{
		const auto cxBtn = m_cxClose + m_cxMax + m_cxMin;
		D2D1_RECT_F rc{ GetWidthF() - cxBtn,0,GetWidthF(),m_cyBtn };
		InvalidateRect(rc);
	}

	EckInlineCe void SetInterpolationMode(D2D1_INTERPOLATION_MODE eInterMode)
	{
		m_eInterMode = eInterMode;
	}
	EckInlineNdCe D2D1_INTERPOLATION_MODE GetInterpolationMode() const
	{
		return (D2D1_INTERPOLATION_MODE)m_eInterMode;
	}
};
