#pragma once

#include <filesystem>

#include "Engine/Resource/ObjInfo.h"
#include "Engine/Resource/StaticMesh.h"

struct FObjImporter
{
    // OBJ와 참조된 MTL을 읽어 Raw 데이터를 반환한다.
    // 파일 열기 실패 또는 잘못된 입력은 예외로 전달한다.
    static FObjInfo Import(const std::filesystem::path& Path);

    // Raw 데이터를 조립하고 Device에 GPU 버퍼를 생성한다. 실패 시 예외를 전달한다.
    // 위치·법선 인덱스와 위치별 색상이 유효하고, 법선 생성은 완료된 입력을 받는다.
    // UV 인덱스의 -1은 기본 UV(0, 0)으로 변환한다.
    // Materials는 Info.Materials와 같은 순서의 비소유 포인터 배열이다.
    // 재질 미지정 면이 있으면 배열 끝에 기본 재질을 하나 더 전달한다.
    // 모든 재질은 유효해야 하며 반환된 메시를 사용하는 동안 살아 있어야 한다.
    static FStaticMesh Cook(const FObjInfo& Info, ID3D11Device* Device, const TArray<FMaterial*>& Materials);

};
