#include "pch.h"
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")
#include "CVeVideoPlayer.h"

#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "ole32.lib")

#ifdef _DEBUG
#define VDBG(fmt, ...)                                          \
    do {                                                        \
        wchar_t _b_[1024];                                      \
        swprintf_s(_b_, fmt, ##__VA_ARGS__);                    \
        OutputDebugStringW(_b_);                                \
    } while (0)
#else
#define VDBG(fmt, ...) ((void)0)
#endif

// ============================================================
// 构造 / 析构
// ============================================================
CVeVideoPlayer::CVeVideoPlayer() = default;

CVeVideoPlayer::~CVeVideoPlayer()
{
    Stop();
    CloseInternal();
    DestroyTexturePool();
    DetachDevice();
}

// ============================================================
// 设备附加
// ============================================================
void CVeVideoPlayer::AttachDevice(ID3D11Device* pDev,
    ID3D11DeviceContext* pCtx,
    ID2D1DeviceContext* pD2D)
{
    DetachDevice();

    m_pD3DDevice = pDev;
    if (m_pD3DDevice) m_pD3DDevice->AddRef();
    m_pD3DContext = pCtx;
    if (m_pD3DContext) m_pD3DContext->AddRef();
    m_pD2DContext = pD2D;
    if (m_pD2DContext) m_pD2DContext->AddRef();

    if (m_pD3DDevice)
        m_pD3DDevice->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&m_pVideoDevice);
    if (m_pD3DContext)
        m_pD3DContext->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&m_pVideoContext);

    if (m_pD3DDevice)
    {
        eck::ComPtr<ID3D10Multithread> pMT;
        if (SUCCEEDED(m_pD3DDevice->QueryInterface(IID_PPV_ARGS(&pMT))))
            pMT->SetMultithreadProtected(TRUE);
    }

    m_bDeviceAttached = true;
}

void CVeVideoPlayer::DetachDevice()
{
    if (m_pVideoDevice) { m_pVideoDevice->Release();  m_pVideoDevice = nullptr; }
    if (m_pVideoContext) { m_pVideoContext->Release(); m_pVideoContext = nullptr; }
    if (m_pD2DContext) { m_pD2DContext->Release();   m_pD2DContext = nullptr; }
    if (m_pD3DContext) { m_pD3DContext->Release();   m_pD3DContext = nullptr; }
    if (m_pD3DDevice) { m_pD3DDevice->Release();    m_pD3DDevice = nullptr; }
    m_bDeviceAttached = false;
}

// ============================================================
// 从框架全局设备反查 D3D11，自动附加
// ============================================================
bool CVeVideoPlayer::AttachDeviceFromFramework()
{
    if (m_bDeviceAttached) return true;

    // m_pWnd / m_pDC 是 CElem 的私有成员，必须用公有的 GetWnd() / GetDC()
    auto* pWnd = GetWnd();
    if (!pWnd) return false;
    auto* pDC = GetDC();
    if (!pDC) return false;

    // g_pDxgiDevice 在 eck 命名空间（和 eck::g_pD2DFactory 同级），
    // 不在 eck::Dui 里。如果你项目里发现它在别处，换成对应的前缀即可。
    auto* pDxgi = eck::g_pDxgiDevice;
    if (!pDxgi) return false;

    ID3D11Device* pD3D = nullptr;
    if (FAILED(pDxgi->QueryInterface(IID_PPV_ARGS(&pD3D))))
        return false;

    ID3D11DeviceContext* pCtx = nullptr;
    pD3D->GetImmediateContext(&pCtx);

    AttachDevice(pD3D, pCtx, pDC);

    if (pCtx) pCtx->Release();
    pD3D->Release();
    return true;
}

// ============================================================
// 确保纹理池与视频尺寸匹配
// 尺寸没变就复用，避免重复创建导致 D2D 内部缓存位图累积泄漏
// ============================================================
void CVeVideoPlayer::EnsureTexturePool(int cx, int cy)
{
    if (cx <= 0 || cy <= 0) return;

    // 尺寸相同且池已经存在：复用
    if (m_cxPoolVideo == cx && m_cyPoolVideo == cy && !m_vecBmpPool.empty())
        return;

    // 尺寸变化：先销毁旧的
    DestroyTexturePool();

    m_cxPoolVideo = cx;
    m_cyPoolVideo = cy;

    for (int i = 0; i < 4; ++i)
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = cx;
        td.Height = cy;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc = { 1, 0 };
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D* pTex = nullptr;
        if (FAILED(m_pD3DDevice->CreateTexture2D(&td, nullptr, &pTex)))
            return;
        m_vecBgraPool.push_back(pTex);
        m_vecPoolBusy.push_back(false);

        eck::ComPtr<IDXGISurface> pSurf;
        pTex->QueryInterface(IID_PPV_ARGS(&pSurf));

        D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

        ID2D1Bitmap1* pBmp = nullptr;
        if (FAILED(m_pD2DContext->CreateBitmapFromDxgiSurface(pSurf.Get(), &bp, &pBmp)))
            return;
        m_vecBmpPool.push_back(pBmp);
    }
}

