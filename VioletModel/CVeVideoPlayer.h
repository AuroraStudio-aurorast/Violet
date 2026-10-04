#pragma once
#include "CApp.h"
#include "Utils.h"

// ==================== FFmpeg 头文件 ====================
extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/buffer.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

// ==================== D3D / D2D / WASAPI ====================
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d10_1.h> 
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dxgi1_2.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <wrl/client.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <string>
#include <chrono>

// ==================== 音频环形缓冲 ====================
class CVeAudioRing
{
    std::vector<uint8_t>    m_buf;
    size_t                  m_rp{ 0 }, m_wp{ 0 };
    std::mutex              m_mtx;
    std::condition_variable m_cvWrite;
    std::atomic<bool>       m_abort{ false };

public:
    explicit CVeAudioRing(size_t n) : m_buf(n) {}

    void Abort()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_abort = true;
        m_cvWrite.notify_all();
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rp = m_wp = 0;
        m_abort = false;
        m_cvWrite.notify_all();
    }

    // 音频线程：读数据，非阻塞
    size_t Read(uint8_t* dst, size_t n)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        size_t used = (m_wp - m_rp + m_buf.size()) % m_buf.size();
        size_t toRead = (std::min)(n, used);
        for (size_t i = 0; i < toRead; ++i)
        {
            dst[i] = m_buf[m_rp];
            m_rp = (m_rp + 1) % m_buf.size();
        }
        if (toRead) m_cvWrite.notify_one();
        return toRead;
    }

    // 解码线程：写数据，最多等待 50ms 后返回（让解码线程有机会响应 seek）
    size_t Write(const uint8_t* src, size_t n)
    {
        std::unique_lock<std::mutex> lk(m_mtx);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
        size_t total = 0;
        while (total < n && !m_abort)
        {
            size_t used = (m_wp - m_rp + m_buf.size()) % m_buf.size();
            size_t freeSpc = m_buf.size() - 1 - used;
            if (freeSpc == 0)
            {
                if (m_cvWrite.wait_until(lk, deadline) == std::cv_status::timeout)
                    break;
                continue;
            }
            size_t chunk = (std::min)(n - total, freeSpc);
            for (size_t i = 0; i < chunk; ++i)
            {
                m_buf[m_wp] = src[total + i];
                m_wp = (m_wp + 1) % m_buf.size();
            }
            total += chunk;
        }
        return total;
    }

    size_t Used()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return (m_wp - m_rp + m_buf.size()) % m_buf.size();
    }
};

// ==================== 播放器组件 ====================
class CVeVideoPlayer : public Dui::CElem
{
public:
    enum class State { Idle, Opening, Playing, Paused, Stopped };

private:
    // ---------- D3D/D2D 设备（组件持有强引用） ----------
    ID3D11Device* m_pD3DDevice{};
    ID3D11DeviceContext* m_pD3DContext{};
    ID2D1DeviceContext* m_pD2DContext{};
    bool                 m_bDeviceAttached{ false };

    // ---------- FFmpeg 视频 ----------
    AVFormatContext* m_pFmtCtx{};
    AVCodecContext* m_pVCodecCtx{};
    AVCodecContext* m_pACodecCtx{};
    AVBufferRef* m_pHwDeviceCtx{};
    int              m_iVideoStream{ -1 };
    int              m_iAudioStream{ -1 };
    double           m_dbDuration{};
    AVRational       m_VideoTimeBase{};
    AVRational       m_AudioTimeBase{};
    double           m_dbFps{ 30.0 };

    // ---------- 音频重采样（-> WASAPI 混音格式） ----------
    SwrContext* m_pSwrCtx{};
    int             m_iAudioSampleRate{ 48000 };
    int             m_iAudioChannels{ 2 };
    AVSampleFormat  m_eAudioSampleFmt{ AV_SAMPLE_FMT_FLT };
    int             m_iAudioBlockAlign{};
    int             m_iAudioBytesPerSec{};
    CVeAudioRing* m_pRing{};

    // ---------- WASAPI ----------
    IMMDeviceEnumerator* m_pEnum{};
    IAudioClient* m_pAudioClient{};
    IAudioRenderClient* m_pRenderClient{};
    IAudioClock* m_pAudioClock{};
    ISimpleAudioVolume* m_pVolume{};
    UINT32               m_uBufferFrames{};
    std::thread          m_thAudio;
    std::atomic<bool>    m_bAudioRunning{ false };
    std::atomic<double>  m_dbAudioClock{ 0.0 };
    std::atomic<float>   m_fVolume{ 1.0f };
    std::atomic<bool>    m_bAudioDeviceReady{ false };

