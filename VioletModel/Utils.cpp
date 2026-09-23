#include "pch.h"
#include "Utils.h"
#include "CApp.h"
#include "wrl/client.h"

D2D1_LAYER_PARAMETERS layer_param;
ComPtr<ID2D1RoundedRectangleGeometry> rrect_{};

#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <d2d1effectauthor.h> 
#pragma comment(lib, "d3dcompiler.lib")

// ============================================================
//  Progressive Blur (iOS style) - 自定义像素着色器
//  支持四个方向：上->下 / 下->上 / 左->右 / 右->左
// ============================================================

static const char g_pszProgressiveBlurHLSL[] = R"(
Texture2D InputTexture : register(t0);
SamplerState InputSampler : register(s0);

cbuffer Constants : register(b0)
{
    float4 Rect;
    float2 Origin;
    float2 Direction;
    float  MaxSigma;
    float  BlurExtent;
    float  Falloff;
    float  KernelRadius;
    float2 BlurAxis;
    float  PassIndex;
    float  Padding;
};

struct PSInput
{
    float4 pos      : SV_POSITION;
    float4 posScene : SCENE_POSITION;
    float4 uv0      : TEXCOORD0;
};

#define HALF_N 6

float4 Blur1D(float2 uv, float2 dir, float sigma, float2 imgSize)
{
    float2 stepUV = dir * (sigma / 3.0) / imgSize;
    float4 sum  = InputTexture.Sample(InputSampler, saturate(uv));
    float  wsum = 1.0;

    [loop]
    for (int i = 1; i <= HALF_N; ++i)
    {
        float fi = (float)i;
        float w  = exp(-0.5 * (fi / 3.0) * (fi / 3.0));
        float2 o = stepUV * fi;
        sum  += InputTexture.Sample(InputSampler, saturate(uv + o)) * w;
        sum  += InputTexture.Sample(InputSampler, saturate(uv - o)) * w;
        wsum += 2.0 * w;
    }
    return sum / wsum;
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv0.xy;

    float2 imgSize = Rect.zw;
    if (imgSize.x < 1.0) imgSize.x = 1.0;
    if (imgSize.y < 1.0) imgSize.y = 1.0;

    float2 pixelPos = uv * imgSize;

    float dist  = dot(pixelPos - Origin, Direction);
    float t     = saturate(dist / max(BlurExtent, 1.0));
    float sigma = MaxSigma * pow(1.0 - t, Falloff);

    if (sigma < 0.3)
        return InputTexture.Sample(InputSampler, saturate(uv));

    float2 stepY = float2(0.0, (sigma / 3.0) / imgSize.y);

    float4 sum  = Blur1D(uv, float2(1.0, 0.0), sigma, imgSize);
    float  wsum = 1.0;

    [loop]
    for (int j = 1; j <= HALF_N; ++j)
    {
        float fj = (float)j;
        float w  = exp(-0.5 * (fj / 3.0) * (fj / 3.0));
        float2 o = stepY * fj;
        sum  += Blur1D(uv + o, float2(1.0, 0.0), sigma, imgSize) * w;
        sum  += Blur1D(uv - o, float2(1.0, 0.0), sigma, imgSize) * w;
        wsum += 2.0 * w;
    }

    return sum / wsum;
}
)";

// {9B1C8E2A-4D3F-4A71-B211-8E9C0A1F3377}
static const GUID CLSID_ProgressiveBlurEffect =
{ 0x9b1c8e2a, 0x4d3f, 0x4a71, { 0xb2, 0x11, 0x8e, 0x9c, 0x0a, 0x1f, 0x33, 0x77 } };

// {2F6A1D84-9C22-4E5B-813A-D4771122ABCD}
static const GUID GUID_ProgressiveBlurPS =
{ 0x2f6a1d84, 0x9c22, 0x4e5b, { 0x81, 0x3a, 0xd4, 0x77, 0x11, 0x22, 0xab, 0xcd } };

