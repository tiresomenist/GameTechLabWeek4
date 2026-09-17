#include "pch.h"
#pragma once
#include "Renderer.h"
#include "Core/Math/Matrix.h"
#include "Engine/Renderer/VertexSimple.h"
#include "Editor/Window/EditorWindow.h"
#include "Engine/Renderer/Device.h"
#include "Engine/Scene/Scene.h"
#include "Editor/Editor.h"
#include "Editor/Grid.h"
#include "Editor/Gizmo/Gizmo.h"
#include "Engine/Renderer/RenderUtil.h"
#include "Core/Core.h"
#include "Engine/Component/CameraComponent.h"
#include "Engine/Resource/MeshResource.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Log.h"
#include "Engine/Renderer/Context.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <format>
#include <filesystem>
#include <wrl/client.h>
#include <stdexcept>
#include <vector>
#include <utility>
#include <cstdlib>

using Microsoft::WRL::ComPtr;
namespace
{
	void CheckHR(HRESULT Result)
	{
		if (FAILED(Result))
			throw std::runtime_error(std::format("D3D resource creation failed: {}", Result));
	}
}


void FRenderer::Create(HWND HWnd, GDevice* InDevice, uint32 Width, uint32 Height)
{
	GContext& Context = *GContext::GetInstance();
	if (Device) {
		throw std::logic_error("Renderer is Already initialized");
	}
	if (!HWnd || Width == 0 || Height == 0)
	{
		throw std::invalid_argument("Invalid renderer output parameters");
	}

	if (!InDevice || !InDevice->GetDevice() || !Context.IsInitialized()) {
		throw std::runtime_error("Renderer requires an initialized device");
	}
	
	bRenderReady = false;
	bGraphicsFailed = false;

	Device = InDevice;
	DeviceContext = Context.GetNative();
	D3DDevice = InDevice->GetDevice();
	if (!CreateSwapChain(HWnd, Width, Height) || !CreateFrameBuffer() || 
		!CreateDepthStencilBuffer(static_cast<uint32>(ViewportInfo.Width),static_cast<uint32>(ViewportInfo.Height)))
	{
		throw std::runtime_error("Failed to create renderer output");
	}

	CreateRasterizerState();
	if (!CreateShaders()) throw std::runtime_error("Shader compilation failed");
	CreateConstantBuffer();
	CreateAlphaBlendState();
	CreateDepthStencilStates();
	CreateTextResources();
	CreateTextureResources();
	IMGUI_CHECKVERSION();
	if (!ImGui::CreateContext()) throw std::runtime_error("ImGui context failed");
	bImGuiContextCreated = true;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->AddFontFromFileTTF("Assets/Fonts/Pretendard-Regular.ttf", 16.0f);

	// Setup Platform/Renderer backends
	bImGuiWin32Initialized = ImGui_ImplWin32_Init(HWnd);
	if (!bImGuiWin32Initialized) throw std::runtime_error("ImGui Win32 initialization failed");
	bImGuiDX11Initialized = ImGui_ImplDX11_Init(D3DDevice, DeviceContext);
	if (!bImGuiDX11Initialized) throw std::runtime_error("ImGui DX11 initialization failed");
	if (!ImGui_ImplDX11_CreateDeviceObjects())
		throw std::runtime_error("ImGui GPU resource creation failed");
	bRenderReady = true;
}

void FRenderer::Shutdown()
{
	bRenderReady = false;

	LineBatcher.Release();

	if (DeviceContext){	DeviceContext->ClearState();}

	ReleaseConstantBuffer();
	ReleaseShaders();
	ReleaseRasterizerState();
	ReleaseAlphaBlendState();
	ReleaseDepthStencilStates();
	ReleaseTextResources();
	ReleaseTextureResources();

	if (bImGuiDX11Initialized){ImGui_ImplDX11_Shutdown();}

	if (bImGuiWin32Initialized){ImGui_ImplWin32_Shutdown();}

	if (bImGuiContextCreated){ImGui::DestroyContext();}

	bImGuiDX11Initialized = false;
	bImGuiWin32Initialized = false;
	bImGuiContextCreated = false;

	ReleaseDepthStencilBuffer();
	ReleaseFrameBuffer();
	SwapChain.Reset();
	ViewportInfo = {};

	DeviceContext = nullptr;
	D3DDevice = nullptr;
	Device = nullptr;
}

bool FRenderer::IsRenderReady() const
{
	return bRenderReady && !bGraphicsFailed;
}

const D3D11_VIEWPORT& FRenderer::GetViewport() const
{
	return ViewportInfo;
}

bool FRenderer::CreateShaders()
{
	GResourceManager& Resources = *GResourceManager::GetInstance();

	const FShaderResource* ColorShader = Resources.GetShader(FName("Mesh.Color"));

	if (!ColorShader || !ColorShader->VertexShader || !ColorShader->PixelShader || !ColorShader->InputLayout)
	{
		return false;
	}

	// ResourceManager가 소유한 자원을 참조한다.
	SimpleVertexShader = ColorShader->VertexShader.Get();
	SimplePixelShader = ColorShader->PixelShader.Get();
	SimpleInputLayout = ColorShader->InputLayout.Get();

	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;

	// Wireframe 셰이더는 이번 이전 대상에 포함하지 않는다.
	if (!CompileShader(L"Assets/Shaders/WireframeShader.hlsl","mainPS","ps_5_0",shaderBlob.ReleaseAndGetAddressOf()))
	{
		return false;
	}

	CheckHR(D3DDevice->CreatePixelShader(shaderBlob->GetBufferPointer(),shaderBlob->GetBufferSize(),nullptr,	&WireframePixelShader));

	shaderBlob.Reset();

	// Highlight Shader (VS & PS)
	if (!CompileShader(L"Assets/Shaders/MainShader.hlsl", "VS_Highlight", "vs_5_0", shaderBlob.ReleaseAndGetAddressOf())) return false;
	CheckHR(D3DDevice->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &HighlightVertexShader));
	shaderBlob.Reset();

	if (!CompileShader(L"Assets/Shaders/MainShader.hlsl", "PS_Highlight", "ps_5_0", shaderBlob.ReleaseAndGetAddressOf())) return false;
	CheckHR(D3DDevice->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &HighlightPixelShader));
	shaderBlob.Reset();

	// Grid Shader (VS & PS)
	if (!CompileShader(L"Assets/Shaders/GridShader.hlsl", "VS_Grid", "vs_5_0", shaderBlob.ReleaseAndGetAddressOf())) return false;
	CheckHR(D3DDevice->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &GridVertexShader));
	shaderBlob.Reset();

	if (!CompileShader(L"Assets/Shaders/GridShader.hlsl", "PS_Grid", "ps_5_0", shaderBlob.ReleaseAndGetAddressOf())) return false;
	CheckHR(D3DDevice->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &GridPixelShader));
	shaderBlob.Reset();

	if (!CompileShader(L"Assets/Shaders/BatchLineShader.hlsl", "mainVS", "vs_5_0", shaderBlob.ReleaseAndGetAddressOf())) return false;
	CheckHR(D3DDevice->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &BatchLineVertexShader));
	shaderBlob.Reset();

	if (!CompileShader(L"Assets/Shaders/BatchLineShader.hlsl", "mainPS", "ps_5_0", shaderBlob.ReleaseAndGetAddressOf())) return false;
	CheckHR(D3DDevice->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &BatchLinePixelShader));
	shaderBlob.Reset();



	return true;
}
bool FRenderer::CompileShader(const WCHAR* FilePath, const LPCSTR EntryPoint, const LPCSTR ShaderModel, ID3DBlob** OutBlob)
{
	if (!OutBlob) return false;
	*OutBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	const HRESULT Hr = D3DCompileFromFile(FilePath, nullptr, nullptr, EntryPoint, ShaderModel,
		0, 0, OutBlob, ErrorBlob.GetAddressOf());
	if (ErrorBlob)
		UE_LOG("Shader diagnostic: {}", static_cast<const char*>(ErrorBlob->GetBufferPointer()));
	return SUCCEEDED(Hr);
}

