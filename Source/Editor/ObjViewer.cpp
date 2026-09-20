#include "pch.h"
#include "ObjViewer.h"
#include "Editor/Grid.h"
#include "Editor/Gizmo/WorldAxisGizmo.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Input/InputManager.h"
#include "ImGui/imgui.h"
#include "Core/Util/File.h"
#include "Engine/Log.h"
#include <cwchar>
#include <exception>
#include "Engine/Actor/Actor.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Resource/ResourceManager.h"
#include <cmath>
#include <stdexcept>

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
    // 파일 대화상자를 닫은 프레임에는 카메라 입력을 처리하지 않습니다.
    if (bOpenObjDialogRequested)
    {
        bOpenObjDialogRequested = false;
        OpenObjDialog();
        return;
    }
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
    PreviewActor = nullptr;

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

// 파일 선택 메뉴와 기존 표시 설정 메뉴를 구성합니다.
void FObjViewer::DrawMenu()
{
    MenuBarHeight = 0.0f;
    if (!ImGui::BeginMainMenuBar()) return;
    MenuBarHeight = ImGui::GetWindowHeight();

    // 대화상자는 다음 Tick에서 열고 현재 ImGui 프레임은 정상적으로 마무리합니다.
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open OBJ...")) { bOpenObjDialogRequested = true; }
        ImGui::EndMenu();
    }

    // 기존 표시 설정 메뉴를 유지합니다.
    if (ImGui::BeginMenu("View"))
    {
        bool bShowGrid = IsShowingGrid();
        if (ImGui::MenuItem("Grid", nullptr, &bShowGrid)) { SetShowGrid(bShowGrid); }

        bool bShowWorldAxis = IsShowingWorldAxis();
        if (ImGui::MenuItem("World Axis", nullptr, &bShowWorldAxis)){ SetShowWorldAxis(bShowWorldAxis); }

        ImGui::Separator();
        for (const FViewModeEntry& Entry : ViewModeEntries)
        {
            if (ImGui::MenuItem(Entry.Name, nullptr, GetViewMode() == Entry.Mode)) { SetViewMode(Entry.Mode); }
        }
        ImGui::EndMenu();
    }

    // 현재 선택 상태를 표시하며 파일명에 포함된 서식 문자를 해석하지 않습니다.
    ImGui::Separator();
    if (!FileSelectionError.empty())
    {
        ImGui::TextUnformatted(FileSelectionError.c_str());
    }
    else if (!SelectedObjName.empty())
    {
        ImGui::TextUnformatted(SelectedObjName.c_str());
    }
    else
    {
        ImGui::TextUnformatted("No OBJ selected");
    }

    ImGui::EndMainMenuBar();
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

// 전체 출력 영역에서 상단 메뉴가 차지하는 높이를 제외합니다.
D3D11_VIEWPORT FObjViewer::GetRenderViewport(const D3D11_VIEWPORT& FullViewport) const
{
    D3D11_VIEWPORT Viewport = FullViewport;

    // 작은 창에서도 뷰포트 높이가 음수가 되지 않도록 제한합니다.
    const float FullHeight = (std::max)(0.0f, FullViewport.Height);
    const float ReservedHeight = std::clamp(MenuBarHeight, 0.0f, FullHeight);
    Viewport.TopLeftY += ReservedHeight;
    Viewport.Height = FullHeight - ReservedHeight;
    return Viewport;
}

// OBJ 파일을 선택하고 로딩에 성공한 모델을 미리보기 씬에 연결합니다.
void FObjViewer::OpenObjDialog()
{
    GInputManager& Input = *GInputManager::GetInstance();
    const HWND Owner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);

    // 대화상자를 여는 동안 기존 입력 상태가 유지되지 않도록 초기화합니다.
    Input.KillFocus();
    FileSelectionError.clear();

    try
    {
        // 현재 모델의 폴더를 다음 파일 선택의 시작 위치로 사용합니다.
        const std::filesystem::path InitialDirectory = SelectedObjPath.empty()
            ? std::filesystem::path("Assets/Models")
            : SelectedObjPath.parent_path();
        const auto Path = File::OpenFileDialog(Owner, EFileDialogType::Obj, InitialDirectory);

        // 취소하면 기존 모델과 파일명을 유지합니다.
        if (Path)
        {
            const std::filesystem::path Candidate = std::filesystem::absolute(*Path).lexically_normal();
            if (_wcsicmp(Candidate.extension().c_str(), L".obj") != 0)
            {
                FileSelectionError = "Please select an OBJ file.";
            }
            else if (!std::filesystem::is_regular_file(Candidate))
            {
                FileSelectionError = "The selected file does not exist.";
            }
            else
            {
                // 파일 선택뿐 아니라 모델 구성까지 성공해야 현재 모델을 교체합니다.
                LoadPreviewMesh(Candidate);
            }
        }
    }
    catch (const std::exception& Error)
    {
        // 파싱이나 재질 로딩에 실패해도 Viewer와 기존 모델을 유지합니다.
        FileSelectionError = "Could not load the OBJ file.";
        UE_LOG("[ObjViewer] OBJ load failed: {}", Error.what());
    }

    // 대화상자 종료 시점에 들어온 입력을 비워 카메라 오작동을 방지합니다.
    Input.KillFocus();
}

