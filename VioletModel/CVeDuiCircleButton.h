#pragma once
#include "eck\DuiBase.h"
#include "CApp.h"
#include "Utils.h"


struct NMCBTCUSTOMDRAW : eck::Dui::NMECUSTOMDRAW
{
	eck::Dui::State eState;
	D2D1_RECT_F rcImg;
	ID2D1Bitmap* pImg;
};

class CVeCircleButton :public eck::Dui::CElem
{
private:
	ID2D1Bitmap* m_pImg{};
	ID2D1SolidColorBrush* m_pBrush{};

	D2D1_SIZE_F m_sizeImg{};
	BYTE m_eInterpolation{ D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC };

	BITBOOL m_bHot : 1{ FALSE };
	BITBOOL m_bLBtnDown : 1{ FALSE };

	BITBOOL m_bAutoImgSize : 1{ TRUE };
	BITBOOL m_bCustomDraw : 1{ FALSE };
	BITBOOL m_bTransparentBk : 1{ FALSE };

	eck::CEasingCurve* m_EasingBtn{};
	bool m_bAnimation = true;
	float bgAlpha = 0.f;


	BOOL PtInBtn(POINT ptInClient)
	{
		const auto fRad = std::min(GetWidthF(), GetHeightF()) / 2.f;
		const D2D1_POINT_2F ptCenter
		{
			GetOffsetInClientF().x + fRad,
			GetOffsetInClientF().y + fRad
		};
		return eck::PtInCircle(eck::MakeD2DPointF( ptInClient), ptCenter, fRad);
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
						const auto p = (CVeCircleButton*)lParam;
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



	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		switch (uMsg)
		{
		case WM_PAINT:
		{
			eck::Dui::ELEMPAINTSTRU ps;
			BeginPaint(ps, wParam, lParam);

			eck::Dui::State eState;
			if (m_bLBtnDown)
				eState = eck::Dui::State::Selected;
			else if (m_bHot)
				eState = eck::Dui::State::Hot;
			else
				eState = eck::Dui::State::Normal;

			BOOL bSkipDefault{};
			NMCBTCUSTOMDRAW cd;
			if (m_pImg)
			{
				cd.rcImg.left = (GetWidthF() - m_sizeImg.width) / 2.f;
				cd.rcImg.top = (GetHeightF() - m_sizeImg.height) / 2.f;
				cd.rcImg.right = cd.rcImg.left + m_sizeImg.width;
				cd.rcImg.bottom = cd.rcImg.top + m_sizeImg.height;
			}

			if (m_bCustomDraw)
			{
				cd.uCode = eck::Dui::EE_CUSTOMDRAW;
				cd.dwStage = CDDS_PREPAINT;
				cd.eState = eState;
				cd.pImg = m_pImg;

				bSkipDefault = (GenElemNotify(&cd) & CDRF_SKIPDEFAULT);
			}

			if (!bSkipDefault)
			{
				D2D1_ELLIPSE Ell;
				D2D1_RECT_F rc = GetViewRectF();
				Ell.point = D2D1::Point2F(rc.left + rc.right / 2, rc.top + rc.bottom / 2);
				Ell.radiusX = rc.right / 2;
				Ell.radiusY = rc.bottom / 2;
				if (!m_bTransparentBk) {
					m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
					m_pDC->FillEllipse(Ell, m_pBrush);
				}

				switch (eState)
				{
				case Dui::State::Normal:
					m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.f));
					break;
				case Dui::State::Hot:
					if (m_bAnimation) {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, bgAlpha));
					}
					else {
						m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.1));
					}
					break;
				case Dui::State::Selected:
				    m_pBrush->SetColor(D2D1::ColorF(foregroundColor, 0.2));
					break;
				}
				m_pDC->FillEllipse(Ell, m_pBrush);
                if(eState == Dui::State::Normal && m_bAnimation) {
					m_pBrush->SetColor(D2D1::ColorF(foregroundColor, bgAlpha));
					m_pDC->FillEllipse(Ell, m_pBrush);
				}
					/*GetTheme()->DrawBackground(eck::Dui::Part::CircleButton, eState,
					GetViewRectF(), nullptr);*/


				


				if (m_pImg)
					m_pDC->DrawBitmap(m_pImg, cd.rcImg, 1.f,
						(D2D1_INTERPOLATION_MODE)m_eInterpolation);
			}

			EndPaint(ps);
		}
		return 0;

		case WM_NCHITTEST:
			return (PtInBtn(ECK_GET_PT_LPARAM(lParam)) ? HTCLIENT : HTTRANSPARENT);

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
                POINT pt ECK_GET_PT_LPARAM(lParam);
                ElemToClient(pt);
				if (PtInBtn(pt))
				{
					eck::Dui::DUINMHDR nm{ eck::Dui::EE_COMMAND };
					GenElemNotify(&nm);
				}
			}
		}
		return 0;

		case WM_SIZE:
		{
			if (m_bAutoImgSize)
			{
				m_sizeImg.width = std::min(GetWidthF(), GetHeightF());
				m_sizeImg.width /= 1.414f;
				m_sizeImg.height = m_sizeImg.width;
			}
		}
		return 0;

		case WM_DESTROY:
		{
			SafeRelease(m_pImg);
			SafeRelease(m_pBrush);
			m_bHot = FALSE;
			m_bLBtnDown = FALSE;
		}
		return 0;

		case WM_CREATE:
		{
			m_pDC->CreateSolidColorBrush({}, & m_pBrush);
		}
		return 0;
		}
		return CElem::OnEvent(uMsg, wParam, lParam);
	}

	void SetImage(ID2D1Bitmap* pImg)
	{
		std::swap(m_pImg, pImg);
		if (m_pImg)
			m_pImg->AddRef();
		if (pImg)
			pImg->Release();
	}
	EckInlineNdCe ID2D1Bitmap* GetImage() const { return m_pImg; }

	EckInlineCe void SetImageSize(D2D1_SIZE_F s) { m_sizeImg = s; }
	EckInlineNdCe D2D1_SIZE_F GetImageSize() const { return m_sizeImg; }

	EckInlineCe void SetInterpolationMode(D2D1_INTERPOLATION_MODE e) { m_eInterpolation = (BYTE)e; }
	EckInlineNdCe D2D1_INTERPOLATION_MODE GetInterpolationMode() const { return (D2D1_INTERPOLATION_MODE)m_eInterpolation; }

	EckInlineCe void SetAutoImageSize(BOOL b) { m_bAutoImgSize = b; }
	EckInlineNdCe BOOL GetAutoImageSize() const { return m_bAutoImgSize; }

	EckInlineCe void SetCustomDraw(BOOL b) { m_bCustomDraw = b; }
	EckInlineNdCe BOOL GetCustomDraw() const { return m_bCustomDraw; }

	EckInlineCe void SetTransparentBk(BOOL b) { m_bTransparentBk = b; }
	EckInlineNdCe BOOL GetTransparentBk() const { return m_bTransparentBk; }
};