void FRenderer::ReleaseShaders()
{
	SimpleInputLayout = nullptr;
	SimplePixelShader = nullptr;
	SimpleVertexShader = nullptr;

	if (WireframePixelShader)
	{
		WireframePixelShader->Release();
		WireframePixelShader = nullptr;
	}
	if (HighlightVertexShader)
	{
		HighlightVertexShader->Release();
		HighlightVertexShader = nullptr;
	}
	if (HighlightPixelShader)
	{
		HighlightPixelShader->Release();
		HighlightPixelShader = nullptr;
	}
	if (GridVertexShader)
	{
		GridVertexShader->Release();
		GridVertexShader = nullptr;
	}
	if (GridPixelShader)
	{
		GridPixelShader->Release();
		GridPixelShader = nullptr;
	}
	if (BatchLineVertexShader)
	{
		BatchLineVertexShader->Release();
		BatchLineVertexShader = nullptr;
	}
	if (BatchLinePixelShader)
	{
		BatchLinePixelShader->Release();
		BatchLinePixelShader = nullptr;
	}
}
void FRenderer::SwapBuffer()
{
	if (!IsRenderReady() || !SwapChain.Get()) { return; }

	const HRESULT Result = SwapChain->Present(0, 0);

	if (FAILED(Result))
	{
		bRenderReady = false;
		bGraphicsFailed = true;
		PostQuitMessage(EXIT_FAILURE);
	}
}

void FRenderer::PrepareRTVDSV()
{
	GContext& Context = *GContext::GetInstance();

	SetViewportAndScissor(ViewportInfo);

	DeviceContext->RSSetState(DefaultRasterizerState);
	DeviceContext->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);
	DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	Context.SetRenderTargets(FrameBufferRTV.Get(), DepthStencilView.Get());

	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

ID3D11Buffer* FRenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
{
	D3D11_BUFFER_DESC vertexbufferdesc = {};
	vertexbufferdesc.ByteWidth = byteWidth;
	vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

	ID3D11Buffer* vertexBuffer = nullptr;

	CheckHR(D3DDevice->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer));

	return vertexBuffer;
}

void FRenderer::ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
{
	if (vertexBuffer)
	{
		vertexBuffer->Release();
	}
}

void FRenderer::CreateConstantBuffer()
{
	D3D11_BUFFER_DESC constantbufferdesc = {};
	constantbufferdesc.ByteWidth = (sizeof(FConstants) + 0xf) & 0xfffffff0;
	constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
	constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	CheckHR(D3DDevice->CreateBuffer(&constantbufferdesc, nullptr, &TransformConstantBuffer));

	D3D11_BUFFER_DESC gridconstantbufferdesc = {};
	gridconstantbufferdesc.ByteWidth = (sizeof(FGridConstants) + 0xf) & 0xfffffff0;
	gridconstantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
	gridconstantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	gridconstantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	CheckHR(D3DDevice->CreateBuffer(&gridconstantbufferdesc, nullptr, &GridConstantBuffer));
}

void FRenderer::ReleaseConstantBuffer()
{
	if (TransformConstantBuffer)
	{
		TransformConstantBuffer->Release();
		TransformConstantBuffer = nullptr;
	}
	if (GridConstantBuffer)
	{
		GridConstantBuffer->Release();
		GridConstantBuffer = nullptr;
	}
}

void FRenderer::CreateRasterizerState()
{
	// 일반 메시: 뒷면 컬링
	D3D11_RASTERIZER_DESC SolidDesc = {};
	SolidDesc.FillMode = D3D11_FILL_SOLID;
	SolidDesc.CullMode = D3D11_CULL_BACK;
	
	SolidDesc.ScissorEnable = TRUE; // Scissor를 위해 추가

	CheckHR(D3DDevice->CreateRasterizerState(&SolidDesc, &DefaultRasterizerState));

	// 그리드: 양면 다 그림
	D3D11_RASTERIZER_DESC CullNoneDesc = SolidDesc;
	CullNoneDesc.CullMode = D3D11_CULL_NONE;
	CheckHR(D3DDevice->CreateRasterizerState(&CullNoneDesc, &CullNoneRasterizerState));

	// 하이라이트 외곽선: 1.05배로 부풀린 껍데기의 뒷면만 그린다 (inverted hull)
	D3D11_RASTERIZER_DESC CullFrontDesc = SolidDesc;
	CullFrontDesc.CullMode = D3D11_CULL_FRONT;
	CheckHR(D3DDevice->CreateRasterizerState(&CullFrontDesc, &CullFrontRasterizerState));

	// 와이어프레임 뷰 모드: 채우기만 끄고 컬링은 일반 메시와 동일
	D3D11_RASTERIZER_DESC WireDesc = SolidDesc;
	WireDesc.FillMode = D3D11_FILL_WIREFRAME;
	CheckHR(D3DDevice->CreateRasterizerState(&WireDesc, &WireframeRasterizerState));
}

