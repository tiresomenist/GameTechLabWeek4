#include "pch.h"
#include "ViewportToolbarWindow.h"

#include "Editor/Editor.h"

void UViewportToolbarWindow::Render(float DeltaTime)
{
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(0.0f, 36.0f),
        ImVec2(FLT_MAX, 52.0f));

    constexpr ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::Begin(Name.c_str(), nullptr, Flags);
	{
        UCameraComponent* Camera = Editor->GetEditorCamera();

    	const char* ProjectionLabel = Camera->GetIsPerspective() ? "Perspective" : "Orthogonal";
        BeginPopupButton(ProjectionLabel, "ProjectionPopup", [this, Camera]()
            {
                DrawProjectionPopup(Camera);
            });
        ImGui::SameLine();
        
        const char* ViewModeLabel = "Unknown";
        for (const FViewModeEntry& Entry : ViewModeEntries)
        {
	        if (Entry.Mode == Editor->GetViewMode())
	        {
				ViewModeLabel = Entry.Name;
	        }
        }
        BeginPopupButton(ViewModeLabel, "ViewModePopup", [this]()
            {
                DrawViewModePopup();
            });
        ImGui::SameLine();

        BeginPopupButton("Show Flags", "ShowFlagsPopup", [this]()
            {
                DrawShowFlagsPopup();
            });

        ImGui::SameLine();
        if (ImGui::Button("HideStat"))
        {
            Editor->HideAllStats();
        }
        ImGui::SameLine();
        if (ImGui::Button("ShowStat"))
        {
            Editor->ShowAllStats();
        }
        
	}
	ImGui::End();
}

void UViewportToolbarWindow::BeginPopupButton(const char* ButtonName, const char* PopupName,
	const std::function<void()>& DrawFunction)
{
    if (ImGui::Button(ButtonName))
    {
        ImGui::OpenPopup(PopupName);
    }

    const ImVec2 ButtonMin = ImGui::GetItemRectMin();
    const ImVec2 ButtonMax = ImGui::GetItemRectMax();
    ImGui::SetNextWindowPos(ImVec2(ButtonMin.x, ButtonMax.y), ImGuiCond_Appearing);

    if (ImGui::BeginPopup(PopupName))
    {
        DrawFunction();
        ImGui::EndPopup();
    }
}

