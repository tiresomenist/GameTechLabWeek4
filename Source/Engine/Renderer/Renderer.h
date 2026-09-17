#pragma once
#include <vector>

// D3D11 headers
#include <d3d11.h>

//#include "UEngine"
#include "Engine/Renderer/PrimitiveRenderData.h"
#include "Device.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Quaternion.h"
//#include "../FVertexSimple.h"
#include "Engine/Resource/MeshResource.h"
#include "Core/Container/Array.h"
#include "Engine/Renderer/Text/FontAtlas.h"
#include "Engine/Renderer/Text/TextMeshBuilder.h"
#include "Engine/Renderer/ViewSettings.h"
#include "Engine/Renderer/Line/LineBatcher.h"
#include <wrl/client.h>

//struct FVertexSimple;
struct FConstants
{
	FMatrix MVP;
};
struct FGridConstants
{
	FVector CameraPos;
	int GridPlaneType;
};
class UScene;
class FEditor;
struct FPrimitiveRenderData;
class UCameraComponent;
struct FRenderView
{
	UCameraComponent* Camera = nullptr;
	D3D11_VIEWPORT Viewport{};
	FViewSettings ViewSettings{};
	bool bDrawEditorGizmos = false;
};

#include <cmath>


class FRenderer
{
public:
	GDevice* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3D11Device* D3DDevice = nullptr;
	FLineBatcher LineBatcher;

	// ResourceManager 소유 자원의 비소유 참조
	ID3D11RasterizerState* DefaultRasterizerState = nullptr;
	ID3D11RasterizerState* CullFrontRasterizerState = nullptr;
	ID3D11RasterizerState* CullNoneRasterizerState = nullptr;
	ID3D11RasterizerState* WireframeRasterizerState = nullptr;

	ID3D11DepthStencilState* DefaultDepthStencilState = nullptr;
	ID3D11DepthStencilState* GizmoDepthStencilState = nullptr;
	ID3D11DepthStencilState* HighlightDepthStencilState = nullptr;
	ID3D11DepthStencilState* StencilWriteDepthStencilState = nullptr;	// 선택 오브젝트 본체: 스텐실에 1 기록
	ID3D11DepthStencilState* OutlineDepthStencilState = nullptr;		// 외곽선: 스텐실 != 1 인 곳만 통과
	
	// ResourceManager 소유 자원의 비소유 참조
	ID3D11BlendState* AlphaBlendState = nullptr;

	ID3D11Buffer* TransformConstantBuffer = nullptr;
	ID3D11Buffer* GridConstantBuffer = nullptr;

	FLOAT                   ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	// ResourceManager 소유 자원의 비소유 참조
	ID3D11VertexShader* SimpleVertexShader = nullptr;
	ID3D11PixelShader* SimplePixelShader = nullptr;
	ID3D11InputLayout* SimpleInputLayout = nullptr;

	ID3D11PixelShader* WireframePixelShader = nullptr;
	const FShaderResource* HighlightShader = nullptr;
	const FShaderResource* GridShader = nullptr;
	const FShaderResource* BatchLineShader = nullptr;

	// ---- Text Billboard ----
	const FShaderResource* TextShader = nullptr;
	ID3D11SamplerState* FontSamplerState = nullptr;
	ID3D11DepthStencilState* TextDepthStencilState = nullptr;

	ID3D11Buffer* TextVertexBuffer = nullptr;
	ID3D11Buffer* TextIndexBuffer = nullptr;
	static const UINT MaxTextVertices = 8192;

	// ---- Texture ----
	ID3D11DepthStencilState* TranslucentDepthStencilState = nullptr;

	// ---- 블렌드 스테이트 모드 ----
	// ResourceManager 소유 자원의 비소유 참조
	ID3D11BlendState* AdditiveBlendState = nullptr;


	bool bImGuiContextCreated = false;
	bool bImGuiWin32Initialized = false;
	bool bImGuiDX11Initialized = false;
	void Create(HWND HWnd, GDevice* InDevice, uint32 Width, uint32 Height);
	void Shutdown();
	void OnResize(uint32 Width, uint32 Height);
	bool IsRenderReady() const;
	const D3D11_VIEWPORT& GetViewport() const;

	bool CreateShaders();
	void PrepareRTVDSV();
	void ReleaseShaders();

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);

	void CreateConstantBuffer();
	void UpdateTransformConstantBuffer(const FMatrix& WorldMatrix);
	void UpdateGridConstantBuffer(const FGridConstants& GridConstants);
	void ReleaseConstantBuffer();

	void CreateRasterizerState();
	void ReleaseRasterizerState();

	void CreateAlphaBlendState();
	void ReleaseAlphaBlendState();

	void CreateDepthStencilStates();
	void ReleaseDepthStencilStates();

	void BeginFrame();
	void EndFrame();

	void Render(float DeltaTime, FEditor* Editor, UScene* Scene);
	void Render(float DeltaTime, FEditor* Editor, UScene* Scene, const TArray<FRenderView>& Views);
	void RenderPrimitive(const FPrimitiveRenderData& Data, EViewModeIndex InViewMode, bool bWriteStencil = false);
	void RenderHighlight(const FPrimitiveRenderData& Data);
	void RenderOutline(const FPrimitiveRenderData& Data);
	void RenderGrid(FMeshResource* Data);
	void RenderGizmo(const FPrimitiveRenderData& Data);
	void RenderBatchLine(const FMatrix& ViewProj);

	void CreateTextResources();
	void ReleaseTextResources();
	void UpdateTextVertexBuffer(TArray<FVertexTexture>& Vertices);
	void RenderText(UINT IndexCount);

private:
	void RenderView(FEditor* Editor, UScene* Scene, const FRenderView& View);
	bool CreateSwapChain(HWND HWnd, uint32 Width, uint32 Height);

	bool CreateFrameBuffer();
	void ReleaseFrameBuffer();

	bool CreateDepthStencilBuffer(uint32 Width, uint32 Height);
	void ReleaseDepthStencilBuffer();

	void SetViewportAndScissor(const D3D11_VIEWPORT& Viewport);

	void SwapBuffer();
	bool BindMaterial(const FMaterial& Material);
	void UpdateMaterialConstants(const FPrimitiveRenderData& Data);

	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> FrameBuffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> FrameBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;

	D3D11_VIEWPORT ViewportInfo{};
	bool bRenderReady = false;
	bool bGraphicsFailed = false;

};
