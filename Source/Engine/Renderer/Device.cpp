#include "pch.h"
#include "Device.h"
#include "Engine/Log.h"
#include "Engine/Renderer/Context.h"
#include <wrl/client.h>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

GDevice* GDevice::GetInstance()
{
    static GDevice Instance{};
    return &Instance;
}

void GDevice::Initialize(HWND hWindow, uint32 InWidth, uint32 InHeight)
{
    GContext& Context = *GContext::GetInstance();
    if (Device.Get() || Context.IsInitialized())
    {
        throw std::logic_error("Graphics device is already initialized");
    }
    bRenderReady = false;
    bGraphicsFailed = false;

    try {
        if (!hWindow || InWidth == 0 || InHeight == 0)
        {
            throw std::invalid_argument("Invalid graphics initialization parameters");
        }

        if (!CreateDeviceAndSwapChain(hWindow, InWidth, InHeight))
        {
            throw std::runtime_error("Failed to create device and swap chain");
        }
        
        //프레임 버퍼와 뎁스스텐실 버퍼에 필요해서 조건문 나눔
        Context.Initialize(Device.Get());

        if (!CreateFrameBuffer() || !CreateDepthStencilBuffer(InWidth, InHeight))
        {
            throw std::runtime_error( "Failed to create render target resources");
        }

        bRenderReady = true;
    }
    catch(...){
        Release();
        throw;
    }
    
}

bool GDevice::IsRenderReady() const
{
    return bRenderReady && !bGraphicsFailed;
}

ID3D11Device* GDevice::GetDevice() const
{
    return Device.Get();
}

ID3D11RenderTargetView* GDevice::GetFrameBufferRTV() const
{
    return FrameBufferRTV.Get();
}

ID3D11DepthStencilView* GDevice::GetDepthStencilView() const
{
    return DepthStencilView.Get();
}

const D3D11_VIEWPORT& GDevice::GetViewport() const
{
    return ViewportInfo;
}

void GDevice::Release()
{
    bRenderReady = false;
    ComPtr<ID3D11Debug> Debug;
    if (Device.Get())
    {
        // Debug 인터페이스가 없는 경우에도 종료.
        Device.As(&Debug);
    }
    
    GContext& Context = *GContext::GetInstance();

    Context.ClearState();

    ReleaseFrameBuffer();
    ReleaseDepthStencilBuffer();

    SwapChain.Reset();

    Context.Release();
    Device.Reset();

    if (Debug.Get())
    {
        Debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
    }
}

void GDevice::OnResize(uint32 Width, uint32 Height)
{
    bRenderReady = false;
    GContext& Context = *GContext::GetInstance();

    if (Width == 0 || Height == 0 || !Device.Get() || !Context.IsInitialized() ||
        !SwapChain.Get() || bGraphicsFailed) return;
    
    // 컨텍스트 렌더타겟 언바인딩
    Context.UnbindRenderTargets();
    ReleaseDepthStencilBuffer();
    ReleaseFrameBuffer();
    Context.Flush();

    const HRESULT Result = SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(Result))
    {
        bGraphicsFailed = true;
        PostQuitMessage(EXIT_FAILURE);
        return;
    }
    ViewportInfo = {0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f};
    if (!CreateFrameBuffer() || !CreateDepthStencilBuffer(Width, Height))
    {
        ReleaseFrameBuffer();
        ReleaseDepthStencilBuffer();
        bGraphicsFailed = true;
        PostQuitMessage(EXIT_FAILURE);
        return;
    }
    Context.SetRenderTargets(FrameBufferRTV.Get(), DepthStencilView.Get());
    Context.SetViewport(ViewportInfo);
    bRenderReady = true;
}

bool GDevice::CreateDeviceAndSwapChain(HWND hWindow, uint32 Width, uint32 Height)
{
    // 지원하는 Direct3D 기능 레벨을 정의
    const D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

    // 스왑 체인 설정 구조체 초기화
    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width = Width;
    swapchaindesc.BufferDesc.Height = Height;
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;   // 색상 포맷
    swapchaindesc.SampleDesc.Count = 1;                             // 멀티 샘플링 비활성화
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;    // 렌더 타겟으로 사용
    swapchaindesc.BufferCount = 2;                                  // 더블 버퍼링
    swapchaindesc.OutputWindow = hWindow;                           // 렌더링할 창 핸들
    swapchaindesc.Windowed = TRUE;                                  // 창 모드
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;       // 스왑 방식

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG) || defined(DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    ComPtr<ID3D11Device> NewDevice;
    ComPtr<IDXGISwapChain> NewSwapChain;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,featurelevels,
        ARRAYSIZE(featurelevels), D3D11_SDK_VERSION, &swapchaindesc, NewSwapChain.GetAddressOf(), 
        NewDevice.GetAddressOf(), nullptr, nullptr);

    if (FAILED(hr))
    {
        UE_LOG("[GDevice] Failed to create device and swap chain.");
        return false;
    }

    if (FAILED(NewSwapChain->GetDesc(&swapchaindesc))){ return false; }

    // 필요한 작업이 성공한 뒤 멤버로 소유권을 이동한다.
    Device = std::move(NewDevice);
    SwapChain = std::move(NewSwapChain);

    ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };

    return true;
}