void FRenderer::ReleaseRasterizerState()
{
	if (DefaultRasterizerState)
	{
		DefaultRasterizerState->Release();
		DefaultRasterizerState = nullptr;
	}
	if (CullNoneRasterizerState)
	{
		CullNoneRasterizerState->Release();
		CullNoneRasterizerState = nullptr;
	}
	if (CullFrontRasterizerState)
	{
		CullFrontRasterizerState->Release();
		CullFrontRasterizerState = nullptr;
	}
	if (WireframeRasterizerState)
	{
		WireframeRasterizerState->Release();
		WireframeRasterizerState = nullptr;
	}
}

void FRenderer::CreateAlphaBlendState()
{
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;

	// 0번째 렌더 타겟(메인 화면)
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;       // 새로 그릴 픽셀의 알파값 비중
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;  // 이미 그려진 픽셀의 비중 (1 - SrcAlpha)
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;           // 두 색상을 더함

	// 알파 채널 자체를 섞는다
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	CheckHR(D3DDevice->CreateBlendState(&blendDesc, &AlphaBlendState));

	D3D11_BLEND_DESC Desc{};
	auto& Target = Desc.RenderTarget[0];

	Target.BlendEnable = TRUE;

	// 불꽃 RGB에 알파를 곱하여 기존 화면 RGB에 더함
	Target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	Target.DestBlend = D3D11_BLEND_ONE;
	Target.BlendOp = D3D11_BLEND_OP_ADD;

	// 기존 화면 알파 유지
	Target.SrcBlendAlpha = D3D11_BLEND_ZERO;
	Target.DestBlendAlpha = D3D11_BLEND_ONE;
	Target.BlendOpAlpha = D3D11_BLEND_OP_ADD;

	Target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	CheckHR(D3DDevice->CreateBlendState(&Desc,&AdditiveBlendState));
}

void FRenderer::ReleaseAlphaBlendState()
{
	if (AlphaBlendState)
	{
		AlphaBlendState->Release();
		AlphaBlendState = nullptr;
	}
	if (AdditiveBlendState)
	{
		AdditiveBlendState->Release();
		AdditiveBlendState = nullptr;
	}
}

void FRenderer::CreateDepthStencilStates()
{
	D3D11_DEPTH_STENCIL_DESC DSDesc = {};
	DSDesc.DepthEnable = TRUE;
	DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DSDesc.StencilEnable = FALSE;
	CheckHR(D3DDevice->CreateDepthStencilState(&DSDesc, &DefaultDepthStencilState));

	D3D11_DEPTH_STENCIL_DESC GizmoDSDesc = DSDesc;
	GizmoDSDesc.DepthEnable = FALSE;
	CheckHR(D3DDevice->CreateDepthStencilState(&GizmoDSDesc, &GizmoDepthStencilState));

	D3D11_DEPTH_STENCIL_DESC HighlightDesc = {};
	HighlightDesc.DepthEnable = TRUE;  // 깊이 검사는 유지
	HighlightDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 깊이 기록 안 함
	HighlightDesc.DepthFunc = D3D11_COMPARISON_LESS;

	CheckHR(D3DDevice->CreateDepthStencilState(&HighlightDesc, &HighlightDepthStencilState));

	// 선택 오브젝트 본체용: 깊이는 Default와 같고, 덮은 픽셀의 스텐실을 1로 기록
	D3D11_DEPTH_STENCIL_DESC StencilWriteDesc = DSDesc;
	StencilWriteDesc.StencilEnable = TRUE;
	StencilWriteDesc.StencilReadMask = 0xFF;
	StencilWriteDesc.StencilWriteMask = 0xFF;
	StencilWriteDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	StencilWriteDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	// 가려진 부분도 마스크에 포함해야 외곽선 패스가 가려진 영역을 통째로 칠하지 않음
	StencilWriteDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
	StencilWriteDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	StencilWriteDesc.BackFace = StencilWriteDesc.FrontFace;
	CheckHR(D3DDevice->CreateDepthStencilState(&StencilWriteDesc, &StencilWriteDepthStencilState));

	// 외곽선용: 깊이 무시(가려져도 보임), 스텐실이 1이 아닌 곳에만 그림
	D3D11_DEPTH_STENCIL_DESC OutlineDesc = {};
	OutlineDesc.DepthEnable = FALSE;
	OutlineDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	OutlineDesc.DepthFunc = D3D11_COMPARISON_LESS;
	OutlineDesc.StencilEnable = TRUE;
	OutlineDesc.StencilReadMask = 0xFF;
	OutlineDesc.StencilWriteMask = 0x00;
	OutlineDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	OutlineDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	OutlineDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	OutlineDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	OutlineDesc.BackFace = OutlineDesc.FrontFace;
	CheckHR(D3DDevice->CreateDepthStencilState(&OutlineDesc, &OutlineDepthStencilState));

	D3D11_DEPTH_STENCIL_DESC Desc{};
	Desc.DepthEnable = TRUE;
	Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	Desc.DepthFunc = D3D11_COMPARISON_LESS;

	CheckHR(D3DDevice->CreateDepthStencilState(&Desc, &TranslucentDepthStencilState));
}

void FRenderer::ReleaseDepthStencilStates()
{
	if (DefaultDepthStencilState)
	{
		DefaultDepthStencilState->Release();
		DefaultDepthStencilState = nullptr;
	}
	if (GizmoDepthStencilState)
	{
		GizmoDepthStencilState->Release();
		GizmoDepthStencilState = nullptr;
	}
	if (HighlightDepthStencilState)
	{
		HighlightDepthStencilState->Release();
		HighlightDepthStencilState = nullptr;
	}
	if (StencilWriteDepthStencilState)
	{
		StencilWriteDepthStencilState->Release();
		StencilWriteDepthStencilState = nullptr;
	}
	if (OutlineDepthStencilState)
	{
		OutlineDepthStencilState->Release();
		OutlineDepthStencilState = nullptr;
	}
	if (TranslucentDepthStencilState)
	{
		TranslucentDepthStencilState->Release();
		TranslucentDepthStencilState = nullptr;
	}
}

