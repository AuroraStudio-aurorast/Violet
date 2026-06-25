#include "pch.h"
#include "Utils.h"
#include "CApp.h"
#include "wrl/client.h"

D2D1_LAYER_PARAMETERS layer_param;
ComPtr<ID2D1RoundedRectangleGeometry> rrect_{};


Tag::Result VltGetMusicInfo(PCWSTR pszFile,
	Tag::MUSICINFO& mi, const Tag::SIMPLE_OPT& Opt)
{
	Tag::CMediaFile mf{ pszFile };
	if (!mf.IsValid())
		return Tag::Result::FileAccessDenied;
	mi.Clear();
	const auto uMask = mi.uMask;
	if (mf.GetTagType() & Tag::TAG_FLAC)
	{
		Tag::CFlac x{ mf };
		x.ReadTag();
		x.SimpleGet(mi, Opt);
		mi.uMask &= ~mi.uMaskChecked;
	}
	if (mf.GetTagType() & (Tag::TAG_ID3V2_3 | Tag::TAG_ID3V2_4))
	{
		Tag::CID3v2 x{ mf };
		x.ReadTag();
		x.SimpleGet(mi, Opt);
		mi.uMask &= ~mi.uMaskChecked;
	}
	if (mf.GetTagType() & Tag::TAG_APE)
	{
		Tag::CApe x{ mf };
		x.ReadTag();
		x.SimpleGet(mi, Opt);
		mi.uMask &= ~mi.uMaskChecked;
	}
	if ((uMask & Tag::MIM_TITLE) && mi.rsTitle.IsEmpty())
		mi.rsTitle.DupString(EckStrAndLen(L"未知标题"));
	if ((uMask & Tag::MIM_ARTIST) && mi.slArtist.Str.IsEmpty())
		mi.slArtist.PushBackString(L"未知艺术家"sv, {});
	if ((uMask & Tag::MIM_ALBUM) && mi.rsAlbum.IsEmpty())
		mi.rsAlbum.DupString(EckStrAndLen(L"未知专辑"));
	mi.uMask = uMask;
	return Tag::Result::Ok;
}

HRESULT RoundedRectMaskD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, float borderRadius) {
	D2D1_LAYER_PARAMETERS layer_param;
	ID2D1RoundedRectangleGeometry* rrect_{};

	pFactory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(rc, borderRadius, borderRadius), &rrect_);
	layer_param = D2D1::LayerParameters(D2D1::InfiniteRect(), (ID2D1Geometry*)rrect_, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), (1.0F), (ID2D1Brush*)0, D2D1_LAYER_OPTIONS_NONE);
	pDC->PushLayer(&layer_param, NULL);
	rrect_->Release();

	//pDC->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),(ID2D1Geometry*)0, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), (1.0F), (ID2D1Brush*)0, D2D1_LAYER_OPTIONS_NONE), NULL);
	return S_OK;
}

HRESULT RoundedRectMaskXYD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, float radiusX, float radiusY) {
	D2D1_LAYER_PARAMETERS layer_param;
	ID2D1RoundedRectangleGeometry* rrect_{};

	pFactory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(rc, radiusX, radiusY), &rrect_);
	layer_param = D2D1::LayerParameters(D2D1::InfiniteRect(), (ID2D1Geometry*)rrect_, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), (1.0F), (ID2D1Brush*)0, D2D1_LAYER_OPTIONS_NONE);
	pDC->PushLayer(&layer_param, NULL);
	rrect_->Release();

	//pDC->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),(ID2D1Geometry*)0, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), (1.0F), (ID2D1Brush*)0, D2D1_LAYER_OPTIONS_NONE), NULL);
	return S_OK;
}


