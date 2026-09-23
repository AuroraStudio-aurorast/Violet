#pragma once

Tag::Result VltGetMusicInfo(PCWSTR pszFile,
	Tag::MUSICINFO& mi, const Tag::SIMPLE_OPT& Opt);

enum class BlurDirection
{
    TopToBottom,   // 上 -> 下（最模糊在上）
    BottomToTop,   // 下 -> 上（最模糊在下）
    LeftToRight,   // 左 -> 右（最模糊在左）
    RightToLeft,   // 右 -> 左（最模糊在右）
};

HRESULT ProgressiveBlurD2dDC(
    ID2D1DeviceContext* pDC,
    ID2D1Factory1* pFactory,
    const D2D1_RECT_F& rc,
    D2D1_POINT_2F point,
    float fMaxDeviation,          // 最强端的 sigma，典型 8~20
    BlurDirection direction,      // 四个方向之一
    double borderRadius);

HRESULT RoundedRectMaskD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, float borderRadius);
HRESULT RoundedRectMaskXYD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, float radiusX, float radiusY);
HRESULT BlurD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, D2D1_POINT_2F point, float fDeviation, double borderRadius);
HRESULT OpacityMaskD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory, const D2D1_RECT_F& rc, D2D1_POINT_2F point, ID2D1Brush* opacityBrush, double borderRadius);
void OYM_CvsDrawText(ID2D1DeviceContext* pContext, LPCWSTR lpText, LPCWSTR lpFont, INT nSize, BOOL bBold, ID2D1Brush* pBrush, D2D1_RECT_F rect, DWRITE_PARAGRAPH_ALIGNMENT parAlign, DWRITE_TEXT_ALIGNMENT textAlign, BOOL bWrap);
UINT GDIClrToCommonClr(COLORREF cr);
void UpdateHighlightColor();