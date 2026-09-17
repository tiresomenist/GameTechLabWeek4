#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include "Core/Core.h"

struct FVertexSimple;
struct FVertexTest;

// Device 생성 → Renderer/ResourceManager 생성 → (역순으로) Renderer/ResourceManager 소멸 → Device 소멸

class GDevice
{
public:
    static GDevice* GetInstance();

    void Initialize(HWND hWnd, uint32 Width, uint32 Height);
    void Release();
    void OnResize(uint32 Width, uint32 Height);


    Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(const void* VertexData, UINT ByteWidth);

    Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(const uint32* Indices, UINT ByteWidth);

    void SwapBuffer();

    bool IsRenderReady() const;

    // 모두 빌린 참조를 반환한다.
    ID3D11Device* GetDevice() const;
    ID3D11RenderTargetView* GetFrameBufferRTV() const;
    ID3D11DepthStencilView* GetDepthStencilView() const;

    const D3D11_VIEWPORT& GetViewport() const;

private:
    bool CreateDeviceAndSwapChain(HWND Window, uint32 Width, uint32 Height);

    bool CreateFrameBuffer();
    void ReleaseFrameBuffer();

    bool CreateDepthStencilBuffer(uint32 InWidth, uint32 InHeight);
    void ReleaseDepthStencilBuffer();

    Microsoft::WRL::ComPtr<ID3D11Device> Device = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> FrameBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> FrameBufferRTV = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView = nullptr;
    D3D11_VIEWPORT ViewportInfo{};
    bool bRenderReady = false;
    bool bGraphicsFailed = false;
    GDevice() = default;
    ~GDevice() = default;
    GDevice(const GDevice&) = delete;
    GDevice& operator=(const GDevice&) = delete;
};
