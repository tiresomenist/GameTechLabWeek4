#include "pch.h"
#include "ObjViewer.h"
#include <Windows.h>

void FObjViewer::Initialize()
{
    FEditor::Initialize();

    OutputDebugStringW(L"[ObjViewer] FObjViewer::Initialize completed.\n");
}