void FRenderer::CreateTextResources()
{
	// 텍스트 셰이더
	Microsoft::WRL::ComPtr<ID3DBlob> ShaderBlob;
	if (!CompileShader(L"Assets/Shaders/TextShader.hlsl", "mainVS_Text", "vs_5_0", ShaderBlob.ReleaseAndGetAddressOf()))
		throw std::runtime_error("Text VS compile failed");
	CheckHR(D3DDevice->CreateVertexShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &TextVertexShader));

	D3D11_INPUT_ELEMENT_DESC Layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	CheckHR(D3DDevice->CreateInputLayout(Layout, ARRAYSIZE(Layout), ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), &TextInputLayout));
	ShaderBlob.Reset();

	if (!CompileShader(L"Assets/Shaders/TextShader.hlsl", "mainPS_Text", "ps_5_0", ShaderBlob.ReleaseAndGetAddressOf()))
		throw std::runtime_error("Text PS compile failed");
	CheckHR(D3DDevice->CreatePixelShader(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), nullptr, &TextPixelShader));

	// 샘플러 (기존 프로젝트에 샘플러가 하나도 없어서 신규 생성)
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	CheckHR(D3DDevice->CreateSamplerState(&SamplerDesc, &FontSamplerState));

	// 깊이 검사는 하되(오브젝트에 가려지게) 기록은 안 함(라벨끼리 겹칠 때 z-fight 방지)
	D3D11_DEPTH_STENCIL_DESC DSDesc = {};
	DSDesc.DepthEnable = TRUE;
	DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
	CheckHR(D3DDevice->CreateDepthStencilState(&DSDesc, &TextDepthStencilState));

	// Vertex Buffer: 매 프레임 내용이 바뀌므로 DYNAMIC, 고정 용량으로 1회만 생성
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.ByteWidth = sizeof(FVertexTexture) * MaxTextVertices;
	VBDesc.Usage = D3D11_USAGE_DYNAMIC;
	VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	CheckHR(D3DDevice->CreateBuffer(&VBDesc, nullptr, &TextVertexBuffer));

	// Index Buffer: quad 패턴(0,1,2,0,2,3)이 항상 동일하므로 1회 IMMUTABLE 생성
	const UINT MaxQuads = MaxTextVertices / 4;
	std::vector<uint32> Indices;
	Indices.reserve(MaxQuads * 6);
	for (UINT q = 0; q < MaxQuads; ++q)
	{
		const uint32 Base = q * 4;
		Indices.push_back(Base + 0);
		Indices.push_back(Base + 1);
		Indices.push_back(Base + 2);
		Indices.push_back(Base + 0);
		Indices.push_back(Base + 2);
		Indices.push_back(Base + 3);
	}

	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA IBData = { Indices.data() };
	CheckHR(D3DDevice->CreateBuffer(&IBDesc, &IBData, &TextIndexBuffer));
}

void FRenderer::ReleaseTextResources()
{
	if (TextVertexBuffer) { TextVertexBuffer->Release(); TextVertexBuffer = nullptr; }
	if (TextIndexBuffer) { TextIndexBuffer->Release(); TextIndexBuffer = nullptr; }
	if (TextDepthStencilState) { TextDepthStencilState->Release(); TextDepthStencilState = nullptr; }
	if (FontSamplerState) { FontSamplerState->Release(); FontSamplerState = nullptr; }
	if (TextInputLayout) { TextInputLayout->Release(); TextInputLayout = nullptr; }
	if (TextPixelShader) { TextPixelShader->Release(); TextPixelShader = nullptr; }
	if (TextVertexShader) { TextVertexShader->Release(); TextVertexShader = nullptr; }
}