    std::atomic<UINT64>  m_ullAudioClockBase{ 0 };
    std::atomic<double>  m_dbAudioClockBase{ 0.0 };
    std::atomic<bool>    m_bAudioClockReset{ false };

    // ---------- 解码线程 ----------
    std::thread          m_thDecode;
    std::atomic<bool>    m_bStopRequest{ false };
    std::atomic<bool>    m_bPaused{ false };
    std::atomic<double>  m_dbSeekRequest{ -1.0 };

    // ---------- 视频帧队列 ----------
    struct VideoFrame
    {
        ID3D11Texture2D* pTexture{};
        ID2D1Bitmap1* pBitmap{};
        double           dbPts{};
    };
    std::deque<VideoFrame>  m_queFrames;
    std::mutex              m_mtxVideoQ;
    std::condition_variable m_cvVideoQFull;
    static constexpr size_t kMaxVideoQueue = 8;
    VideoFrame              m_lastFrame;

    // ---------- D3D11 视频处理器（NV12 -> BGRA） ----------
    ID3D11VideoDevice* m_pVideoDevice{};
    ID3D11VideoContext* m_pVideoContext{};
    ID3D11VideoProcessorEnumerator* m_pVpEnum{};
    ID3D11VideoProcessor* m_pVideoProcessor{};
    int                             m_cxVideo{}, m_cyVideo{};

    // ---------- BGRA 纹理池 ----------
    std::vector<ID3D11Texture2D*> m_vecBgraPool;
    std::vector<ID2D1Bitmap1*>    m_vecBmpPool;
    std::vector<bool>             m_vecPoolBusy;
    std::mutex                    m_mtxPool;

    // ---------- D3D11 immediate context 互斥 ----------
    std::mutex m_mtxD3D;

    // ---------- 状态 ----------
    State        m_eState{ State::Idle };
    bool         m_bLoop{ false };
    std::wstring m_strUrl;

    // ---------- 内建渲染定时器（用元素自带的 SetTimer） ----------
    UINT_PTR  m_uTimerId{};
    bool      m_bTimerOn{ false };
    UINT      m_uRenderIntervalMs{ 16 };

    // ---------- 无音频设备时的回退时钟 ----------
    ULONGLONG m_ullFallbackTick{};
    double    m_dbFallbackAcc{};

    // ---------- 视频流的起始时间（秒）----------
    double m_dbStreamStartTime{ 0.0 };

    // ---------- 纹理池（独立于 Open/Close 生命周期）----------
    int m_cxPoolVideo{};    // 池当前对应的视频宽
    int m_cyPoolVideo{};    // 池当前对应的视频高

    void EnsureTexturePool(int cx, int cy);   // 尺寸变化时重建
    void DestroyTexturePool();                // 析构时释放

    // ==================== 内部方法 ====================
    bool   AttachDeviceFromFramework();     // 从框架全局反查并附加设备
    void   DetachDevice();

    bool   OpenInternal(const std::wstring& url);
    void   CloseInternal();

    bool   InitHwDecoder();
    bool   InitVideoProcessor(int cx, int cy);
    bool   InitAudioOutput();
    bool   InitAudioResampler();

    void   DecodeThreadProc();
    void   AudioThreadProc();

    int    DecodeAudioFrame(AVFrame* pFrame);
    bool   ConvertToBgra(AVFrame* pFrame, ID3D11Texture2D** ppOut);

    ID3D11Texture2D* AcquirePoolTexture(int& idx);
    void   ReleasePoolTextureByPtr(ID3D11Texture2D* pTex);

    void   PushVideoFrame(VideoFrame&& f);
    bool   GetFrontFrame(VideoFrame& f);
    void   PopFrontFrame();
    void   ClearVideoQueue();

    void   StartRenderTimer();
    void   StopRenderTimer();

    D2D1_RECT_F GetAspectFitRect() const;

    double GetAudioClock() const { return m_dbAudioClock.load(); }

public:
    CVeVideoPlayer();
    ~CVeVideoPlayer();

    // 允许外部注入自定义设备（可选，不调则 WM_CREATE 自动从框架取）
    void AttachDevice(ID3D11Device* pDev,
        ID3D11DeviceContext* pCtx,
        ID2D1DeviceContext* pD2D);

    // 播放控制
    bool  Open(const std::wstring& url);
    void  Play();
    void  Pause();
    void  Stop();
    void  Seek(double dbSeconds);
    void  SetLoop(bool b) { m_bLoop = b; }
    void  SetVolume(float v);
    float GetVolume() const { return m_fVolume.load(); }

    State  GetState()    const { return m_eState; }
    double GetDuration() const { return m_dbDuration; }
    double GetPosition() const { return GetAudioClock(); }

    LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};