#pragma once
class CCompPlayPageAn final : public Dui::CCompositorCornerMapping
{
private:
	ComPtr<ID2D1Bitmap1> pBitmapOverlay{};
	float CornerRadius = 0.f;
public:
	void PostRender(Dui::COMP_RENDER_INFO& cri) override
	{
		//RoundedRectMaskD2dDC(cri.pDC, eck::g_pD2dFactory, cri.rcDst, 100);
		cri.pDC->DrawBitmap(cri.pBitmap, cri.rcDst, 1 - Opacity, D2D1_INTERPOLATION_MODE_LINEAR, cri.rcSrc, (D2D1_MATRIX_4X4_F*)&Mat);
		if (pBitmapOverlay.Get())
			cri.pDC->DrawBitmap(pBitmapOverlay.Get(), cri.rcDst, Opacity, D2D1_INTERPOLATION_MODE_LINEAR, nullptr, (D2D1_MATRIX_4X4_F*)&Mat);


		//cri.pDC->PopLayer();
	}

	void SetOverlayBitmap(ID2D1Bitmap1* pBmp)
	{
		pBitmapOverlay = pBmp;
	}

	void SetCornerRadius(float cornerRadius) {
		CornerRadius = cornerRadius;
	}
};