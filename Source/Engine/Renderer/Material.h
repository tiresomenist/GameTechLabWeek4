#pragma once

#include <d3d11.h>
#include "Core/Core.h"
#include "Core/Container/String.h"
#include "Core/Math/Vector.h"

struct FShaderResource;

enum class EPrimitiveBlendMode : uint32
{
	Opaque,
	Additive,
};

struct FMaterial
{
	// TODO:: 현재 .mat 파일이 없으므로 FMaterial이 자신의 텍스처 파일 경로를 가지고 있어야 함
	// 모든 포인터는 비소유 참조.
	// 참조 대상은 이 Material을 사용하는 Draw가 끝날 때까지 유효해야 한다.

	ID3D11ShaderResourceView* SRV = nullptr; //SRV
	const FShaderResource* Shader = nullptr;	//VS,PS,InputLayout을 묶은 구조체
	ID3D11SamplerState* Sampler = nullptr;	//Sampler

	EPrimitiveBlendMode BlendMode = EPrimitiveBlendMode::Opaque;	//Blend mode

	ID3D11Buffer* ConstantBuffer = nullptr;	//UV·Tint·AlphaCutoff등을 담을 constantBuffer

	FString TexturePath;


};