// 常量缓冲（对应 HLSL cbuffer 布局，16 字节对齐）
struct PBConstants
{
	float rect[4];        // offset 0
	float origin[2];      // offset 16
	float direction[2];   // offset 24
	float maxSigma;       // offset 32
	float blurExtent;     // offset 36
	float falloff;        // offset 40
	float kernelRadius;   // offset 44
	float blurAxis[2];    // offset 48
	float passIndex;      // offset 56
	float padding;        // offset 60
};

// 全局参数（单线程 UI 场景可直接用）
static PBConstants g_pbConstants = {};

// ============================================================
//  Transform（只实现 ID2D1DrawTransform）
// ============================================================
class ProgressiveBlurTransform : public ID2D1DrawTransform
{
public:
	ProgressiveBlurTransform(float ax, float ay, float passIdx)
		: m_ref(1), m_drawInfo(nullptr), m_passIndex(passIdx)
	{
		m_axis[0] = ax;
		m_axis[1] = ay;
	}
	~ProgressiveBlurTransform()
	{
		if (m_drawInfo) { m_drawInfo->Release(); m_drawInfo = nullptr; }
	}

	IFACEMETHOD_(ULONG, AddRef)() override { return InterlockedIncrement(&m_ref); }
	IFACEMETHOD_(ULONG, Release)() override
	{
		ULONG c = InterlockedDecrement(&m_ref);
		if (c == 0) delete this;
		return c;
	}
	IFACEMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (!ppv) return E_POINTER;
		if (riid == __uuidof(IUnknown) ||
			riid == __uuidof(ID2D1DrawTransform) ||
			riid == __uuidof(ID2D1Transform) ||
			riid == __uuidof(ID2D1TransformNode))
		{
			*ppv = static_cast<ID2D1DrawTransform*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	IFACEMETHOD_(UINT32, GetInputCount)() const override { return 1; }

	IFACEMETHOD(MapOutputRectToInputRects)(
		const D2D1_RECT_L* out, D2D1_RECT_L* in, UINT32 count) const override
	{
		if (!out || !in || count < 1) return E_INVALIDARG;

		in[0] = *out;
		return S_OK;
	}

	IFACEMETHOD(MapInputRectsToOutputRect)(
		const D2D1_RECT_L* in, const D2D1_RECT_L* inOp, UINT32 count,
		D2D1_RECT_L* out, D2D1_RECT_L* outOp) override
	{
		if (!in || !out || !outOp || count < 1) return E_INVALIDARG;
		*out = in[0];
		*outOp = in[0];   // 关键：整个输出都是不透明的
		return S_OK;
	}

	IFACEMETHOD(MapInvalidRect)(
		UINT32, D2D1_RECT_L invalid, D2D1_RECT_L* out) const override
	{
		if (!out) return E_POINTER;
		*out = invalid;
		return S_OK;
	}

	IFACEMETHOD(SetDrawInfo)(ID2D1DrawInfo* drawInfo) override
	{
		if (m_drawInfo) { m_drawInfo->Release(); m_drawInfo = nullptr; }
		m_drawInfo = drawInfo;
		if (m_drawInfo) m_drawInfo->AddRef();
		if (!m_drawInfo) return E_FAIL;
		return m_drawInfo->SetPixelShader(GUID_ProgressiveBlurPS);
	}

	HRESULT UpdateConstants()
	{
		if (!m_drawInfo) return E_FAIL;

		PBConstants local = g_pbConstants;
		local.blurAxis[0] = m_axis[0];
		local.blurAxis[1] = m_axis[1];
		local.passIndex = m_passIndex;

		return m_drawInfo->SetPixelShaderConstantBuffer(
			reinterpret_cast<const BYTE*>(&local), sizeof(local));
	}

private:
	LONG m_ref;
	ID2D1DrawInfo* m_drawInfo;
	float m_axis[2];
	float m_passIndex;
};

// ============================================================
//  Effect（只实现 ID2D1EffectImpl）
// ============================================================
class ProgressiveBlurEffect : public ID2D1EffectImpl
{
public:
	ProgressiveBlurEffect() : m_ref(1), m_transform(nullptr) {}
	~ProgressiveBlurEffect()
	{
		if (m_transform) m_transform->Release();
	}

