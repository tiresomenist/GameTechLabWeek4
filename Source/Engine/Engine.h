#pragma once
#include "Windows.h"
#include "Engine/Renderer/Renderer.h"

#include <chrono>

#include "EngineStats.h"

class FConsole;
class FEditor;
class FObjViewer;


enum class EApplicationMode
{
	Editor,
	ObjViewer
};

struct FUnitData
{
	float FrameTimeMs = 0.0f;
	float GameTimeMs = 0.0f;
	float DrawTimeMs = 0.0f;
	float GPUTimeMs = 0.0f;
	float GPUWaitMs = 0.0f;
};

struct FMemoryData
{
	size_t PhysicalMemMB;
	size_t VirtualMemMB;
	size_t TotalMemMB;
	size_t UsedMemMB;
	size_t GPUDedicatedMemMB;
	size_t GPUVRAMUsedMB;
	size_t HeapBytes;
	size_t ObjectCount;

};

class GEngine
{
public:
	static GEngine* GetInstance();

	void Initialize(HWND InHwnd, EApplicationMode Mode);

	void Tick();

	void Destroy();

	void OnResize(uint32 Width, uint32 Height);
	const D3D11_VIEWPORT& GetViewport() const;

	FConsole* GetConsole() const { return Console; };

	// 게임 시작 이후 얼마나 흘렀는지 반환합니다.
	float GetTime();

	const FEngineStats& GetEngineStats() const { return EngineStats; };

private:
	EApplicationMode ApplicationMode = EApplicationMode::Editor;
	float LastTickTime = 0;
	float StartTime = 0;

	FRenderer Renderer;
	FConsole* Console = nullptr;
	FEditor* Editor = nullptr;

	// 싱글톤
	GEngine() = default;
	~GEngine() = default;
	GEngine(const GEngine&) = delete;
	GEngine& operator=(const GEngine&) = delete;

	FEngineStats EngineStats;
	float GameTimeMs;
};