void FRenderer::UpdateTextVertexBuffer(TArray<FVertexTexture>& Vertices)
{
	if (Vertices.Num() == 0) return;

	const UINT CopyCount = (static_cast<UINT>(Vertices.Num()) < MaxTextVertices) ? static_cast<UINT>(Vertices.Num()) : MaxTextVertices;

	D3D11_MAPPED_SUBRESOURCE Mapped;
	DeviceContext->Map(TextVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	memcpy(Mapped.pData, Vertices.GetData(), sizeof(FVertexTexture) * CopyCount);
	DeviceContext->Unmap(TextVertexBuffer, 0);
}

void FRenderer::RenderText(UINT IndexCount)
{
	if (IndexCount == 0) return;

	UINT Stride = sizeof(FVertexTexture);
	UINT Offset = 0;
	DeviceContext->IASetInputLayout(TextInputLayout);
	DeviceContext->IASetVertexBuffers(0, 1, &TextVertexBuffer, &Stride, &Offset);
	DeviceContext->IASetIndexBuffer(TextIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->VSSetShader(TextVertexShader, nullptr, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &TransformConstantBuffer);

	DeviceContext->PSSetShader(TextPixelShader, nullptr, 0);
	FFontAtlas* FontAtlas = GResourceManager::GetInstance()->GetDefaultFont();
	if (!FontAtlas) return;
	ID3D11ShaderResourceView* SRV = FontAtlas->GetSRV();
	DeviceContext->PSSetShaderResources(0, 1, &SRV);
	DeviceContext->PSSetSamplers(0, 1, &FontSamplerState);

	DeviceContext->RSSetState(CullNoneRasterizerState);
	float BlendFactor[4] = { 0, 0, 0, 0 };
	DeviceContext->OMSetBlendState(AlphaBlendState, BlendFactor, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(TextDepthStencilState, 0);

	DeviceContext->DrawIndexed(IndexCount, 0, 0);
}

void FRenderer::CreateTextureResources()
{
	GResourceManager& Resources = *GResourceManager::GetInstance();

	const FShaderResource* TextureShader = Resources.GetShader(FName("Mesh.Texture"));

	ID3D11SamplerState* Sampler = Resources.GetSampler(FName("LinearClamp"));

	if (!TextureShader || !TextureShader->VertexShader || !TextureShader->PixelShader ||
		!TextureShader->InputLayout || !Sampler)
	{
		throw std::runtime_error("Required texture render resources are missing");
	}

	TextureVertexShader = TextureShader->VertexShader.Get();
	TexturePixelShader = TextureShader->PixelShader.Get();
	TextureInputLayout = TextureShader->InputLayout.Get();
	TextureSamplerState = Sampler;

	// Draw마다 갱신하는 기존 상수 버퍼는 이번 단계에서 유지한다.
	D3D11_BUFFER_DESC UVDesc{};
	UVDesc.ByteWidth = sizeof(FTextureDrawConstants);
	UVDesc.Usage = D3D11_USAGE_DEFAULT;
	UVDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	CheckHR(D3DDevice->CreateBuffer(&UVDesc, nullptr, &TextureUVConstantBuffer));
}

void FRenderer::ReleaseTextureResources()
{
	if (TextureUVConstantBuffer)
	{
		TextureUVConstantBuffer->Release();
		TextureUVConstantBuffer = nullptr;
	}

	// 공유 자원은 참조만 끊는다.
	TextureSamplerState = nullptr;
	TextureInputLayout = nullptr;
	TexturePixelShader = nullptr;
	TextureVertexShader = nullptr;
}

void FRenderer::RenderTexturedPrimitive(const FPrimitiveRenderData& Data,EViewModeIndex InViewMode, bool bWriteStencil)
{
	if (!Data.VertexBuffer||!Data.IndexBuffer||!Data.Material||Data.IndexCount == 0){return;}

	FTextureDrawConstants Constants{};
	Constants.UV = Data.UVTransform;
	Constants.Tint = Data.TextureTint;
	Constants.AlphaCutoff = Data.AlphaCutoff;

	DeviceContext->UpdateSubresource(TextureUVConstantBuffer,0,nullptr,&Constants,0,0);
	//UV변환에 사용함
    DeviceContext->VSSetConstantBuffers(1, 1, &TextureUVConstantBuffer);

	//색상 및 알파컷아웃에 사용함
	DeviceContext->PSSetConstantBuffers(1, 1, &TextureUVConstantBuffer);
	const UINT Offset = 0;

	// 위치와 UV 형식으로 정점 버퍼를 읽음
	DeviceContext->IASetInputLayout(TextureInputLayout);

	DeviceContext->IASetVertexBuffers(0,1,&Data.VertexBuffer,&Data.Stride,&Offset);

	DeviceContext->IASetIndexBuffer(Data.IndexBuffer,DXGI_FORMAT_R32_UINT,0);

	DeviceContext->IASetPrimitiveTopology(Data.Topology);

	// 정점 위치 변환에 사용할 셰이더와 행렬을 연결함
	DeviceContext->VSSetShader(TextureVertexShader,	nullptr,0);

	DeviceContext->VSSetConstantBuffers(0,1,&TransformConstantBuffer);

	// 픽셀 셰이더의 t0와 s0에 텍스처와 샘플러를 연결함
	DeviceContext->PSSetShader(InViewMode == EViewModeIndex::VMI_Wireframe ? WireframePixelShader : TexturePixelShader,nullptr,0);

	DeviceContext->PSSetShaderResources(0,1,&Data.Material);

	DeviceContext->PSSetSamplers(0,1,&TextureSamplerState);

	// 구형 텍스처는 백페이스 컬링 / 플립북,빌보드 는 none 컬링
	ID3D11RasterizerState* RasterizerState = InViewMode == EViewModeIndex::VMI_Wireframe ? WireframeRasterizerState : (Data.bTwoSided ? CullNoneRasterizerState : DefaultRasterizerState);
	DeviceContext->RSSetState(RasterizerState);

	//블렌드 모드에 따라 가산블렌딩으로 변환
	const bool bAdditive = Data.BlendMode == EPrimitiveBlendMode::Additive;
	DeviceContext->OMSetBlendState(bAdditive ? AdditiveBlendState : nullptr,nullptr,0xffffffff);

	if (bAdditive)
	{
		DeviceContext->OMSetDepthStencilState(TranslucentDepthStencilState, 0);
	}
	else if (bWriteStencil)
	{
		DeviceContext->OMSetDepthStencilState(StencilWriteDepthStencilState, 1);
	}
	else
	{
		DeviceContext->OMSetDepthStencilState(DefaultDepthStencilState, 0);
	}

	DeviceContext->DrawIndexed(Data.IndexCount,0,0);

	// 사용한 텍스처 슬롯을 비움
	ID3D11ShaderResourceView* NullSRV = nullptr;
	DeviceContext->PSSetShaderResources(0, 1, &NullSRV);
	// 상태 복원
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(DefaultDepthStencilState, 0);
}

void FRenderer::BeginFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
	PrepareRTVDSV();
}

void FRenderer::EndFrame()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	SwapBuffer();

	GContext::GetInstance()->UnbindRenderTargets();
}

void FRenderer::RenderView(FEditor* Editor,UScene* Scene,const FRenderView& View)
{
	if (!View.Camera ||	View.Viewport.Width <= 0.0f || View.Viewport.Height <= 0.0f)
	{ return; }

	UCameraComponent* Camera = View.Camera;
	const FViewSettings& ViewSettings = View.ViewSettings;

	SetViewportAndScissor(View.Viewport);
	
	DeviceContext->RSSetState(DefaultRasterizerState);
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(DefaultDepthStencilState, 0);

	ID3D11ShaderResourceView* NullSRV = nullptr;
	DeviceContext->PSSetShaderResources(0, 1, &NullSRV);

	Camera->SetAspectRatio(View.Viewport.Width / View.Viewport.Height);

	LineBatcher.Clear();

	FMatrix ViewProjMatrix = Camera->GetViewMatrix() * Camera->GetProjectionMatrix();

	TArray<FPrimitiveRenderData> RenderList = RenderUtil::GetRenderList(Editor, Scene, Camera);

	const bool bShowPrimitives = ViewSettings.ShowFlags.IsEnabled(EEngineShowFlag::Primitives);

	const EViewModeIndex ViewMode = ViewSettings.ViewMode;

	TArray<const FPrimitiveRenderData*> AdditiveRenderList;
	TArray<const FPrimitiveRenderData*> OutlineRenderList;

	for (auto& Item : RenderList)
	{
		if (!Item.WorldMatrix || !Item.VertexBuffer || !Item.IndexBuffer || Item.IndexCount == 0)
		{
			continue;
		}
		if (bShowPrimitives) 
		{
			if (Item.BlendMode == EPrimitiveBlendMode::Additive)
			{
				AdditiveRenderList.Add(&Item);
			}
			else
			{
				FMatrix MVP = (*Item.WorldMatrix) * ViewProjMatrix;
				UpdateTransformConstantBuffer(MVP);

				// 선택된 오브젝트는 그리면서 스텐실 마스크를 기록하고, 외곽선은 나중에 그림
				const bool bOutline = Item.isSelected && Item.bAllowOutline && ViewMode != EViewModeIndex::VMI_Wireframe;
				RenderPrimitive(Item, ViewMode, bOutline);
				if (bOutline)
				{
					OutlineRenderList.Add(&Item);
				}
			}
		}
	}
	// 모든 라인 요청을 배처의 통합 배열에 즉시 병합함
	RenderUtil::SubmitLineDrawRequests(Editor, Scene, Camera, ViewSettings, LineBatcher);

	// 통합 데이터를 GPU에 업로드하고 배치 렌더링함
	RenderBatchLine(ViewProjMatrix);

	for (const FPrimitiveRenderData* Item : AdditiveRenderList)
	{
		const FMatrix MVP = (*Item->WorldMatrix) * ViewProjMatrix;
		UpdateTransformConstantBuffer(MVP);

		RenderPrimitive(*Item, ViewMode);
	}

	// 외곽선: 모든 씬 오브젝트 이후, 기즈모 이전에 그림 (깊이 무시라 뒤에 그려진 물체에 덮이지 않게)
	for (const FPrimitiveRenderData* Item : OutlineRenderList)
	{
		const FMatrix MVP = (*Item->WorldMatrix) * ViewProjMatrix;
		UpdateTransformConstantBuffer(MVP);

		RenderOutline(*Item);
	}

	// Render Gizmo
	if (View.bDrawEditorGizmos)
	{
		TArray<FPrimitiveRenderData> GizmoRenderList = RenderUtil::GetGizmoList(Editor, Scene, Camera, View.Viewport);
		for (const auto& Item : GizmoRenderList)
		{
			FMatrix MVP = (*Item.WorldMatrix) * ViewProjMatrix;
			UpdateTransformConstantBuffer(MVP);

			if (Item.isSelected)
			{ RenderHighlight(Item); }
			RenderGizmo(Item);
		}
	}

	FFontAtlas* FontAtlas = GResourceManager::GetInstance()->GetDefaultFont();
	if (FontAtlas)
	{
		TArray<FWorldTextItem> TextItems = RenderUtil::GetTextRenderList(Scene, Camera, ViewSettings.ShowFlags.IsEnabled(EEngineShowFlag::UUID));
		TArray<FVertexTexture> TextVerts = FTextMeshBuilder::Build(TextItems, *FontAtlas);
		UpdateTextVertexBuffer(TextVerts);
		UpdateTransformConstantBuffer(ViewProjMatrix); // 텍스트는 이미 월드공간이라 World=Identity
		const UINT TextVertexCount = (static_cast<UINT>(TextVerts.Num()) < MaxTextVertices) ? static_cast<UINT>(TextVerts.Num()) : MaxTextVertices;
		RenderText(TextVertexCount / 4 * 6);
	}

	UpdateTransformConstantBuffer(ViewProjMatrix);
}

void FRenderer::UpdateTransformConstantBuffer(const FMatrix& MVP)
{
	if (!DeviceContext || !TransformConstantBuffer)
	{
		return;
	}
	D3D11_MAPPED_SUBRESOURCE constantbufferMSR{};

	HRESULT hr = DeviceContext->Map(TransformConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
	if (SUCCEEDED(hr))
	{
		FConstants* constants = (FConstants*)constantbufferMSR.pData;
		if (constants)
		{
			constants->MVP = MVP;
		}
		DeviceContext->Unmap(TransformConstantBuffer, 0);
	}
	else
	{
		UE_LOG("[FRenderer] Failed to Map TransformConstantBuffer. HRESULT: {}\n", hr);
	}
}

void FRenderer::UpdateGridConstantBuffer(const FGridConstants& GridConstants)
{
	if (!DeviceContext || !GridConstantBuffer)
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE constantbufferMSR{};

	HRESULT hr = DeviceContext->Map(GridConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
	if (SUCCEEDED(hr))
	{
		FGridConstants* constants = (FGridConstants*)constantbufferMSR.pData;
		if (constants)
		{
			constants->CameraPos = GridConstants.CameraPos;
			constants->GridPlaneType = GridConstants.GridPlaneType;
		}
		DeviceContext->Unmap(GridConstantBuffer, 0);
	}
	else
	{
		UE_LOG("[FRenderer] Failed to Map GridConstantBuffer. HRESULT: {}\n", hr);
	}
}

void FRenderer::RenderPrimitive(const FPrimitiveRenderData& Data, EViewModeIndex InViewMode, bool bWriteStencil)
{
	if (Data.Pipeline == EPrimitivePipeline::Texture) {
		RenderTexturedPrimitive(Data, InViewMode, bWriteStencil);
		return;
	}
	UINT Offset = 0;
	DeviceContext->IASetInputLayout(SimpleInputLayout);
	DeviceContext->IASetVertexBuffers(0, 1, &Data.VertexBuffer, &Data.Stride, &Offset);
	DeviceContext->IASetIndexBuffer(Data.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetPrimitiveTopology(Data.Topology);

	DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &TransformConstantBuffer);

	// Lit currently uses the unlit pipeline until lighting is implemented.
	DeviceContext->RSSetState(InViewMode == EViewModeIndex::VMI_Wireframe ? WireframeRasterizerState : DefaultRasterizerState);

	DeviceContext->PSSetShader(InViewMode == EViewModeIndex::VMI_Wireframe ? WireframePixelShader : SimplePixelShader, nullptr, 0);
	//DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);

	if (bWriteStencil)
	{
		DeviceContext->OMSetDepthStencilState(StencilWriteDepthStencilState, 1);
	}
	else
	{
		DeviceContext->OMSetDepthStencilState(DefaultDepthStencilState, 0);
	}
	// BindMaterial(Data.Material);

	DeviceContext->DrawIndexed(Data.IndexCount, 0, 0);
}

void FRenderer::RenderOutline(const FPrimitiveRenderData& Data)
{
	UINT Offset = 0;
	// 두 레이아웃 모두 POSITION(0), COLOR(12) 배치라 VS_Highlight와 호환됨
	DeviceContext->IASetInputLayout(SimpleInputLayout);
	DeviceContext->IASetVertexBuffers(0, 1, &Data.VertexBuffer, &Data.Stride, &Offset);
	DeviceContext->IASetIndexBuffer(Data.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetPrimitiveTopology(Data.Topology);

	DeviceContext->VSSetShader(HighlightVertexShader, nullptr, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &TransformConstantBuffer);

	// 안쪽은 스텐실이 가려주므로 컬링 불필요 (Plane처럼 한 면짜리도 처리)
	DeviceContext->RSSetState(CullNoneRasterizerState);

	DeviceContext->PSSetShader(HighlightPixelShader, nullptr, 0);

	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(OutlineDepthStencilState, 1);

	DeviceContext->DrawIndexed(Data.IndexCount, 0, 0);

	DeviceContext->OMSetDepthStencilState(DefaultDepthStencilState, 0);
}

void FRenderer::RenderHighlight(const FPrimitiveRenderData& Data)
{
	UINT Offset = 0;
	DeviceContext->IASetInputLayout(SimpleInputLayout);
	DeviceContext->IASetVertexBuffers(0, 1, &Data.VertexBuffer, &Data.Stride, &Offset);
	DeviceContext->IASetIndexBuffer(Data.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetPrimitiveTopology(Data.Topology);

	DeviceContext->VSSetShader(HighlightVertexShader, nullptr, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &TransformConstantBuffer);

	DeviceContext->RSSetState(CullFrontRasterizerState);

	DeviceContext->PSSetShader(HighlightPixelShader, nullptr, 0);

	DeviceContext->OMSetDepthStencilState(HighlightDepthStencilState, 0);

	DeviceContext->DrawIndexed(Data.IndexCount, 0, 0);
}

void FRenderer::RenderGrid(FMeshResource* Data)
{
	if (!Data) return;
	ID3D11Buffer* VertexBuffer = Data->GetVertexBuffer();
	const UINT Stride = Data->GetStride();
	UINT Offset = 0;
	DeviceContext->IASetInputLayout(SimpleInputLayout);
	DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	DeviceContext->IASetIndexBuffer(Data->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->VSSetShader(GridVertexShader, nullptr, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &TransformConstantBuffer);
	DeviceContext->VSSetConstantBuffers(1, 1, &GridConstantBuffer);

	DeviceContext->RSSetState(CullNoneRasterizerState);

	DeviceContext->PSSetShader(GridPixelShader, nullptr, 0);
	DeviceContext->PSSetConstantBuffers(1, 1, &GridConstantBuffer);

	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	UINT sampleMask = 0xffffffff;
	DeviceContext->OMSetBlendState(AlphaBlendState, blendFactor, sampleMask);
	DeviceContext->OMSetDepthStencilState(DefaultDepthStencilState, 0);

	DeviceContext->DrawIndexed(Data->GetIndexCount(), 0, 0);
}

void FRenderer::RenderGizmo(const FPrimitiveRenderData& Data)
{
	UINT Offset = 0;
	DeviceContext->IASetInputLayout(SimpleInputLayout);
	DeviceContext->IASetVertexBuffers(0, 1, &Data.VertexBuffer, &Data.Stride, &Offset);
	DeviceContext->IASetIndexBuffer(Data.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetPrimitiveTopology(Data.Topology);

	DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &TransformConstantBuffer);

	DeviceContext->RSSetState(DefaultRasterizerState);

	DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);

	DeviceContext->OMSetDepthStencilState(GizmoDepthStencilState, 0);
	// BindMaterial(Data.Material); 

	DeviceContext->DrawIndexed(Data.IndexCount, 0, 0);
}

void FRenderer::RenderBatchLine(const FMatrix& ViewProj)
{
	const UINT VertexCount = LineBatcher.GetVertexCount();
	const UINT IndexCount = LineBatcher.GetIndexCount();

	if (VertexCount == 0 || IndexCount == 0)
	{
		return;
	}

	if (!LineBatcher.Build())
	{
		LineBatcher.Clear();
		return;
	}

	UpdateTransformConstantBuffer(ViewProj);

	ID3D11Buffer* VB = LineBatcher.GetVertexBuffer();
	ID3D11Buffer* IB = LineBatcher.GetIndexBuffer();
	UINT Stride = sizeof(FVertexSimple);
	UINT Offset = 0;

	DeviceContext->IASetInputLayout(SimpleInputLayout);
	DeviceContext->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
	DeviceContext->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	DeviceContext->VSSetShader(BatchLineVertexShader, nullptr, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &TransformConstantBuffer);

	DeviceContext->RSSetState(DefaultRasterizerState);

	DeviceContext->PSSetShader(BatchLinePixelShader, nullptr, 0);

	float BlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	UINT SampleMask = 0xffffffff;
	DeviceContext->OMSetBlendState(AlphaBlendState, BlendFactor, SampleMask);

	DeviceContext->OMSetDepthStencilState(DefaultDepthStencilState, 0);

	DeviceContext->DrawIndexed(IndexCount, 0, 0);

	// 나중에 같은 데이터로 여러번 그리려면 Clear() 분리가 필요할 수 있음
	LineBatcher.Clear();
}

bool FRenderer::CreateSwapChain(HWND HWnd, uint32 Width, uint32 Height)
{
	if (!D3DDevice || !HWnd || Width == 0 || Height == 0)
	{
		return false;
	}

	ComPtr<IDXGIDevice> DxgiDevice;
	ComPtr<IDXGIAdapter> Adapter;
	ComPtr<IDXGIFactory> Factory;

	HRESULT Result = D3DDevice->QueryInterface(IID_PPV_ARGS(DxgiDevice.GetAddressOf()));

	if (FAILED(Result)){return false;}

	Result = DxgiDevice->GetAdapter(Adapter.GetAddressOf());
	if (FAILED(Result))
	{
		return false;
	}

	Result = Adapter->GetParent(IID_PPV_ARGS(Factory.GetAddressOf()));

	if (FAILED(Result))
	{
		return false;
	}

	DXGI_SWAP_CHAIN_DESC Desc{};
	Desc.BufferDesc.Width = Width;
	Desc.BufferDesc.Height = Height;
	Desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = 2;
	Desc.OutputWindow = HWnd;
	Desc.Windowed = TRUE;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain> NewSwapChain;

	Result = Factory->CreateSwapChain(D3DDevice, &Desc, NewSwapChain.GetAddressOf());

	if (FAILED(Result))
	{
		UE_LOG("[FRenderer] Failed to create swap chain. HRESULT: {}\n", Result);
		return false;
	}

	Result = NewSwapChain->GetDesc(&Desc);
	if (FAILED(Result))
	{
		return false;
	}

	SwapChain = std::move(NewSwapChain);

	ViewportInfo = {0.0f,0.0f,static_cast<float>(Desc.BufferDesc.Width),
		static_cast<float>(Desc.BufferDesc.Height),	0.0f,1.0f};

	return true;
}

bool FRenderer::CreateFrameBuffer()
{
	if (!D3DDevice || !SwapChain.Get())
	{
		return false;
	}

	ComPtr<ID3D11Texture2D> NewFrameBuffer;
	ComPtr<ID3D11RenderTargetView> NewRTV;

	HRESULT Result = SwapChain->GetBuffer(0, IID_PPV_ARGS(NewFrameBuffer.GetAddressOf()));

	if (FAILED(Result))
	{
		UE_LOG("[FRenderer] Failed to get back buffer from SwapChain. HRESULT: {}\n", Result);
		ReleaseFrameBuffer();
		return false;
	}

	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc{};
	RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Result = D3DDevice->CreateRenderTargetView(NewFrameBuffer.Get(), &RTVDesc, NewRTV.GetAddressOf());

	if (FAILED(Result))
	{
		UE_LOG("[FRenderer] Failed to create Render Target View. HRESULT: {}\n", Result);
		return false;
	}

	FrameBuffer = std::move(NewFrameBuffer);
	FrameBufferRTV = std::move(NewRTV);

	return true;
}

void FRenderer::ReleaseFrameBuffer()
{
	FrameBufferRTV.Reset();
	FrameBuffer.Reset();
}

bool FRenderer::CreateDepthStencilBuffer(uint32 Width, uint32 Height)
{
	if (!D3DDevice || Width == 0 || Height == 0){ return false; }

	ComPtr<ID3D11Texture2D> NewDepthBuffer;
	ComPtr<ID3D11DepthStencilView> NewDSV;

	D3D11_TEXTURE2D_DESC DepthStencilDesc{};
	DepthStencilDesc.Width = Width;
	DepthStencilDesc.Height = Height;
	DepthStencilDesc.MipLevels = 1;
	DepthStencilDesc.ArraySize = 1;
	DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthStencilDesc.SampleDesc.Count = 1;
	DepthStencilDesc.SampleDesc.Quality = 0;
	DepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthStencilDesc.CPUAccessFlags = 0;
	DepthStencilDesc.MiscFlags = 0;

	HRESULT Result = D3DDevice->CreateTexture2D(&DepthStencilDesc, nullptr, NewDepthBuffer.GetAddressOf());

	if (FAILED(Result)){ return false; }

	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc{};
	DSVDesc.Format = DepthStencilDesc.Format;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;

	Result = D3DDevice->CreateDepthStencilView( NewDepthBuffer.Get(),&DSVDesc, NewDSV.GetAddressOf());

	if (FAILED(Result))
	{
		UE_LOG("[FRenderer] Failed to create depth stencil view. HRESULT: {}\n",Result);
		return false;
	}

	DepthStencilBuffer = std::move(NewDepthBuffer);
	DepthStencilView = std::move(NewDSV);

	return true;
}

void FRenderer::ReleaseDepthStencilBuffer()
{
	DepthStencilView.Reset();
	DepthStencilBuffer.Reset();
}

void FRenderer::OnResize(uint32 Width, uint32 Height)
{
	bRenderReady = false;

	GContext& Context = *GContext::GetInstance();

	if (Width == 0 || Height == 0 || !D3DDevice || !Context.IsInitialized() || !SwapChain.Get() || bGraphicsFailed)
	{ return; }

	Context.UnbindRenderTargets();

	ReleaseDepthStencilBuffer();
	ReleaseFrameBuffer();

	Context.Flush();

	const HRESULT Result = SwapChain->ResizeBuffers(0,Width,Height,	DXGI_FORMAT_UNKNOWN, 0);

	if (FAILED(Result))
	{
		bGraphicsFailed = true;
		PostQuitMessage(EXIT_FAILURE);
		return;
	}

	ViewportInfo = {0.0f,0.0f,static_cast<float>(Width),static_cast<float>(Height),0.0f,1.0f};

	if (!CreateFrameBuffer() || !CreateDepthStencilBuffer(Width, Height))
	{
		ReleaseFrameBuffer();
		ReleaseDepthStencilBuffer();

		bGraphicsFailed = true;
		PostQuitMessage(EXIT_FAILURE);
		return;
	}

	Context.SetRenderTargets(FrameBufferRTV.Get(),DepthStencilView.Get());

	Context.SetViewport(ViewportInfo);

	bRenderReady = true;
}

// 단일 View
void FRenderer::Render(float DeltaTime,FEditor* Editor,UScene* Scene)
{
	if (!IsRenderReady() || !Editor || !Scene)
	{
		return;
	}

	FRenderView View{};
	View.Camera = Editor->GetEditorCamera();
	View.Viewport = ViewportInfo;
	View.ViewSettings = Editor->GetViewSettings();
	View.bDrawEditorGizmos = true;

	TArray<FRenderView> Views;
	Views.Add(View);

	Render(DeltaTime, Editor, Scene, Views);
}

// 다중 View
void FRenderer::Render(float DeltaTime,FEditor* Editor,UScene* Scene, const TArray<FRenderView>& Views)
{
	if (!IsRenderReady() || !Editor || !Scene)
	{
		return;
	}

	BeginFrame();

	for (const FRenderView& View : Views)
	{
		RenderView(Editor, Scene, View);
	}

	SetViewportAndScissor(ViewportInfo);

	// UI 렌더링
	for (auto Item : Editor->GetWindows())
	{
		Item->Render(DeltaTime);
	}

	EndFrame();
}

// Viewport,Scissor 설정
void FRenderer::SetViewportAndScissor(const D3D11_VIEWPORT& Viewport)
{
	GContext::GetInstance()->SetViewport(Viewport);

	// Viewport는 정수 픽셀 경계로 구성한다.
	const D3D11_RECT ScissorRect
	{
		static_cast<LONG>(Viewport.TopLeftX),
		static_cast<LONG>(Viewport.TopLeftY),
		static_cast<LONG>(Viewport.TopLeftX + Viewport.Width),
		static_cast<LONG>(Viewport.TopLeftY + Viewport.Height)
	};

	DeviceContext->RSSetScissorRects(1, &ScissorRect);
}