HRESULT BlurD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, D2D1_POINT_2F point, float fDeviation, double borderRadius)
{
	ComPtr<ID2D1Bitmap1> pBmp;
	ComPtr<ID2D1Image> pTarget;
	pDC->GetTarget(&pTarget);
	pTarget->QueryInterface(&pBmp);
	if (pBmp.Get() != nullptr) {
		HRESULT hr;
		ComPtr<ID2D1Effect> pEffect;
		ComPtr<ID2D1Effect> saturationEffect;
		float dpiX{};
		float dpiY{};
		double Zoom;
		pDC->GetDpi(&dpiX, &dpiY);
		Zoom = dpiX / 96;

		pDC->Flush();
		//pDC->SetDpi(96, 96);
		hr = pDC->CreateEffect(CLSID_D2D1GaussianBlur, &pEffect);
		if (FAILED(hr))
			return hr;
		pEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
		pEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, fDeviation);

		hr = pDC->CreateEffect(CLSID_D2D1Saturation, &saturationEffect);
		if (FAILED(hr))
			return hr;
		saturationEffect->SetValue(D2D1_SATURATION_PROP_SATURATION, 5.f);


		ComPtr<ID2D1Bitmap> pBmpEffect;
		hr = pDC->CreateBitmap({ (UINT32)((rc.right - rc.left) * Zoom), (UINT32)((rc.bottom - rc.top)*Zoom) },
			NULL, 0, D2D1::BitmapProperties(pBmp->GetPixelFormat(), dpiX, dpiY), &pBmpEffect);
		if (FAILED(hr))
			return hr;
		const D2D1_RECT_U rcU{ (UINT32)(rc.left*Zoom), (UINT32)(rc.top*Zoom), (UINT32)(rc.right*Zoom), (UINT32)(rc.bottom*Zoom) };
		pBmpEffect->CopyFromBitmap(NULL, pBmp.Get(), &rcU);

		pEffect->SetInput(0, pBmpEffect.Get());

		saturationEffect->SetInputEffect(0, pEffect.Get());

		const auto iBlend = pDC->GetPrimitiveBlend();
		pDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		/*
		pFactory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(point.x * Zoom, point.y * Zoom, (rc.right - rc.left), (rc.bottom - rc.top)), borderRadius * Zoom, borderRadius * Zoom), &rrect_);
		layer_param = D2D1::LayerParameters(D2D1::InfiniteRect(), (ID2D1Geometry*)rrect_.Get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), (1.0F), (ID2D1Brush*)0, D2D1_LAYER_OPTIONS_NONE);
		pDC->PushLayer(&layer_param, NULL);
		*/
		RoundedRectMaskD2dDC(pDC, pFactory, D2D1::RectF(point.x * Zoom, point.y * Zoom, (rc.right - rc.left), (rc.bottom - rc.top)), borderRadius * Zoom);
		pDC->Clear(D2D1::ColorF(0x000000, 0));
		//pDC->SetDpi(96, 96);

		pDC->DrawImage(saturationEffect.Get(), D2D1::Point2(point.x * Zoom,point.y * Zoom));
		//pDC->SetTransform(D2D1::Matrix3x2F::Scale(1, 1));
		//pDC->SetDpi(96 * Zoom, 96 * Zoom);
		pDC->PopLayer();
		pDC->SetPrimitiveBlend(iBlend);
		//pDC->SetDpi(dpiX, dpiY);
		//pBmpEffect->Release();
		//pEffect->Release();
		return S_OK;
	}
	else {
		return S_FALSE;
	}
}

HRESULT OpacityMaskD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, D2D1_POINT_2F point, ID2D1Brush* opacityBrush, double borderRadius)
{
	ComPtr<ID2D1Bitmap1> pBmp;
	ComPtr<ID2D1Image> pTarget;
	pDC->GetTarget(&pTarget);
	pTarget->QueryInterface(&pBmp);
	if (pBmp.Get() != nullptr) {
		HRESULT hr;
		ComPtr<ID2D1Effect> pEffect;
		float dpiX{};
		float dpiY{};
		double Zoom;
		pDC->GetDpi(&dpiX, &dpiY);
		Zoom = dpiX / 96;

		pDC->Flush();
		//pDC->SetDpi(96, 96);


		ComPtr<ID2D1Bitmap> pBmpEffect;
		hr = pDC->CreateBitmap({ (UINT32)((rc.right - rc.left) * Zoom), (UINT32)((rc.bottom - rc.top) * Zoom) },
			NULL, 0, D2D1::BitmapProperties(pBmp->GetPixelFormat(), dpiX, dpiY), &pBmpEffect);
		if (FAILED(hr))
			return hr;
		const D2D1_RECT_U rcU{ (UINT32)(rc.left * Zoom), (UINT32)(rc.top * Zoom), (UINT32)(rc.right * Zoom), (UINT32)(rc.bottom * Zoom) };
		pBmpEffect->CopyFromBitmap(NULL, pBmp.Get(), &rcU);

		const auto iBlend = pDC->GetPrimitiveBlend();
		pDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		
		D2D1_LAYER_PARAMETERS1 LyParam{ D2D1::LayerParameters1() };
		//LyParam.contentBounds = { 0.f,0.f,rc.right - rc.left,rc.bottom - rc.top };
		LyParam.contentBounds = { point.x,point.y,point.x + rc.right - rc.left,point.y + rc.bottom - rc.top };
		LyParam.opacityBrush = opacityBrush;

		pDC->PushLayer(&LyParam, NULL);
		pDC->Clear(D2D1::ColorF(0x000000, 0));
		//pDC->SetDpi(96, 96);

		//pDC->DrawImage(pBmpEffect.Get(), D2D1::Point2(point.x * Zoom, point.y * Zoom));
		pDC->DrawImage(pBmpEffect.Get(), point);
		//pDC->SetTransform(D2D1::Matrix3x2F::Scale(1, 1));
		//pDC->SetDpi(96 * Zoom, 96 * Zoom);
		pDC->PopLayer();
		pDC->SetPrimitiveBlend(iBlend);
		//pDC->SetDpi(dpiX, dpiY);
		//pBmpEffect->Release();
		//pEffect->Release();
		return S_OK;
	}
	else {
		return S_FALSE;
	}
}