bool GDevice::CreateFrameBuffer()
{
    if (!Device.Get() || !SwapChain.Get()) { return false; }
    ComPtr<ID3D11Texture2D> NewFrameBuffer;
    ComPtr<ID3D11RenderTargetView> NewRTV;
    // ComPtr을 쓰면서 IID_PPV_ARGS로 변경->인수가 하나 준것처럼 보임. 실제로는 여전히 인수 3개
    HRESULT hr = SwapChain->GetBuffer(0, IID_PPV_ARGS(NewFrameBuffer.GetAddressOf()));
    if (FAILED(hr))
    {
        UE_LOG("[GDevice] Failed to get back buffer from SwapChain. HRESULT: {}\n", hr);
        ReleaseFrameBuffer();
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;        // 색상 포맷
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;   // 2D 텍스처
    hr = Device->CreateRenderTargetView(NewFrameBuffer.Get(), &framebufferRTVdesc, NewRTV.GetAddressOf());
    if (FAILED(hr))
    {
        UE_LOG("[GDevice] Failed to create Render Target View. HRESULT: {}\n", hr);
        //지역 ComPtr이라 따로 정리해주지않아도 됨.
        return false;
    }
    FrameBuffer = std::move(NewFrameBuffer);
    FrameBufferRTV = std::move(NewRTV);
    return true;
}

void GDevice::ReleaseFrameBuffer()
{
    FrameBufferRTV.Reset();
    FrameBuffer.Reset();
}

bool GDevice::CreateDepthStencilBuffer(uint32 InWidth, uint32 InHeight)
{
    if (!Device.Get() || InWidth == 0 || InHeight == 0) { return false; }
    HRESULT hr;

    ComPtr<ID3D11Texture2D> NewDepthBuffer;
    ComPtr<ID3D11DepthStencilView> NewDSV;

    D3D11_TEXTURE2D_DESC DepthStencilDesc = {};
    DepthStencilDesc.Width = InWidth;
    DepthStencilDesc.Height = InHeight;
    DepthStencilDesc.MipLevels = 1;
    DepthStencilDesc.ArraySize = 1;
    DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;    // 24비트 깊이(Depth) + 8비트 스텐실(Stencil)
    DepthStencilDesc.SampleDesc.Count = 1;                      // MSAA - 스왑체인 생성 시의 SampleDesc
    DepthStencilDesc.SampleDesc.Quality = 0;
    DepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    DepthStencilDesc.CPUAccessFlags = 0;
    DepthStencilDesc.MiscFlags = 0;

    hr = Device->CreateTexture2D(&DepthStencilDesc, nullptr, NewDepthBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
    DSVDesc.Format = DepthStencilDesc.Format;
    DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DSVDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(NewDepthBuffer.Get(), &DSVDesc, NewDSV.GetAddressOf());
    if (FAILED(hr))
    {
        UE_LOG("[GDevice] Failed to create depth stencil view.\n");
        return false;
    }
    DepthStencilBuffer = std::move(NewDepthBuffer);
    DepthStencilView = std::move(NewDSV);

    return true;
}

void GDevice::ReleaseDepthStencilBuffer()
{
    DepthStencilView.Reset();
    DepthStencilBuffer.Reset();
}

//const void를 사용함으로써 FVertex 종류가 달라져도 호환가능하게됨.
//구분은 렌더러의 stride,InputLayout으로 가능
//해당 버퍼들의 보유는 호출한 메쉬 리소스가 가져갑니다.
ComPtr<ID3D11Buffer> GDevice::CreateVertexBuffer(const void* VertexData, UINT ByteWidth)
{
    if (!Device.Get() || !VertexData || ByteWidth == 0)
    {
        return {};
    }
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = ByteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD{};
    vertexbufferSRD.pSysMem = VertexData;

    ComPtr<ID3D11Buffer> vertexBuffer;

    HRESULT hr = Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    if (FAILED(hr))
    {
        UE_LOG("[GDevice] Failed to create Vertex Buffer. HRESULT: {}\n", hr);
        return {};
    }
    return vertexBuffer;
};

ComPtr<ID3D11Buffer> GDevice::CreateIndexBuffer(const uint32* indices, UINT byteWidth)
{
    if (!Device.Get() || !indices || byteWidth == 0)
    {
        return {};
    }
    ComPtr<ID3D11Buffer> indexBuffer = {};

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = byteWidth;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = indices;

    HRESULT hr = Device->CreateBuffer(&bd, &initData, &indexBuffer);
    if (FAILED(hr))
    {
        UE_LOG("[GDevice] Failed to create Index Buffer. HRESULT: {}\n", hr);
        return {};
    }

    return indexBuffer;
}


void GDevice::SwapBuffer()
{
    if (!IsRenderReady() || !SwapChain.Get()) return;
    const HRESULT Result = SwapChain->Present(0, 0);
    if (FAILED(Result))
    {
        bRenderReady = false;
        bGraphicsFailed = true;
        PostQuitMessage(EXIT_FAILURE);
    }
}