// ============================================================
// 销毁纹理池
// ============================================================
void CVeVideoPlayer::DestroyTexturePool()
{
    // 1) 清空队列和当前帧，它们持有池中纹理/位图的裸指针
    ClearVideoQueue();
    if (m_lastFrame.pTexture)
    {
        ReleasePoolTextureByPtr(m_lastFrame.pTexture);
        m_lastFrame = {};
    }

    // 2) 解除 D2D 上下文对这些位图的引用
    //    D2D 会缓存最近使用的 source bitmap，SetTarget(nullptr) 触发它
    //    刷新缓存引用；如果当前 target 恰好是池中的 bitmap（不应该发生，
    //    但保险起见也清一下）。
    if (m_pD2DContext)
    {
        m_pD2DContext->SetTarget(nullptr);
    }

    // 3) 先释放位图，再释放纹理（位图隐式引用纹理，顺序反了会让纹理
    //    多活一轮，但更严重的是 D2D 内部缓存的位图引用不清会累积）
    for (auto* b : m_vecBmpPool) if (b) b->Release();
    m_vecBmpPool.clear();

    for (auto* t : m_vecBgraPool) if (t) t->Release();
    m_vecBgraPool.clear();

    m_vecPoolBusy.clear();
    m_cxPoolVideo = 0;
    m_cyPoolVideo = 0;
}

// ============================================================
// 打开 / 关闭
// ============================================================
bool CVeVideoPlayer::Open(const std::wstring& url)
{
    // 首次打开：确保设备已附加
    if (!m_bDeviceAttached)
    {
        if (!AttachDeviceFromFramework())
            return false;
    }

    Stop();
    CloseInternal();
    m_strUrl = url;
    m_eState = State::Opening;

    if (!OpenInternal(url))
    {
        CloseInternal();
        m_eState = State::Idle;
        return false;
    }

    m_bStopRequest = false;
    m_bPaused = false;
    m_eState = State::Playing;

    m_dbAudioClockBase = 0.0;
    m_ullAudioClockBase = 0;
    m_dbAudioClock = 0.0;
    m_dbStreamStartTime = 0.0;
    m_dbFallbackAcc = 0.0;
    m_ullFallbackTick = GetTickCount64();

    // 按视频帧率调整定时器间隔
    /*
    if (m_dbFps > 1.0 && m_dbFps < 240.0)
        m_uRenderIntervalMs = (UINT)(std::max)(5.0, 1000.0 / m_dbFps);
    else
        m_uRenderIntervalMs = 16;
    */
    m_uRenderIntervalMs = 16;

    // 启动音频线程
    if (m_bAudioDeviceReady)
    {
        m_bAudioRunning = true;
        m_bAudioClockReset = true;
        m_thAudio = std::thread(&CVeVideoPlayer::AudioThreadProc, this);
    }

    // 启动解码线程
    m_thDecode = std::thread(&CVeVideoPlayer::DecodeThreadProc, this);

    // 启动渲染定时器
    StartRenderTimer();
    return true;
}

void CVeVideoPlayer::CloseInternal()
{
    StopRenderTimer();

    if (m_pFmtCtx)      avformat_close_input(&m_pFmtCtx);
    if (m_pVCodecCtx)   avcodec_free_context(&m_pVCodecCtx);
    if (m_pACodecCtx)   avcodec_free_context(&m_pACodecCtx);
    if (m_pHwDeviceCtx) av_buffer_unref(&m_pHwDeviceCtx);
    if (m_pSwrCtx)      swr_free(&m_pSwrCtx);

    m_pFmtCtx = nullptr;
    m_pVCodecCtx = nullptr;
    m_pACodecCtx = nullptr;
    m_pHwDeviceCtx = nullptr;
    m_pSwrCtx = nullptr;
    m_iVideoStream = -1;
    m_iAudioStream = -1;
    m_dbDuration = 0.0;

    ClearVideoQueue();
    if (m_lastFrame.pTexture)
    {
        ReleasePoolTextureByPtr(m_lastFrame.pTexture);
        m_lastFrame = {};
    }

    if (m_pAudioClient) { m_pAudioClient->Stop(); m_pAudioClient->Release(); m_pAudioClient = nullptr; }
    if (m_pRenderClient) { m_pRenderClient->Release(); m_pRenderClient = nullptr; }
    if (m_pAudioClock) { m_pAudioClock->Release();   m_pAudioClock = nullptr; }
    if (m_pVolume) { m_pVolume->Release();       m_pVolume = nullptr; }
    if (m_pEnum) { m_pEnum->Release();         m_pEnum = nullptr; }
    delete m_pRing; m_pRing = nullptr;
    m_bAudioDeviceReady = false;

    if (m_pVideoProcessor) { m_pVideoProcessor->Release(); m_pVideoProcessor = nullptr; }
    if (m_pVpEnum) { m_pVpEnum->Release();         m_pVpEnum = nullptr; }

}

