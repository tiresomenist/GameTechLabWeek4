#pragma once

#include <filesystem>

#include "Engine/Resource/ObjInfo.h"

struct FObjImporter
{
    // OBJ와 참조된 MTL을 읽어 Raw 데이터를 반환한다.
    // 파일 열기 실패 또는 잘못된 입력은 예외로 전달한다.
    static FObjInfo Import(const std::filesystem::path& Path);
};