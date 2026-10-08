#pragma once
#include "CVeCover.h"
#include "CVeLrc.h"
#include "CVeDuiImage.h"
#include "CVeDuiButton.h"
#include "CVeVideoPlayer.h"
#include "CVeDuiSpacer.h"
#include "CApp.h"
// CWndMain负责更新该元素的图片
class CPagePlaying : public Dui::CElem
{
	friend class CWndMain;
private:
	CVeCover m_Cover{};
	CVeLrc m_Lrc{};
	CVeButton m_BTBack{};
	Dui::CLabel m_LATitle{};
	Dui::CLabel m_LAAlbum{};
	Dui::CLabel m_LAArtist{};

	CVeImage m_CoverImg{};

	CVeVideoPlayer m_VideoPlayer{};

	CVeSpacer m_ControlsSpacer{};
	CVeSpacer m_TitlebarSpacer{};

	ID2D1Bitmap1* m_pBmpCover{};
	ID2D1Bitmap1* m_pBmpBlurredCover{};
	ComPtr<ID2D1SolidColorBrush> m_pBrBkg{};

	ComPtr<IDWriteTextFormat> pTfTitle;
	ComPtr<IDWriteTextFormat> pTfSubtitle;

	D2D1_RECT_F rcView{};

	bool m_isMouseIn = false;

	float radiusX = 0.f;
	float radiusY = 0.f;

	void UpdateBlurredCover();

	void OnPlayEvent(const PLAY_EVT_PARAM& e);

	void SetEmptyText();

	void OnColorSchemeChanged();

	void SetControlsVisible(bool visible);
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	void SetLabelTextFormatTitle(IDWriteTextFormat* pTf)
	{
		m_LATitle.SetTextFormat(pTf);
	}

	void SetLabelTextFormat(IDWriteTextFormat* pTf)
	{
		m_LAAlbum.SetTextFormat(pTf);
		m_LAArtist.SetTextFormat(pTf);
	}

	void setRadiusXY(float radiusX_, float radiusY_) {
		radiusX = radiusX_;
		radiusY = radiusY_;
		m_CoverImg.SetBorderRadius(radiusX_, radiusY_);
		m_VideoPlayer.SetBorderRadius(radiusX_, radiusY_);
		m_TitlebarSpacer.setRoundedRectMask(rcView, radiusX_, radiusY_);
		m_ControlsSpacer.setRoundedRectMask(rcView, radiusX_, radiusY_);
		m_ControlsSpacer.setMaskFlip(true);
	}

	void setCoverImgOpacity(float Opacity) {
		m_CoverImg.SetOpacity(Opacity);
		if (App->GetPlayer().IsVideo()) {
			m_VideoPlayer.SetOpacity(1 - Opacity);
			m_TitlebarSpacer.setBgAlpha(0.4 * (1 - Opacity));
			m_ControlsSpacer.setBgAlpha(0.4 * (1 - Opacity));
		}
		if (Opacity == 0) {
			m_CoverImg.SetVisible(false);
		}
		else {
			m_CoverImg.SetVisible(true);
		}
	}

	void setMouseIn(bool mouseIn) {
		m_isMouseIn = mouseIn;
		SetControlsVisible(m_isMouseIn);
	}
};