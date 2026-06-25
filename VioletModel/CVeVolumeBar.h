#pragma once
#include "CVeDuiTrackBar.h"
class CVeVolumeBar : public Dui::CElem
{
private:
	Dui::CLabel m_LAVol{};
	CVeTrackBar m_TrackBar{};

	ID2D1SolidColorBrush* m_pBrush{};
	Dui::CCompositorPageAn* m_pPageAn{};
	eck::CEasingCurve* m_pecShowing{};

	BOOL m_bShow{};
public:
	float m_opacity = 1.f;
	
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	void ShowAnimation();

	void OnVolChanged(float fVol);
};