// ============================================================
// 打开文件 + 初始化所有子系统
// ============================================================
bool CVeVideoPlayer::OpenInternal(const std::wstring& url)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u8(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, u8.data(), n, nullptr, nullptr);

    if (avformat_open_input(&m_pFmtCtx, u8.c_str(), nullptr, nullptr) < 0) return false;
    if (avformat_find_stream_info(m_pFmtCtx, nullptr) < 0)                  return false;

    m_iVideoStream = av_find_best_stream(m_pFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    m_iAudioStream = av_find_best_stream(m_pFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (m_iVideoStream < 0) return false;

    AVStream* pVS = m_pFmtCtx->streams[m_iVideoStream];
    m_VideoTimeBase = pVS->time_base;
    m_dbDuration = m_pFmtCtx->duration / (double)AV_TIME_BASE;
    if (pVS->avg_frame_rate.den) m_dbFps = av_q2d(pVS->avg_frame_rate);

    if (pVS->start_time != AV_NOPTS_VALUE)
        m_dbStreamStartTime = pVS->start_time * av_q2d(m_VideoTimeBase);
    else
        m_dbStreamStartTime = 0.0;

    const AVCodec* pVCodec = avcodec_find_decoder(pVS->codecpar->codec_id);
    if (!pVCodec) return false;
    m_pVCodecCtx = avcodec_alloc_context3(pVCodec);
    avcodec_parameters_to_context(m_pVCodecCtx, pVS->codecpar);

    if (!InitHwDecoder()) return false;

    m_pVCodecCtx->get_format = [](AVCodecContext*, const enum AVPixelFormat* fmts) -> AVPixelFormat {
        for (const auto* p = fmts; *p != AV_PIX_FMT_NONE; ++p)
            if (*p == AV_PIX_FMT_D3D11) return *p;
        return fmts[0];
        };

    if (avcodec_open2(m_pVCodecCtx, pVCodec, nullptr) < 0) return false;

    m_cxVideo = m_pVCodecCtx->width;
    m_cyVideo = m_pVCodecCtx->height;

    if (!InitVideoProcessor(m_cxVideo, m_cyVideo)) return false;

    if (m_iAudioStream >= 0)
    {
        AVStream* pAS = m_pFmtCtx->streams[m_iAudioStream];
        const AVCodec* pACodec = avcodec_find_decoder(pAS->codecpar->codec_id);
        if (pACodec)
        {
            m_pACodecCtx = avcodec_alloc_context3(pACodec);
            avcodec_parameters_to_context(m_pACodecCtx, pAS->codecpar);
            if (avcodec_open2(m_pACodecCtx, pACodec, nullptr) >= 0)
                m_AudioTimeBase = pAS->time_base;
            else
            {
                avcodec_free_context(&m_pACodecCtx);
                m_iAudioStream = -1;
            }
        }
    }

    if (m_iAudioStream >= 0)
    {
        if (InitAudioOutput() && InitAudioResampler())
        {
            m_bAudioDeviceReady = true;
        }
        else
        {
            m_bAudioDeviceReady = false;
            if (m_pAudioClient) { m_pAudioClient->Release();  m_pAudioClient = nullptr; }
            if (m_pRenderClient) { m_pRenderClient->Release(); m_pRenderClient = nullptr; }
            if (m_pAudioClock) { m_pAudioClock->Release();   m_pAudioClock = nullptr; }
            if (m_pVolume) { m_pVolume->Release();       m_pVolume = nullptr; }
            if (m_pEnum) { m_pEnum->Release();         m_pEnum = nullptr; }
            if (m_pSwrCtx) swr_free(&m_pSwrCtx);
            delete m_pRing; m_pRing = nullptr;
        }
    }

    EnsureTexturePool(m_cxVideo, m_cyVideo);
    return true;
}

// ============================================================
// D3D11VA 硬解初始化
// ============================================================
bool CVeVideoPlayer::InitHwDecoder()
{
    if (!m_pD3DDevice || !m_pD3DContext) return false;

    m_pHwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!m_pHwDeviceCtx) return false;

    auto* pDevCtx = (AVHWDeviceContext*)m_pHwDeviceCtx->data;
    auto* pD3D11 = (AVD3D11VADeviceContext*)pDevCtx->hwctx;

    pD3D11->device = m_pD3DDevice;
    pD3D11->device_context = m_pD3DContext;

    pD3D11->lock_ctx = &m_mtxD3D;
    pD3D11->lock = [](void* p) { static_cast<std::mutex*>(p)->lock(); };
    pD3D11->unlock = [](void* p) { static_cast<std::mutex*>(p)->unlock(); };

    if (av_hwdevice_ctx_init(m_pHwDeviceCtx) < 0) return false;

    m_pVCodecCtx->hw_device_ctx = av_buffer_ref(m_pHwDeviceCtx);
    return true;
}

