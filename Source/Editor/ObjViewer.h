#pragma once
#include "Editor/Editor.h"

class UScene;

class FObjViewer : public FEditor
{
public:
    // Viewer 전용 씬, 카메라, 표시 요소를 생성합니다.
    void Initialize() override;

    // Viewer 씬과 카메라 입력을 갱신합니다.
    void Tick(float DeltaTime) override;

    // 공통 표시 요소와 Viewer 전용 씬을 정리합니다.
    void Release() override;

    // Viewer가 소유한 미리보기 씬을 반환합니다.
    UScene* GetCurrentScene() override;

    // Viewer의 메뉴를 구성합니다.
    void DrawMenu() override;

    // Viewer의 도구 창을 구성합니다.
    void DrawWindows(float DeltaTime) override;

    // 오브젝트 변형용 기즈모의 표시 여부를 반환합니다.
    bool ShouldDrawEditorGizmos() const override;

    // 단일 카메라로 전체 출력 영역을 사용하는 뷰를 구성합니다.
    TArray<FRenderView> BuildRenderViews(const D3D11_VIEWPORT& FullViewport) const override;

    D3D11_VIEWPORT GetRenderViewport(const D3D11_VIEWPORT& FullViewport) const override;

private:
    float MenuBarHeight = 0.0f;
    UScene* PreviewScene = nullptr;
};