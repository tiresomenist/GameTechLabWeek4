#include "pch.h"
#include "ObjViewer.h"
#include "Editor/Grid.h"
#include "Editor/Gizmo/WorldAxisGizmo.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Input/InputManager.h"
#include "ImGui/imgui.h"

// Viewer 전용 씬, 카메라, 그리드와 월드축을 생성합니다.
void FObjViewer::Initialize()
{
    // 에디터의 ImGui 배치 파일을 읽거나 덮어쓰지 않도록 설정합니다.
    ImGui::GetIO().IniFilename = nullptr;

    // 카메라는 PreviewScene의 Actor가 소유하고 EditorCamera는 참조만 합니다.
    PreviewScene = new UScene();
    PreviewScene->CreateMainCamera();
    EditorCamera = PreviewScene->GetMainCamera();
    EditorCamera->SetIsPerspective(true);
    EditorCamera->SetRelativeLocation(FVector(-15.0f, -15.0f, 10.0f));
    EditorCamera->LookAt(FVector(0.0f, 0.0f, 0.0f));
    CameraController.SetCamera(EditorCamera);

    // 모델 미리보기에 사용할 기본 표시 설정을 구성합니다.
    SetViewMode(EViewModeIndex::VMI_Unlit);
    SetShowPrimitives(true);
    SetShowGrid(true);
    SetShowWorldAxis(true);
    SetShowUUIDLabels(false);
    SetShowBoundingBoxes(false);

    // 기존 등록 기능과 라인 렌더링 경로를 재사용합니다.
    RegisterGrid(UGrid::GetClass());
    RegisterGizmo(UWorldAxisGizmo::GetClass());
    PreviewScene->BeginPlay();
}

// Viewer 씬을 갱신하고 기존 카메라 컨트롤러에 입력을 전달합니다.
void FObjViewer::Tick(float DeltaTime)
{
    if (!PreviewScene || !EditorCamera) return;
    PreviewScene->Tick(DeltaTime);

    // 이번 단계에서 사용하지 않는 선택 및 기즈모 전환 입력을 소비합니다.
    GInputManager& Input = *GInputManager::GetInstance();
    Input.ConsumeLeftClick();
    Input.ConsumeSpacePress();

    // UI 조작 중에는 카메라 입력을 차단하고 누적 회전량을 비웁니다.
    const ImGuiIO& IO = ImGui::GetIO();
    const D3D11_VIEWPORT Viewport = GetRenderViewport(GEngine::GetInstance()->GetViewport());
    if (IO.WantCaptureMouse || IO.WantCaptureKeyboard
        || Viewport.Width <= 0.0f || Viewport.Height <= 0.0f)
    {
        int32 DeltaX = 0, DeltaY = 0;
        Input.ConsumeRightDragDelta(DeltaX, DeltaY);
        return;
    }

    CameraController.Tick(DeltaTime);
}

// 공통 등록 자원을 정리한 뒤 Viewer 씬을 제거합니다.
void FObjViewer::Release()
{
    // 컨트롤러의 카메라 참조와 등록한 그리드·기즈모를 먼저 정리합니다.
    FEditor::Release();
    EditorCamera = nullptr;

    // 씬을 삭제하면 소유 Actor와 카메라 컴포넌트도 함께 삭제됩니다.
    if (PreviewScene)
    {
        PreviewScene->EndPlay();
        delete PreviewScene;
        PreviewScene = nullptr;
    }
}

// 렌더러와 공통 기능에 Viewer 전용 씬을 제공합니다.
UScene* FObjViewer::GetCurrentScene()
{
    return PreviewScene;
}

// Viewer 메뉴를 구성하며 이번 단계에서는 빈 화면 구성을 유지합니다.
void FObjViewer::DrawMenu()
{
    // 파일 열기 메뉴는 이후 단계에서 추가합니다.
}

// Viewer 도구 창을 구성하며 이번 단계에서는 별도 창을 만들지 않습니다.
void FObjViewer::DrawWindows(float DeltaTime)
{
    // 모델 정보와 섹션 목록은 이후 단계에서 추가합니다.
}

// Viewer에서는 오브젝트 변형용 기즈모를 표시하지 않습니다.
bool FObjViewer::ShouldDrawEditorGizmos() const
{
    return false;
}

// Viewer 카메라와 전체 출력 영역으로 단일 렌더 뷰를 구성합니다.
TArray<FRenderView> FObjViewer::BuildRenderViews(const D3D11_VIEWPORT& FullViewport) const
{
    TArray<FRenderView> Views;
    if (!PreviewScene || !EditorCamera) return Views;

    // 창 크기에 맞는 출력 영역과 Viewer 표시 설정을 전달합니다.
    FRenderView View{};
    View.Camera = EditorCamera;
    View.Viewport = GetRenderViewport(FullViewport);
    View.ViewSettings = GetViewSettings();
    View.bDrawEditorGizmos = ShouldDrawEditorGizmos();
    if (View.Viewport.Width <= 0.0f || View.Viewport.Height <= 0.0f) return Views;

    Views.Add(View);
    return Views;
}