// ============================================================
// D3D11 视频处理器
// ============================================================
bool CVeVideoPlayer::InitVideoProcessor(int cx, int cy)
{
    if (!m_pVideoDevice) return false;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd{};
    cd.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    cd.InputWidth = cx;
    cd.InputHeight = cy;
    cd.OutputWidth = cx;
    cd.OutputHeight = cy;
    cd.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    if (FAILED(m_pVideoDevice->CreateVideoProcessorEnumerator(&cd, &m_pVpEnum))) return false;
    if (FAILED(m_pVideoDevice->CreateVideoProcessor(m_pVpEnum, 0, &m_pVideoProcessor))) return false;
    return true;
}

// ============================================================
// WASAPI 音频输出
// ============================================================
bool CVeVideoPlayer::InitAudioOutput()
{
    //HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    //if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE) return false;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&m_pEnum));
    if (FAILED(hr)) return false;

    IMMDevice* pDev = nullptr;
    hr = m_pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDev);
    if (FAILED(hr)) return false;

    hr = pDev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_pAudioClient);
    pDev->Release();
    if (FAILED(hr)) return false;

    WAVEFORMATEX* pMix = nullptr;
    hr = m_pAudioClient->GetMixFormat(&pMix);
    if (FAILED(hr)) return false;

    m_iAudioSampleRate = pMix->nSamplesPerSec;
    m_iAudioChannels = pMix->nChannels;
    m_iAudioBlockAlign = pMix->nBlockAlign;
    m_iAudioBytesPerSec = pMix->nAvgBytesPerSec;
    m_eAudioSampleFmt = AV_SAMPLE_FMT_FLT;

    hr = m_pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        10000000,
        0,
        pMix, nullptr);
    CoTaskMemFree(pMix);
    if (FAILED(hr)) return false;

    hr = m_pAudioClient->GetBufferSize(&m_uBufferFrames);
    if (FAILED(hr)) return false;
    hr = m_pAudioClient->GetService(IID_PPV_ARGS(&m_pRenderClient));
    if (FAILED(hr)) return false;
    hr = m_pAudioClient->GetService(IID_PPV_ARGS(&m_pAudioClock));
    if (FAILED(hr)) return false;
    hr = m_pAudioClient->GetService(IID_PPV_ARGS(&m_pVolume));
    if (FAILED(hr)) return false;

    m_pRing = new CVeAudioRing(m_iAudioBytesPerSec);
    return true;
}

bool CVeVideoPlayer::InitAudioResampler()
{
    if (!m_pACodecCtx) return false;

    AVChannelLayout inLayout{};
    if (m_pACodecCtx->ch_layout.nb_channels)
        av_channel_layout_copy(&inLayout, &m_pACodecCtx->ch_layout);
    else
        av_channel_layout_default(&inLayout, 2);

    AVChannelLayout outLayout{};
    av_channel_layout_default(&outLayout, m_iAudioChannels);

    int ret = swr_alloc_set_opts2(&m_pSwrCtx,
        &outLayout, m_eAudioSampleFmt, m_iAudioSampleRate,
        &inLayout, m_pACodecCtx->sample_fmt, m_pACodecCtx->sample_rate,
        0, nullptr);

    av_channel_layout_uninit(&inLayout);
    av_channel_layout_uninit(&outLayout);
    if (ret < 0) return false;
    if (swr_init(m_pSwrCtx) < 0) return false;
    return true;
}