// 기존 리소스 로더로 모델을 준비하고 성공한 경우에만 현재 모델을 교체합니다.
void FObjViewer::LoadPreviewMesh(const std::filesystem::path& FilePath)
{
    // 로딩과 표시 이름 준비를 기존 모델 삭제 전에 완료합니다.
    std::filesystem::path NewPath = std::filesystem::absolute(FilePath).lexically_normal();
    const auto Utf8Name = NewPath.filename().u8string();
    FString NewName(Utf8Name.begin(), Utf8Name.end());

    // 기존 로더가 사용하는 문자열 경로와 캐시 키를 그대로 사용합니다.
    const FString PathText = NewPath.string();
    const FName MeshKey(PathText);
    UStaticMesh* Mesh = GResourceManager::GetInstance()->GetOrLoadStaticMesh(MeshKey);
    if (!Mesh)
        throw std::runtime_error("Failed to load OBJ mesh.");

    // 로더가 반환했더라도 실제로 그릴 GPU 데이터가 있는지 확인합니다.
    const FMeshResource* Resource = Mesh->GetMeshResource();
    if (!Resource || !Resource->GetVertexBuffer() || !Resource->GetIndexBuffer()
        || Resource->GetIndexCount() == 0 || Mesh->GetSections().IsEmpty() || !Mesh->HasBounds())
        throw std::runtime_error("OBJ contains no renderable mesh.");

    // 원본 데이터는 유지하고 미리보기 컴포넌트의 위치와 크기만 정규화합니다.
    const FVector BoundsMin = Mesh->GetBoundsMin();
    const FVector BoundsMax = Mesh->GetBoundsMax();
    const FVector Center = BoundsMin * 0.5f + BoundsMax * 0.5f;
    const float Radius = (BoundsMax * 0.5f - BoundsMin * 0.5f).Length();
    if (!std::isfinite(Radius) || Radius <= 0.0f)
        throw std::runtime_error("OBJ mesh has invalid spatial extent.");

    const float PreviewScale = 5.0f / Radius;
    const FVector PreviewLocation = Center * -PreviewScale;
    if (!std::isfinite(PreviewScale) || !std::isfinite(PreviewLocation.X)
        || !std::isfinite(PreviewLocation.Y) || !std::isfinite(PreviewLocation.Z))
        throw std::runtime_error("OBJ preview transform is out of range.");

    // 새 Actor를 구성하는 동안 기존 모델은 그대로 유지합니다.
    AActor* NewActor = nullptr;
    try
    {
        NewActor = PreviewScene->SpawnActor<AActor*>(AActor::GetClass());
        auto* Component = static_cast<UStaticMeshComponent*>(
            NewActor->CreateComponent(UStaticMeshComponent::GetClass()));

        // 처음 로딩한 키를 그대로 전달하여 이미 생성된 메시를 재사용합니다.
        Component->SetStaticMesh(MeshKey);
        Component->SetRelativeScale3D(FVector(PreviewScale, PreviewScale, PreviewScale));
        Component->SetRelativeLocation(PreviewLocation);

        // 현재 SpawnActor는 실행 중인 씬에서도 BeginPlay를 자동 호출하지 않습니다.
        NewActor->BeginPlay();
    }
    catch (...)
    {
        // 새 Actor 구성에 실패하면 새 Actor만 제거합니다.
        if (NewActor) PreviewScene->DestroyActor(NewActor);
        throw;
    }

    // 준비가 끝난 뒤 기존 모델을 제거하고 표시 상태를 함께 교체합니다.
    if (PreviewActor) PreviewScene->DestroyActor(PreviewActor);
    PreviewActor = NewActor;
    SelectedObjPath.swap(NewPath);
    SelectedObjName.swap(NewName);
    FileSelectionError.clear();

    // 정규화한 모델 중심을 기본 카메라 위치에서 바라봅니다.
    EditorCamera->SetRelativeLocation(FVector(-15.0f, -15.0f, 10.0f));
    EditorCamera->LookAt(FVector(0.0f, 0.0f, 0.0f));
}