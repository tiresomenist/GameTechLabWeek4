#pragma once

#include "EditorWindow.h"
#include "Engine/Object/ClassType.h"
#include "Core/Math/Matrix.h"
#include "Core/Container/String.h"
#include "Core/Math/Quaternion.h"
#include "Core/Name/Name.h"
class FEditor;

class USceneWindow : public UEditorWindow
{
    UCLASS(USceneWindow, "SceneWindow", UEditorWindow)

private:
	uint32 NumberOfSpawn = 1;
	uint32 Step = 1;
	FString SceneName{"NewScene"};
	FName SelectedMeshKey;

	TArray<FClassType*> SpecialComponentClasses;
	FClassType* SelectedSpecialComponentClass = nullptr;
	/*             */
	// ImGui 창 그리기 도중이 아니라 End() 뒤에 대화상자를 띄우기 위한 플래그
	bool bRequestLoadDialog = false;
public:
	void SpawnStaticMesh();
	void SpawnSpecialComponent();
	void SpawnEmptyActor();
	void NewScene();
	void SaveScene();
	void LoadScene();

	virtual void InitializeWindow(FEditor* InEditor, const FString& Name) override;
	void Render(float DeltaTime) override;


};