// ============================================================
// 纹理池
// ============================================================
ID3D11Texture2D* CVeVideoPlayer::AcquirePoolTexture(int& idx)
{
    std::lock_guard<std::mutex> lk(m_mtxPool);
    for (size_t i = 0; i < m_vecPoolBusy.size(); ++i)
        if (!m_vecPoolBusy[i]) { m_vecPoolBusy[i] = true; idx = (int)i; return m_vecBgraPool[i]; }

    idx = 0;
    m_vecPoolBusy[0] = true;
    return m_vecBgraPool[0];
}

void CVeVideoPlayer::ReleasePoolTextureByPtr(ID3D11Texture2D* pTex)
{
    if (!pTex) return;
    std::lock_guard<std::mutex> lk(m_mtxPool);
    for (size_t i = 0; i < m_vecBgraPool.size(); ++i)
        if (m_vecBgraPool[i] == pTex) { m_vecPoolBusy[i] = false; return; }
}

// ============================================================
// NV12 -> BGRA
// ============================================================
bool CVeVideoPlayer::ConvertToBgra(AVFrame* pFrame, ID3D11Texture2D** ppOut)
{
    auto* pSrcTex = (ID3D11Texture2D*)pFrame->data[0];
    intptr_t slice = (intptr_t)pFrame->data[1];
    if (!pSrcTex) {
        VDBG(L"[Video] Blt fail: pSrcTex=%p, dev=%p, vp=%p\n",
            pSrcTex, m_pVideoDevice, m_pVideoProcessor);
        return false;
    }

    int idx = -1;
    ID3D11Texture2D* pDstTex = AcquirePoolTexture(idx);
    if (!pDstTex) {
        VDBG(L"[Video] Blt fail: pSrcTex=%p, dev=%p, vp=%p\n",
            pSrcTex, m_pVideoDevice, m_pVideoProcessor);
        return false;
    }
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd{};
    ivd.FourCC = 0;
    ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    ivd.Texture2D.ArraySlice = (UINT)slice;

    eck::ComPtr<ID3D11VideoProcessorInputView> pIn;
    if (FAILED(m_pVideoDevice->CreateVideoProcessorInputView(pSrcTex, m_pVpEnum, &ivd, &pIn)))
    {
        ReleasePoolTextureByPtr(pDstTex);
        VDBG(L"[Video] Blt fail: pSrcTex=%p, dev=%p, vp=%p\n",
            pSrcTex, m_pVideoDevice, m_pVideoProcessor);
        return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovd{};
    ovd.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;

    eck::ComPtr<ID3D11VideoProcessorOutputView> pOut;
    if (FAILED(m_pVideoDevice->CreateVideoProcessorOutputView(pDstTex, m_pVpEnum, &ovd, &pOut)))
    {
        ReleasePoolTextureByPtr(pDstTex);
        VDBG(L"[Video] Blt fail: pSrcTex=%p, dev=%p, vp=%p\n",
            pSrcTex, m_pVideoDevice, m_pVideoProcessor);
        return false;
    }

    D3D11_VIDEO_PROCESSOR_STREAM st{};
    st.Enable = TRUE;
    st.pInputSurface = pIn.Get();

    {
        std::lock_guard<std::mutex> lk(m_mtxD3D);
        if (FAILED(m_pVideoContext->VideoProcessorBlt(m_pVideoProcessor, pOut.Get(), 0, 1, &st)))
        {
            ReleasePoolTextureByPtr(pDstTex);
            VDBG(L"[Video] Blt fail: pSrcTex=%p, dev=%p, vp=%p\n",
                pSrcTex, m_pVideoDevice, m_pVideoProcessor);
            return false;
        }
    }

    *ppOut = pDstTex;
    return true;
}

// ============================================================
// 解码一帧音频
// ============================================================
int CVeVideoPlayer::DecodeAudioFrame(AVFrame* pFrame)
{
    if (!m_pSwrCtx || !m_pRing) return 0;

    int outSamples = swr_get_out_samples(m_pSwrCtx, pFrame->nb_samples);
    if (outSamples <= 0) return 0;

    int outBytesPerSample = av_get_bytes_per_sample(m_eAudioSampleFmt);
    int outTotal = outSamples * m_iAudioChannels * outBytesPerSample;

    std::vector<uint8_t> outBuf(outTotal);
    uint8_t* pOut = outBuf.data();

    int converted = swr_convert(m_pSwrCtx, &pOut, outSamples,
        (const uint8_t**)pFrame->data, pFrame->nb_samples);
    if (converted <= 0) return 0;

    int bytes = converted * m_iAudioChannels * outBytesPerSample;
    return (int)m_pRing->Write(outBuf.data(), bytes);
}

// ============================================================
// 视频帧队列
// ============================================================
void CVeVideoPlayer::PushVideoFrame(VideoFrame&& f)
{
    std::unique_lock<std::mutex> lk(m_mtxVideoQ);
    // 队列满就等，让时钟追上；停止时直接丢
    while (m_queFrames.size() >= kMaxVideoQueue && !m_bStopRequest)
    {
        m_cvVideoQFull.wait_for(lk, std::chrono::milliseconds(20));
    }
    if (m_bStopRequest) return;
    m_queFrames.push_back(std::move(f));
}

bool CVeVideoPlayer::GetFrontFrame(VideoFrame& f)
{
    std::lock_guard<std::mutex> lk(m_mtxVideoQ);
    if (m_queFrames.empty()) return false;
    f = m_queFrames.front();
    return true;
}

void CVeVideoPlayer::PopFrontFrame()
{
    std::lock_guard<std::mutex> lk(m_mtxVideoQ);
    if (!m_queFrames.empty())
    {
        m_queFrames.pop_front();
        m_cvVideoQFull.notify_one();
    }
}

void CVeVideoPlayer::ClearVideoQueue()
{
    std::lock_guard<std::mutex> lk(m_mtxVideoQ);
    for (auto& f : m_queFrames)
        ReleasePoolTextureByPtr(f.pTexture);
    m_queFrames.clear();
    m_cvVideoQFull.notify_all();
}

// ============================================================
// 解码线程
// ============================================================
void CVeVideoPlayer::DecodeThreadProc()
{
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frm = av_frame_alloc();

    while (!m_bStopRequest)
    {
        double dbSeek = m_dbSeekRequest.exchange(-1.0);
        if (dbSeek >= 0.0)
        {
            if (m_pRing) m_pRing->Abort();

            int64_t tsV = (int64_t)(dbSeek / av_q2d(m_VideoTimeBase));
            av_seek_frame(m_pFmtCtx, m_iVideoStream, tsV, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(m_pVCodecCtx);

            if (m_pACodecCtx)
            {
                int64_t tsA = (int64_t)(dbSeek / av_q2d(m_AudioTimeBase));
                av_seek_frame(m_pFmtCtx, m_iAudioStream, tsA, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(m_pACodecCtx);
            }
            if (m_pRing) m_pRing->Reset();
            if (m_pSwrCtx) { swr_close(m_pSwrCtx); swr_init(m_pSwrCtx); }
            ClearVideoQueue();

            m_dbAudioClockBase = dbSeek;
            m_bAudioClockReset = true;
            m_dbAudioClock = dbSeek;
        }

        if (m_bPaused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int ret = av_read_frame(m_pFmtCtx, pkt);
        if (ret < 0)
        {
            if (m_bLoop)
            {
                Seek(0.0);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            bool bEmpty = false;
            {
                std::lock_guard<std::mutex> lk(m_mtxVideoQ);
                bEmpty = m_queFrames.empty();
            }
            if (bEmpty) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        if (pkt->stream_index == m_iVideoStream)
        {
            if (avcodec_send_packet(m_pVCodecCtx, pkt) == 0)
            {
                while (!m_bStopRequest && avcodec_receive_frame(m_pVCodecCtx, frm) == 0)
                {
                    if (frm->format == AV_PIX_FMT_D3D11)
                    {
                        ID3D11Texture2D* pBgra = nullptr;
                        bool ok = ConvertToBgra(frm, &pBgra);

                        static int _cnt = 0;
                        if (++_cnt % 30 == 1)   // 每 30 帧打一次，避免刷屏
                        {
                            VDBG(L"[Video] recv: fmt=%d pts=%lld tb=%d/%d ok=%d bgra=%p\n",
                                frm->format,
                                (long long)frm->pts,
                                m_VideoTimeBase.num, m_VideoTimeBase.den,
                                (int)ok, pBgra);
                        }

                        if (ok)
                        {
                            VideoFrame vf;
                            vf.pTexture = pBgra;
                            for (size_t i = 0; i < m_vecBgraPool.size(); ++i)
                                if (m_vecBgraPool[i] == pBgra) { vf.pBitmap = m_vecBmpPool[i]; break; }
                            vf.dbPts = frm->pts * av_q2d(m_VideoTimeBase);
                            PushVideoFrame(std::move(vf));
                        }
                        av_frame_unref(frm);
                    }
                    if (frm->format == AV_PIX_FMT_D3D11)
                    {
                        ID3D11Texture2D* pBgra = nullptr;
                        if (ConvertToBgra(frm, &pBgra))
                        {
                            VideoFrame vf;
                            vf.pTexture = pBgra;
                            for (size_t i = 0; i < m_vecBgraPool.size(); ++i)
                                if (m_vecBgraPool[i] == pBgra) { vf.pBitmap = m_vecBmpPool[i]; break; }
                            vf.dbPts = frm->pts * av_q2d(m_VideoTimeBase);
                            PushVideoFrame(std::move(vf));
                        }
                    }
                    av_frame_unref(frm);
                }
            }
        }
        else if (pkt->stream_index == m_iAudioStream && m_pACodecCtx && m_bAudioDeviceReady)
        {
            if (avcodec_send_packet(m_pACodecCtx, pkt) == 0)
            {
                while (!m_bStopRequest && avcodec_receive_frame(m_pACodecCtx, frm) == 0)
                {
                    DecodeAudioFrame(frm);
                    av_frame_unref(frm);
                }
            }
        }
        av_packet_unref(pkt);
    }

    av_frame_free(&frm);
    av_packet_free(&pkt);
}

// ============================================================
// WASAPI 音频线程
// ============================================================
void CVeVideoPlayer::AudioThreadProc()
{
    if (!m_pAudioClient) return;

    m_pAudioClient->Start();

    const UINT32 blockAlign = m_iAudioBlockAlign;

    while (m_bAudioRunning && !m_bStopRequest)
    {
        if (m_bAudioClockReset.exchange(false))
        {
            UINT64 pos = 0, qpc = 0;
            if (m_pAudioClock && SUCCEEDED(m_pAudioClock->GetPosition(&pos, &qpc)))
                m_ullAudioClockBase = pos;
            else
                m_ullAudioClockBase = 0;
        }

        UINT32 padding = 0;
        if (FAILED(m_pAudioClient->GetCurrentPadding(&padding))) break;

        UINT32 avail = m_uBufferFrames - padding;
        if (avail == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        BYTE* pData = nullptr;
        if (FAILED(m_pRenderClient->GetBuffer(avail, &pData)))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        size_t want = (size_t)avail * blockAlign;
        size_t got = m_pRing ? m_pRing->Read(pData, want) : 0;
        if (got < want)
            memset(pData + got, 0, want - got);

        m_pRenderClient->ReleaseBuffer(avail, 0);

        if (m_pAudioClock)
        {
            UINT64 pos = 0, qpc = 0;
            if (SUCCEEDED(m_pAudioClock->GetPosition(&pos, &qpc)))
            {
                UINT64 base = m_ullAudioClockBase.load();
                double baseSec = m_dbAudioClockBase.load();
                if (pos >= base)
                    m_dbAudioClock = baseSec + (double)(pos - base) / m_iAudioSampleRate;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    m_pAudioClient->Stop();
}

// ============================================================
// 内建渲染定时器（用元素自带的 SetTimer / KillTimer）
// ============================================================
void CVeVideoPlayer::StartRenderTimer()
{
    if (m_bTimerOn) return;

    m_uTimerId = reinterpret_cast<UINT_PTR>(this);   // 用 this 当唯一 ID
    if (SetTimer(m_uTimerId, m_uRenderIntervalMs))
    {
        m_bTimerOn = true;
        m_ullFallbackTick = GetTickCount64();
        m_dbFallbackAcc = m_dbAudioClock.load();
    }
}

void CVeVideoPlayer::StopRenderTimer()
{
    if (!m_bTimerOn) return;
    KillTimer(m_uTimerId);
    m_bTimerOn = false;
    m_uTimerId = 0;
}

D2D1_RECT_F CVeVideoPlayer::GetAspectFitRect() const
{
    auto rc = GetViewRectF();
    if (m_cxVideo <= 0 || m_cyVideo <= 0)
        return rc;

    const float w = rc.right - rc.left;
    const float h = rc.bottom - rc.top;
    if (w <= 0.f || h <= 0.f) return rc;

    const float videoAspect = (float)m_cxVideo / (float)m_cyVideo;
    const float elemAspect = w / h;

    float wNew, hNew;
    if (elemAspect > videoAspect)
    {
        // 元素比视频"更宽"，以高度为基准
        hNew = h;
        wNew = h * videoAspect;
    }
    else
    {
        // 元素比视频"更高"，以宽度为基准
        wNew = w;
        hNew = w / videoAspect;
    }

    const float x = rc.left + (w - wNew) * 0.5f;
    const float y = rc.top + (h - hNew) * 0.5f;
    return { x, y, x + wNew, y + hNew };
}

// ============================================================
// 播放控制
// ============================================================
void CVeVideoPlayer::Play()
{
    if (m_eState == State::Paused)
    {
        m_bPaused = false;
        m_ullFallbackTick = GetTickCount64();
        m_bAudioClockReset = true;
        if (m_pAudioClient) m_pAudioClient->Start();
        m_eState = State::Playing;
    }
    else if (m_eState == State::Stopped || m_eState == State::Idle)
    {
        if (!m_strUrl.empty()) Open(m_strUrl);
    }
}

void CVeVideoPlayer::Pause()
{
    if (m_eState == State::Playing)
    {
        m_bPaused = true;
        if (m_pAudioClient) m_pAudioClient->Stop();
        m_eState = State::Paused;
    }
}

void CVeVideoPlayer::Stop()
{
    m_bStopRequest = true;
    m_bAudioRunning = false;
    if (m_pRing) m_pRing->Abort();
    if (m_pAudioClient) m_pAudioClient->Stop();

    if (m_thDecode.joinable()) m_thDecode.join();
    if (m_thAudio.joinable())  m_thAudio.join();

    StopRenderTimer();

    ClearVideoQueue();
    if (m_lastFrame.pTexture)
    {
        ReleasePoolTextureByPtr(m_lastFrame.pTexture);
        m_lastFrame = {};
    }
    if (m_pRing) m_pRing->Reset();
    m_dbAudioClock = 0.0;
    if (m_eState != State::Idle) m_eState = State::Stopped;
}

void CVeVideoPlayer::Seek(double dbSeconds)
{
    m_dbSeekRequest = dbSeconds;
}

void CVeVideoPlayer::SetVolume(float v)
{
    if (v < 0.f) v = 0.f;
    if (v > 1.f) v = 1.f;
    m_fVolume = v;
    if (m_pVolume) m_pVolume->SetMasterVolume(v, nullptr);
}

// ============================================================
// 事件与渲染
// ============================================================
LRESULT CVeVideoPlayer::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        // 自动从框架反查设备
        AttachDeviceFromFramework();
        return 0;

    case WM_TIMER:
        if (wParam == reinterpret_cast<UINT_PTR>(this) && m_eState == State::Playing)
            InvalidateRect();
        return 0;

    case WM_PAINT:
    {
        Dui::ELEMPAINTSTRU ps;
        BeginPaint(ps, wParam, lParam);

        // 填自己的矩形（不要用 Clear）
        {
            ID2D1SolidColorBrush* pBlack = nullptr;
            m_pDC->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 1.f), &pBlack);
            if (pBlack)
            {
                m_pDC->FillRectangle(GetViewRectF(), pBlack);
                pBlack->Release();
            }
        }

        // 系统时钟推进（从 Open 时刻算起）
        if (m_eState == State::Playing && !m_bPaused)
        {
            ULONGLONG now = GetTickCount64();
            m_dbFallbackAcc += (now - m_ullFallbackTick) / 1000.0;
            m_ullFallbackTick = now;
            m_dbAudioClock = m_dbFallbackAcc;
        }

        double dbNow = m_dbAudioClock.load();

        // ---- 取帧 ----
        if (m_eState == State::Playing && !m_bPaused)
        {
            VideoFrame f;

            // 丢掉过期帧
            while (GetFrontFrame(f))
            {
                const double vRel = f.dbPts - m_dbStreamStartTime;
                if (vRel < dbNow - 0.100)
                {
                    PopFrontFrame();
                    ReleasePoolTextureByPtr(f.pTexture);
                    continue;
                }
                break;
            }
            // 采用已到达的帧
            if (GetFrontFrame(f))
            {
                const double vRel = f.dbPts - m_dbStreamStartTime;
                if (vRel <= dbNow + 0.020)
                {
                    if (m_lastFrame.pTexture)
                        ReleasePoolTextureByPtr(m_lastFrame.pTexture);
                    m_lastFrame = f;
                    PopFrontFrame();
                }
            }
        }

        // ---- 画帧 ----
        if (m_lastFrame.pBitmap)
        {
            D2D1_RECT_F rc = GetAspectFitRect();
            m_pDC->DrawBitmap(m_lastFrame.pBitmap, rc, 1.0f,
                D2D1_INTERPOLATION_MODE_LINEAR);
        }

        static int _pc = 0;
        if (++_pc % 10 == 1)
            VDBG(L"[Video] PAINT: clock=%.3f, start=%.3f, que=%zu, last=%p\n",
                dbNow, m_dbStreamStartTime, m_queFrames.size(), m_lastFrame.pBitmap);

        EndPaint(ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect();
        return 0;

    case WM_DESTROY:
        Stop();
        CloseInternal();
        // 注意：不在这里 DetachDevice，析构里才做，
        // 因为 WM_DESTROY 时窗口的资源还没全释放，安全些。
        return 0;
    }
    return CElem::OnEvent(uMsg, wParam, lParam);
}