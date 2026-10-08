#pragma once
class CVeSpacer : public Dui::CElem
{
private:
	ID2D1SolidColorBrush* m_pBrush{};
	D2D1_RECT_F roundedRectMask;
	float bgAlpha = 0.4;
	float m_roundedRectMaskBorderRadiusX = 0.f;
	float m_roundedRectMaskBorderRadiusY = 0.f;
	bool m_isMaskFlip = false;
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	void setRoundedRectMask(D2D1_RECT_F rect, float borderRadiusX, float borderRadiusY) {
		roundedRectMask = rect;
		m_roundedRectMaskBorderRadiusX = borderRadiusX;
		m_roundedRectMaskBorderRadiusY = borderRadiusY;
	}
	void setMaskFlip(bool flip) {
		m_isMaskFlip = flip;
	}
	void setBgAlpha(float alpha) {
		bgAlpha = alpha;
	}
};

