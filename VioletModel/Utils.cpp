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
    float4 Rect;          // (0, 0, W, H)
    float2 Origin;
    float2 Direction;
    float  MaxSigma;
    float  BlurExtent;
    float  Falloff;
    float  KernelRadius;
};

struct PSInput
{
    float4 pos      : SV_POSITION;
    float4 posScene : SCENE_POSITION;
    float4 uv0      : TEXCOORD0;
};

#define SAMPLES 64
static const float GOLDEN_ANGLE = 2.39996323;

float4 main(PSInput input) : SV_TARGET
{
    float2 imgSize  = Rect.zw - Rect.xy;
    float2 uv       = input.uv0.xy;
    float2 pixelPos = uv * imgSize + Rect.xy;

    // ---- 位置相关的 sigma ----
    float dist  = dot(pixelPos - Origin, Direction);
    float t     = saturate(dist / max(BlurExtent, 1.0));
    float sigma = MaxSigma * pow(1.0 - t, Falloff);

    // 模糊太弱就直接采样原图，省性能
    if (sigma < 0.4)
        return InputTexture.Sample(InputSampler, uv);

    float2 invSize = 1.0 / max(imgSize, float2(1, 1));

    // ---- 二维黄金螺旋采样 ----
    float4 sum  = InputTexture.Sample(InputSampler, uv);
    float  wsum = 1.0;

    float radiusScale = sigma * 3.0;   // 覆盖 3σ

    [loop]
    for (int i = 1; i < SAMPLES; ++i)
    {
        float fi = (float)i;
        // 面积均匀分布（r 的平方与 i 成正比）
        float r  = sqrt(fi / (float)(SAMPLES - 1));   // 0..1
        float theta = fi * GOLDEN_ANGLE;

        // 像素空间圆盘 → UV 空间
        float2 offsetPix = float2(cos(theta), sin(theta)) * r * radiusScale;
        float2 offsetUV  = offsetPix * invSize;

        // 高斯权重：r=0 → 1，r=1 → exp(-4.5) ≈ 0.011
        float w = exp(-4.5 * r * r);

        sum  += InputTexture.Sample(InputSampler, uv + offsetUV) * w;
        wsum += w;
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
};                        // 总 48 字节 ✓

// 全局参数（单线程 UI 场景可直接用）
static PBConstants g_pbConstants = {};

// ============================================================
//  Transform（只实现 ID2D1DrawTransform）
// ============================================================
class ProgressiveBlurTransform : public ID2D1DrawTransform
{
public:
	ProgressiveBlurTransform() : m_ref(1), m_drawInfo(nullptr) {}
	~ProgressiveBlurTransform()
	{
		if (m_drawInfo) { m_drawInfo->Release(); m_drawInfo = nullptr; }
	}

	// ---- IUnknown ----
	IFACEMETHOD_(ULONG, AddRef)() override
	{
		return InterlockedIncrement(&m_ref);
	}
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

	// ---- ID2D1TransformNode ----
	IFACEMETHOD_(UINT32, GetInputCount)() const override { return 1; }

	// ---- ID2D1Transform ----
	IFACEMETHOD(MapOutputRectToInputRects)(
		const D2D1_RECT_L* out, D2D1_RECT_L* in, UINT32 count) const override
	{
		if (!out || !in || count < 1) return E_INVALIDARG;
		const int pad = (int)(g_pbConstants.maxSigma * 3.0f + 2.0f);
		in[0].left = out->left - pad;
		in[0].top = out->top - pad;
		in[0].right = out->right + pad;
		in[0].bottom = out->bottom + pad;
		return S_OK;
	}

	IFACEMETHOD(MapInputRectsToOutputRect)(
		const D2D1_RECT_L* in, const D2D1_RECT_L*, UINT32 count,
		D2D1_RECT_L* out, D2D1_RECT_L* outOp) override
	{
		if (!in || !out || !outOp || count < 1) return E_INVALIDARG;
		*out = in[0];
		*outOp = D2D1::RectL(0, 0, 0, 0);
		return S_OK;
	}

	IFACEMETHOD(MapInvalidRect)(
		UINT32, D2D1_RECT_L invalid, D2D1_RECT_L* out) const override
	{
		if (!out) return E_POINTER;
		*out = invalid;
		return S_OK;
	}

	// ---- ID2D1DrawTransform ----
	IFACEMETHOD(SetDrawInfo)(ID2D1DrawInfo* drawInfo) override
	{
		OutputDebugStringA("[PB] SetDrawInfo called\n");

		if (m_drawInfo) { m_drawInfo->Release(); m_drawInfo = nullptr; }
		m_drawInfo = drawInfo;
		if (m_drawInfo) m_drawInfo->AddRef();

		if (!m_drawInfo)
		{
			OutputDebugStringA("[PB] drawInfo is NULL\n");
			return E_FAIL;
		}

		HRESULT hr = m_drawInfo->SetPixelShader(GUID_ProgressiveBlurPS);
		char buf[128];
		sprintf_s(buf, "[PB] SetPixelShader hr=0x%08X\n", (unsigned)hr);
		OutputDebugStringA(buf);
		return hr;
	}

	HRESULT UpdateConstants()
	{
		if (!m_drawInfo) { OutputDebugStringA("[PB] UpdateConstants: no drawInfo\n"); return E_FAIL; }
		HRESULT hr = m_drawInfo->SetPixelShaderConstantBuffer(
			reinterpret_cast<const BYTE*>(&g_pbConstants),
			sizeof(g_pbConstants));
		char buf[128];
		sprintf_s(buf, "[PB] SetConstantBuffer hr=0x%08X\n", (unsigned)hr);
		OutputDebugStringA(buf);
		return hr;
	}

private:
	LONG m_ref;
	ID2D1DrawInfo* m_drawInfo;
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
		if (m_transform) { m_transform->Release(); m_transform = nullptr; }
	}

	// ---- IUnknown ----
	IFACEMETHOD_(ULONG, AddRef)() override
	{
		return InterlockedIncrement(&m_ref);
	}
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
			riid == __uuidof(ID2D1EffectImpl))
		{
			*ppv = static_cast<ID2D1EffectImpl*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	// ---- ID2D1EffectImpl ----
	IFACEMETHOD(Initialize)(ID2D1EffectContext* ctx,
		ID2D1TransformGraph* graph) override
	{
		static bool s_shaderLoaded = false;
		if (!s_shaderLoaded)
		{
			ID3DBlob* code = nullptr;
			ID3DBlob* err = nullptr;
			HRESULT hr = D3DCompile(
				g_pszProgressiveBlurHLSL,
				sizeof(g_pszProgressiveBlurHLSL) - 1,
				nullptr, nullptr, nullptr,
				"main", "ps_4_0",
				D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
				&code, &err);

			if (FAILED(hr))
			{
				if (err)
				{
					OutputDebugStringA("[PB] D3DCompile FAILED:\n");
					OutputDebugStringA((LPCSTR)err->GetBufferPointer());
					err->Release();
				}
				if (code) code->Release();
				return hr;
			}
			OutputDebugStringA("[PB] D3DCompile OK\n");

			hr = ctx->LoadPixelShader(GUID_ProgressiveBlurPS,
				(const BYTE*)code->GetBufferPointer(),
				(UINT32)code->GetBufferSize());

			code->Release();
			if (err) err->Release();

			if (FAILED(hr))
			{
				char buf[128];
				sprintf_s(buf, "[PB] LoadPixelShader FAILED hr=0x%08X\n", (unsigned)hr);
				OutputDebugStringA(buf);
				return hr;
			}
			OutputDebugStringA("[PB] LoadPixelShader OK\n");
			s_shaderLoaded = true;
		}

		m_transform = new (std::nothrow) ProgressiveBlurTransform();
		if (!m_transform) return E_OUTOFMEMORY;

		HRESULT hr2 = graph->SetSingleTransformNode(m_transform);
		if (FAILED(hr2))
		{
			char buf[128];
			sprintf_s(buf, "[PB] SetSingleTransformNode FAILED hr=0x%08X\n", (unsigned)hr2);
			OutputDebugStringA(buf);
			return hr2;
		}
		OutputDebugStringA("[PB] SetSingleTransformNode OK\n");
		return S_OK;
	}

	IFACEMETHOD(PrepareForRender)(D2D1_CHANGE_TYPE) override
	{
		if (!m_transform) return E_FAIL;
		return m_transform->UpdateConstants();
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

	// ---- 拷贝源区域到临时位图 ----
	ComPtr<ID2D1Bitmap> pBmpSrc;
	hr = pDC->CreateBitmap(
		{ (UINT32)((rc.right - rc.left) * Zoom),
		  (UINT32)((rc.bottom - rc.top) * Zoom) },
		NULL, 0,
		D2D1::BitmapProperties(pBmp->GetPixelFormat(), dpiX, dpiY),
		&pBmpSrc);
	if (FAILED(hr)) return hr;

	const D2D1_RECT_U rcU{
		(UINT32)(rc.left * Zoom), (UINT32)(rc.top * Zoom),
		(UINT32)(rc.right * Zoom), (UINT32)(rc.bottom * Zoom) };
	pBmpSrc->CopyFromBitmap(NULL, pBmp.Get(), &rcU);

	// ---- 创建自定义效果 ----
	ComPtr<ID2D1Effect> pEffect;
	hr = pDC->CreateEffect(CLSID_ProgressiveBlurEffect, &pEffect);
	if (FAILED(hr)) return hr;
	pEffect->SetInput(0, pBmpSrc.Get());

	// ---- 计算图像尺寸 ----
	const float W = (float)((rc.right - rc.left) * Zoom);
	const float H = (float)((rc.bottom - rc.top) * Zoom);

	// ---- 根据方向枚举决定 Origin 和 Direction ----
	// 说明：坐标系为临时位图局部坐标，(0,0) 在左上，(W,H) 在右下
	// Origin 指向"模糊最强端"的位置
	float ox = 0.0f, oy = 0.0f;
	float dx = 0.0f, dy = 1.0f;
	switch (direction)
	{
	case BlurDirection::TopToBottom:   // 上 -> 下（最模糊在上）
		ox = 0.0f; oy = 0.0f;
		dx = 0.0f; dy = 1.0f;
		break;
	case BlurDirection::BottomToTop:   // 下 -> 上（最模糊在下）
		ox = 0.0f; oy = H;
		dx = 0.0f; dy = -1.0f;
		break;
	case BlurDirection::LeftToRight:   // 左 -> 右（最模糊在左）
		ox = 0.0f; oy = 0.0f;
		dx = 1.0f; dy = 0.0f;
		break;
	case BlurDirection::RightToLeft:   // 右 -> 左（最模糊在右）
		ox = W;    oy = 0.0f;
		dx = -1.0f; dy = 0.0f;
		break;
	}

	// ---- 填 shader 常量 ----
	g_pbConstants.rect[0] = 0.0f;
	g_pbConstants.rect[1] = 0.0f;
	g_pbConstants.rect[2] = W;
	g_pbConstants.rect[3] = H;

	g_pbConstants.origin[0] = ox;
	g_pbConstants.origin[1] = oy;

	g_pbConstants.direction[0] = dx;
	g_pbConstants.direction[1] = dy;

	g_pbConstants.maxSigma = (fMaxDeviation > 0.0f) ? fMaxDeviation : 12.0f;
	// 过渡距离取方向上的总跨度
	g_pbConstants.blurExtent = (dx != 0.0f) ? W : H;
	g_pbConstants.falloff = 1.4f;   // iOS 观感 1.2 ~ 1.6
	g_pbConstants.kernelRadius = 12.0f;  // 与 HLSL 循环上界一致

	// ---- 圆角遮罩 + 绘制 ----
	const auto iBlend = pDC->GetPrimitiveBlend();
	pDC->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);

	RoundedRectMaskD2dDC(pDC, pFactory,
		D2D1::RectF(point.x * Zoom, point.y * Zoom,
			point.x * Zoom + W, point.y * Zoom + H),
		(float)(borderRadius * Zoom));

	pDC->Clear(D2D1::ColorF(0x000000, 0));
	pDC->DrawImage(pEffect.Get(),
		D2D1::Point2F((float)(point.x * Zoom), (float)(point.y * Zoom)));
	pDC->PopLayer();
	pDC->SetPrimitiveBlend(iBlend);

	return S_OK;
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