#pragma once

Tag::Result VltGetMusicInfo(PCWSTR pszFile,
	Tag::MUSICINFO& mi, const Tag::SIMPLE_OPT& Opt);

HRESULT RoundedRectMaskD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, float borderRadius);
HRESULT RoundedRectMaskXYD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, float radiusX, float radiusY);
HRESULT BlurD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, D2D1_POINT_2F point, float fDeviation, double borderRadius);
HRESULT OpacityMaskD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, D2D1_POINT_2F point, ID2D1Brush* opacityBrush, double borderRadius);
void OYM_CvsDrawText(ID2D1DeviceContext* pContext, LPCWSTR lpText, LPCWSTR lpFont, INT nSize, BOOL bBold, ID2D1Brush* pBrush, D2D1_RECT_F rect, DWRITE_PARAGRAPH_ALIGNMENT parAlign, DWRITE_TEXT_ALIGNMENT textAlign, BOOL bWrap);
UINT GDIClrToCommonClr(COLORREF cr);
void UpdateHighlightColor();