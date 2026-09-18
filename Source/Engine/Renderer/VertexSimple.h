#pragma once
#include <cstddef>
#include <type_traits>

#include "Core/Math/Vector.h"

struct FVertexSimple
{
	float x = 0.0f, y = 0.0f, z = 0.0f;    // Position
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f; // Color
};

struct FVertexTest
{
	float x, y, z;      // Position
	float nx, ny, nz;   // Normal (빛 테스트용)
	float u, v;         // UV (텍스처 테스트용)
};

struct FVertexTexture {
	float x = 0.0f, y = 0.0f, z = 0.0f;
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f; // Color
	float u = 0.0f, v = 0.0f;
};


struct FVertexPNCT {
	float X = 0.0f, Y = 0.0f, Z = 0.0f;				// Position
	float NX = 0.0f, NY = 0.0f, NZ = 0.0f;			// Normal Vector
	float R = 0.0f, G = 0.0f, B = 0.0f, A = 1.0f;	//Color
	float U = 0.0f, V = 0.0f;						//UV
};

//형식 체크용 static_assert
static_assert(std::is_standard_layout_v<FVertexPNCT>);
static_assert(sizeof(FVertexPNCT) == 48);
static_assert(offsetof(FVertexPNCT, X) == 0);
static_assert(offsetof(FVertexPNCT, NX) == 12);
static_assert(offsetof(FVertexPNCT, R) == 24);
static_assert(offsetof(FVertexPNCT, U) == 40);