void UViewportToolbarWindow::DrawProjectionPopup(UCameraComponent* Camera)
{
    int ProjectionSelection = Camera->GetIsPerspective() ? 0 : 1;

    // if (지금 카메라가 perspective 카메라면) 아래로직 실행. 직교투영(탑, 프론트, 오른쪽)일때는 아예 버튼 없애기
    uint32 currViewIdx = Editor->GetCurrentEditViewportIndex();
    const TArray<FViewportClient>& Viewports = Editor->GetViewports();
    if (Viewports[currViewIdx].GetViewportType() != EViewportType::Perspective)
    {
        if (ImGui::RadioButton("Orthogonal", true))
        {
            Camera->SetIsPerspective(false);
        }
    }
    else
    {
	    const char* ProjectionNames[] = { "Perspective", "Orthogonal" };
    	for (int i = 0; i < 2; ++i)
    	{
    		if (ImGui::RadioButton(ProjectionNames[i], ProjectionSelection == i))
    		{
    			ProjectionSelection = i;
    			ImGui::CloseCurrentPopup();
    		}
    	}
    	Camera->SetIsPerspective(ProjectionSelection == 0);
    }

    // TODO: Orthographic 방향에 따라서 옵션 추가 분리

    ImGui::Separator();

    ImGui::PushItemWidth(100.0f);

    float CameraMoveSpeed = Camera->GetMoveSpeed();
    if (ImGui::SliderFloat("Move Speed", &CameraMoveSpeed, 10.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
    {
        Camera->SetMoveSpeed(std::clamp(CameraMoveSpeed, 0.1f, 100.0f));
    }

	float FOV = Camera->GetFOV() * 180.0f / PI;
    if (ImGui::SliderFloat("Field of View", &FOV, 5.0f, 170.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
    {
        Editor->SetCameraFOV(FOV);
    }

    ImGui::PopItemWidth();

    ImGui::Separator();

    FVector CameraLocation = Editor->GetCameraLocation();
    static bool bEditingCameraRotation = false;
    static FVector CameraRotationDegree;
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2f); // Item 너비 설정
    ImGui::DragFloat("##cameraX", &CameraLocation.X, 0.1f);
    DrawItemBottomLine(IM_COL32(210, 15, 57, 255), 2.0f);
    ImGui::SameLine();
    ImGui::DragFloat("##cameraY", &CameraLocation.Y, 0.1f);
    DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
    ImGui::SameLine();
    ImGui::DragFloat("##cameraZ", &CameraLocation.Z, 0.1f);
    DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);
    ImGui::SameLine();
    ImGui::Text("Camera Location");
    bool bRotationChanged = false;
    bool bRotationActive = false;
    bool bRotationFinished = false;
    constexpr ImGuiSliderFlags PitchFlags = ImGuiSliderFlags_AlwaysClamp;
    constexpr ImGuiSliderFlags YawFlags = ImGuiSliderFlags_WrapAround | ImGuiSliderFlags_AlwaysClamp;

    // ConstrainEditorRotation()이 Roll을 제거하므로 수정할 수 없는 값으로 표시한다.
    if (!bEditingCameraRotation)
    {
        CameraRotationDegree = Editor->GetCameraRotationDegree();
    }
    ImGui::BeginDisabled();
    ImGui::DragFloat("##cameraRX", &CameraRotationDegree.X, 0.1f, -180.0f, 180.0f, "%.3f");
    ImGui::EndDisabled();
    bRotationActive |= ImGui::IsItemActive();
    bRotationFinished |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SameLine();
    bRotationChanged |= ImGui::DragFloat("##cameraRY", &CameraRotationDegree.Y, 0.1f, -89.0f, 89.0f, "%.3f", PitchFlags);
    bRotationActive |= ImGui::IsItemActive();
    bRotationFinished |= ImGui::IsItemDeactivatedAfterEdit();
    DrawItemBottomLine(IM_COL32(64, 160, 43, 255), 2.0f);
    ImGui::SameLine();
    bRotationChanged |= ImGui::DragFloat("##cameraRZ", &CameraRotationDegree.Z, 0.1f, 0.0f, 0.0f, "%.3f", YawFlags);
    bRotationActive |= ImGui::IsItemActive();
    bRotationFinished |= ImGui::IsItemDeactivatedAfterEdit();
    DrawItemBottomLine(IM_COL32(30, 102, 245, 255), 2.0f);
    ImGui::SameLine();
    ImGui::Text("Camera Rotation");
    if (bRotationChanged || bRotationFinished)
    {
        CameraRotationDegree.X = 0.0f;

        // 카메라 Pitch를 도 단위로 제한함
        CameraRotationDegree.Y = std::clamp(CameraRotationDegree.Y, -89.0f, 89.0f);

        // 도 단위 입력값을 FRotator Setter로 전달함
        Editor->SetCameraRotationDegree(CameraRotationDegree);
    }
    bEditingCameraRotation = bRotationActive;
    ImGui::PopItemWidth();
    Editor->SetCameraLocation(CameraLocation);

    ImGui::Separator();

    ImGui::PushItemWidth(100.0f);
    float GridInterval = Editor->GetGrid().Interval;
    if (ImGui::SliderFloat("Grid Spacing", &GridInterval, FGrid::MinInterval, FGrid::MaxInterval, "%.2f", ImGuiSliderFlags_AlwaysClamp))
    {
        Editor->SetGridInterval(GridInterval);
    }

    ImGui::PopItemWidth();
}

void UViewportToolbarWindow::DrawViewModePopup()
{
    for (const FViewModeEntry& Entry : ViewModeEntries)
    {
        if (ImGui::RadioButton(Entry.Name, Entry.Mode == Editor->GetViewMode()))
        {
            Editor->SetViewMode(Entry.Mode);
            ImGui::CloseCurrentPopup();
        }
    }
}

void UViewportToolbarWindow::DrawShowFlagsPopup()
{
    bool bShowUUIDLabels = Editor->IsShowingUUIDLabels();
    if (ImGui::Checkbox("UUID", &bShowUUIDLabels))
    {
        Editor->SetShowUUIDLabels(bShowUUIDLabels);
    }
    bool bShowBoundingBoxes = Editor->IsShowingBoundingBoxes();
    if (ImGui::Checkbox("Bounding Boxes", &bShowBoundingBoxes))
    {
        Editor->SetShowBoundingBoxes(bShowBoundingBoxes);
    }
    bool bShowPrimitives = Editor->IsShowingPrimitives();
    if (ImGui::Checkbox("Primitives", &bShowPrimitives))
    {
        Editor->SetShowPrimitives(bShowPrimitives);
    }
    bool bShowGrid = Editor->IsShowingGrid();
    if (ImGui::Checkbox("Grid", &bShowGrid))
    {
        Editor->SetShowGrid(bShowGrid);
    }
    bool bShowWorldAxis = Editor->IsShowingWorldAxis();
    if (ImGui::Checkbox("World Axis", &bShowWorldAxis))
    {
        Editor->SetShowWorldAxis(bShowWorldAxis);
    }
}