void OYM_CvsCreateText(ID2D1DeviceContext* pContext, LPCWSTR lpText, LPCWSTR lpFont, INT nSize, BOOL bBold, D2D1_RECT_F rect, DWRITE_PARAGRAPH_ALIGNMENT parAlign, DWRITE_TEXT_ALIGNMENT textAlign, BOOL bWrap, IDWriteTextFormat** pTextFormat, IDWriteTextLayout** pTextLayout)
{
	FLOAT cvsWidth = pContext->GetSize().width;
	D2D1_RECT_F prect = rect;
	eck::g_pDwFactory->CreateTextFormat(lpFont, NULL, bBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, nSize, L"zh-cn", pTextFormat);
	if (*pTextFormat)
	{
		(*pTextFormat)->SetParagraphAlignment(parAlign);
		(*pTextFormat)->SetTextAlignment(textAlign);
		(*pTextFormat)->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
		(*pTextFormat)->SetWordWrapping(bWrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
		eck::g_pDwFactory->CreateTextLayout(lpText, lstrlenW(lpText), (*pTextFormat), prect.right - prect.left, prect.bottom - prect.top, pTextLayout);

	}
}

void OYM_CvsDrawText(ID2D1DeviceContext* pContext, LPCWSTR lpText, LPCWSTR lpFont, INT nSize, BOOL bBold, ID2D1Brush* pBrush, D2D1_RECT_F rect, DWRITE_PARAGRAPH_ALIGNMENT parAlign, DWRITE_TEXT_ALIGNMENT textAlign, BOOL bWrap)
{
	FLOAT cvsWidth = pContext->GetSize().width;
	D2D1_RECT_F prect = rect;
	ComPtr<IDWriteTextFormat> pTextFormat = nullptr;
	ComPtr<IDWriteTextLayout> pTextLayout = nullptr;
	DWRITE_TEXT_METRICS textMetrics;
	OYM_CvsCreateText(pContext, lpText, lpFont, nSize, bBold, rect, parAlign, textAlign, bWrap, &pTextFormat, &pTextLayout);
	if (pTextFormat.Get())
	{
		if (pTextLayout.Get())
		{
			pContext->DrawTextLayout(D2D1::Point2F(prect.left, prect.top), pTextLayout.Get(), pBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
			pTextLayout.Get()->GetMetrics(&textMetrics);

		}

	}

}


UINT GDIClrToCommonClr(COLORREF cr)
{
	BYTE by[4];
	UINT u;
	memcpy(by, &cr, 4);
	by[3] = by[0];
	by[0] = by[2];
	by[2] = by[3];
	by[3] = 0;
	memcpy(&u, by, 4);
	return u;
}


void UpdateHighlightColor() {
	DWORD crColorization;
	BOOL fOpaqueBlend;
	COLORREF theme_color{};
	HRESULT result = DwmGetColorizationColor(&crColorization, &fOpaqueBlend);
	if (result == S_OK) {
		BYTE r, g, b;
		r = (crColorization >> 16) % 256;
		g = (crColorization >> 8) % 256;
		b = crColorization % 256;
		theme_color = RGB(r, g, b);

		highlightColor = GDIClrToCommonClr(theme_color);
	}
}