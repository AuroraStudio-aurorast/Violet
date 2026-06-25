#pragma once
class CVeImage : public Dui::CElem
{
private:
	ID2D1Bitmap1* m_pBmp{};

	float radiusX = 0.f;
	float radiusY = 0.f;

	float imageOpacity = 1.f;
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	void SetBitmap(ID2D1Bitmap1* pBmp)
	{
		ECK_DUILOCK;
		std::swap(m_pBmp, pBmp);
		if (m_pBmp)
			m_pBmp->AddRef();
		if (pBmp)
			pBmp->Release();
	}

	void SetBorderRadius(float radiusX_, float radiusY_) {
		radiusX = radiusX_;
		radiusY = radiusY_;
	}

	void SetOpacity(float opacity) {
		imageOpacity = opacity;
	}

};