	IFACEMETHOD_(ULONG, AddRef)() override { return InterlockedIncrement(&m_ref); }
	IFACEMETHOD_(ULONG, Release)() override
	{
		ULONG c = InterlockedDecrement(&m_ref);
		if (c == 0) delete this;
		return c;
	}
	IFACEMETHOD(QueryInterface)(REFIID riid, void** ppv) override
	{
		if (!ppv) return E_POINTER;
		if (riid == __uuidof(IUnknown) || riid == __uuidof(ID2D1EffectImpl))
		{
			*ppv = static_cast<ID2D1EffectImpl*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	IFACEMETHOD(Initialize)(ID2D1EffectContext* ctx,
		ID2D1TransformGraph* graph) override
	{
		static bool s_shaderLoaded = false;
		if (!s_shaderLoaded)
		{
			ID3DBlob* code = nullptr;
			ID3DBlob* err = nullptr;
			HRESULT hr = D3DCompile(
				g_pszProgressiveBlurHLSL, sizeof(g_pszProgressiveBlurHLSL) - 1,
				nullptr, nullptr, nullptr, "main", "ps_4_0",
				D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);

			if (FAILED(hr))
			{
				if (err) { OutputDebugStringA((LPCSTR)err->GetBufferPointer()); err->Release(); }
				if (code) code->Release();
				return hr;
			}

			hr = ctx->LoadPixelShader(GUID_ProgressiveBlurPS,
				(const BYTE*)code->GetBufferPointer(),
				(UINT32)code->GetBufferSize());

			code->Release();
			if (err) err->Release();
			if (FAILED(hr)) return hr;
			s_shaderLoaded = true;
		}

		m_transform = new (std::nothrow) ProgressiveBlurTransform(0.0f, 0.0f, 0.0f);
		if (!m_transform) return E_OUTOFMEMORY;

		HRESULT hr2 = graph->SetSingleTransformNode(m_transform);
		if (FAILED(hr2)) return hr2;
		return S_OK;
	}

	IFACEMETHOD(PrepareForRender)(D2D1_CHANGE_TYPE) override
	{
		if (m_transform) return m_transform->UpdateConstants();
		return E_FAIL;
	}

	IFACEMETHOD(SetGraph)(ID2D1TransformGraph*) override { return E_NOTIMPL; }

private:
	LONG m_ref;
	ProgressiveBlurTransform* m_transform;
};

static HRESULT CALLBACK ProgressiveBlurEffectFactory(IUnknown** ppEffectImpl)
{
	if (!ppEffectImpl) return E_POINTER;
	ProgressiveBlurEffect* p = new (std::nothrow) ProgressiveBlurEffect();
	if (!p) return E_OUTOFMEMORY;
	*ppEffectImpl = static_cast<ID2D1EffectImpl*>(p);
	return S_OK;
}

// 懒注册：只执行一次
static HRESULT EnsureProgressiveBlurRegistered(ID2D1Factory1* factory)
{
	static bool s_registered = false;
	if (s_registered) return S_OK;

	PCWSTR xml = LR"(<?xml version='1.0'?>
<Effect>
    <Property name='DisplayName' type='string' value='ProgressiveBlur'/>
    <Property name='Author'      type='string' value='you'/>
    <Property name='Category'    type='string' value='Blur'/>
    <Property name='Description' type='string' value='iOS style progressive blur (4 directions)'/>
    <Inputs>
        <Input name='Source'/>
    </Inputs>
</Effect>)";

	HRESULT hr = factory->RegisterEffectFromString(
		CLSID_ProgressiveBlurEffect,
		xml,
		nullptr, 0,
		ProgressiveBlurEffectFactory);

	if (FAILED(hr)) return hr;
	s_registered = true;
	return S_OK;
}

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

HRESULT ProgressiveBlurD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory,
	const D2D1_RECT_F& rc, D2D1_POINT_2F point,
	float fMaxDeviation, BlurDirection direction,
	double borderRadius)
{
	ComPtr<ID2D1Bitmap1> pBmp;
	ComPtr<ID2D1Image>   pTarget;
	pDC->GetTarget(&pTarget);
	pTarget->QueryInterface(&pBmp);
	if (pBmp.Get() == nullptr)
		return S_FALSE;

	HRESULT hr = EnsureProgressiveBlurRegistered(pFactory);
	if (FAILED(hr)) return hr;

	float dpiX{}, dpiY{};
	pDC->GetDpi(&dpiX, &dpiY);
	const double Zoom = dpiX / 96.0;

	pDC->Flush();

	const D2D1_SIZE_U szBmp = pBmp->GetPixelSize();

	INT32 l = (INT32)floorf(rc.left * (float)Zoom);
	INT32 t = (INT32)floorf(rc.top * (float)Zoom);
	INT32 r = (INT32)ceilf(rc.right * (float)Zoom);
	INT32 b = (INT32)ceilf(rc.bottom * (float)Zoom);

	if (l < 0) l = 0;
	if (t < 0) t = 0;
	if (r > (INT32)szBmp.width)  r = (INT32)szBmp.width;
	if (b > (INT32)szBmp.height) b = (INT32)szBmp.height;
	if (r <= l) r = l + 1;
	if (b <= t) b = t + 1;

	const UINT32 W_phys = (UINT32)(r - l);
	const UINT32 H_phys = (UINT32)(b - t);

	const float srcLeftDip = l / (float)Zoom;
	const float srcTopDip = t / (float)Zoom;
	const float W_dip = W_phys / (float)Zoom;
	const float H_dip = H_phys / (float)Zoom;

	const float destX = point.x + (srcLeftDip - rc.left);
	const float destY = point.y + (srcTopDip - rc.top);

	ComPtr<ID2D1Bitmap> pBmpSrc;
	hr = pDC->CreateBitmap(
		{ W_phys, H_phys },
		NULL, 0,
		D2D1::BitmapProperties(pBmp->GetPixelFormat(), dpiX, dpiY),
		&pBmpSrc);
	if (FAILED(hr)) return hr;

	const D2D1_RECT_U rcU{ (UINT32)l, (UINT32)t, (UINT32)r, (UINT32)b };
	hr = pBmpSrc->CopyFromBitmap(NULL, pBmp.Get(), &rcU);
	if (FAILED(hr)) return hr;

	const float Wf = (float)W_phys;
	const float Hf = (float)H_phys;

	float ox = 0.0f, oy = 0.0f;
	float dx = 0.0f, dy = 1.0f;
	switch (direction)
	{
	case BlurDirection::TopToBottom:
		ox = 0.0f; oy = 0.0f;
		dx = 0.0f; dy = 1.0f;
		break;
	case BlurDirection::BottomToTop:
		ox = 0.0f; oy = Hf;
		dx = 0.0f; dy = -1.0f;
		break;
	case BlurDirection::LeftToRight:
		ox = 0.0f; oy = 0.0f;
		dx = 1.0f; dy = 0.0f;
		break;
	case BlurDirection::RightToLeft:
		ox = Wf;   oy = 0.0f;
		dx = -1.0f; dy = 0.0f;
		break;
	}

	g_pbConstants.rect[0] = 0.0f;
	g_pbConstants.rect[1] = 0.0f;
	g_pbConstants.rect[2] = Wf;
	g_pbConstants.rect[3] = Hf;

	g_pbConstants.origin[0] = ox;
	g_pbConstants.origin[1] = oy;

	g_pbConstants.direction[0] = dx;
	g_pbConstants.direction[1] = dy;

	g_pbConstants.maxSigma = (fMaxDeviation > 0.0f) ? fMaxDeviation : 12.0f;
	g_pbConstants.blurExtent = (dx != 0.0f) ? Wf : Hf;
	g_pbConstants.falloff = 1.4f;
	g_pbConstants.kernelRadius = 12.0f;

	ComPtr<ID2D1Effect> pEffect;
	hr = pDC->CreateEffect(CLSID_ProgressiveBlurEffect, &pEffect);
	if (FAILED(hr)) return hr;
	pEffect->SetInput(0, pBmpSrc.Get());

	const auto iBlend = pDC->GetPrimitiveBlend();
	pDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

	RoundedRectMaskD2dDC(pDC, pFactory,
		D2D1::RectF(destX, destY, destX + W_dip, destY + H_dip),
		(float)borderRadius);

	pDC->Clear(D2D1::ColorF(0x000000, 0));
	pDC->DrawImage(pEffect.Get(), D2D1::Point2F(destX, destY));
	pDC->PopLayer();
	pDC->SetPrimitiveBlend(iBlend);

	return S_OK;
}

HRESULT OpacityMaskD2dDC(ID2D1DeviceContext* pDC, ID2D1Factory1* pFactory,
	const D2D1_RECT_F& rc, D2D1_POINT_2F point,
	ID2D1Brush* opacityBrush, double borderRadius)
{
	ComPtr<ID2D1Bitmap1> pBmp;
	ComPtr<ID2D1Image>   pTarget;
	pDC->GetTarget(&pTarget);
	pTarget->QueryInterface(&pBmp);
	if (pBmp.Get() == nullptr)
		return S_FALSE;

	HRESULT hr;
	float dpiX{}, dpiY{};
	pDC->GetDpi(&dpiX, &dpiY);
	const double Zoom = dpiX / 96.0;

	pDC->Flush();

	const D2D1_SIZE_U szBmp = pBmp->GetPixelSize();

	INT32 l = (INT32)floorf(rc.left * (float)Zoom);
	INT32 t = (INT32)floorf(rc.top * (float)Zoom);
	INT32 r = (INT32)ceilf(rc.right * (float)Zoom);
	INT32 b = (INT32)ceilf(rc.bottom * (float)Zoom);

	if (l < 0) l = 0;
	if (t < 0) t = 0;
	if (r > (INT32)szBmp.width)  r = (INT32)szBmp.width;
	if (b > (INT32)szBmp.height) b = (INT32)szBmp.height;
	if (r <= l) r = l + 1;
	if (b <= t) b = t + 1;

	const UINT32 W_phys = (UINT32)(r - l);
	const UINT32 H_phys = (UINT32)(b - t);

	const float srcLeftDip = l / (float)Zoom;
	const float srcTopDip = t / (float)Zoom;
	const float W_dip = W_phys / (float)Zoom;
	const float H_dip = H_phys / (float)Zoom;

	const float destX = point.x + (srcLeftDip - rc.left);
	const float destY = point.y + (srcTopDip - rc.top);

	ComPtr<ID2D1Bitmap> pBmpEffect;
	hr = pDC->CreateBitmap(
		{ W_phys, H_phys }, NULL, 0,
		D2D1::BitmapProperties(pBmp->GetPixelFormat(), dpiX, dpiY),
		&pBmpEffect);
	if (FAILED(hr)) return hr;

	const D2D1_RECT_U rcU{ (UINT32)l, (UINT32)t, (UINT32)r, (UINT32)b };
	hr = pBmpEffect->CopyFromBitmap(NULL, pBmp.Get(), &rcU);
	if (FAILED(hr)) return hr;

	const auto iBlend = pDC->GetPrimitiveBlend();
	pDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

	D2D1_LAYER_PARAMETERS1 LyParam{ D2D1::LayerParameters1() };
	LyParam.contentBounds = D2D1::RectF(destX, destY,
		destX + W_dip, destY + H_dip);
	LyParam.opacityBrush = opacityBrush;

	pDC->PushLayer(&LyParam, NULL);

	pDC->DrawImage(
		pBmpEffect.Get(),
		D2D1::Point2F(destX, destY),
		D2D1::RectF(0, 0, W_dip, H_dip),
		D2D1_INTERPOLATION_MODE_LINEAR,
		D2D1_COMPOSITE_MODE_SOURCE_OVER);

	pDC->PopLayer();
	pDC->SetPrimitiveBlend(iBlend);

	return S_OK;
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