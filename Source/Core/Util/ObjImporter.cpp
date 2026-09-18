#include "pch.h"
#include "Core/Util/ObjImporter.h"
#include "Core/Util/File.h"
#include "Core/Container/Map.h"

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace
{
    //Parse 단계에서 에러가 터진 경우 호출
    [[noreturn]] void ParseError(const std::filesystem::path& Path, size_t LineNumber, FStringView Message)
    {
        // 오류 메시지의 경로도 UTF-8로 구성한다.
        const auto Utf8Path = Path.generic_u8string();
        const FString PathText(Utf8Path.begin(), Utf8Path.end());

        throw std::runtime_error(PathText + ":" + std::to_string(LineNumber) + ": " + FString(Message));
    }

    //공백 문자 여부
    bool IsSpace(char C)
    {
        return C == ' ' || C == '\t' || C == '\r' || C == '\v' || C == '\f';
    }


    //앞뒤 공백 문자 제거
    FStringView Trim(FStringView Text)
    {
        while (!Text.empty() && IsSpace(Text.front()))
        {
            Text.remove_prefix(1);
        }
        while (!Text.empty() && IsSpace(Text.back()))
        {
            Text.remove_suffix(1);
        }
        return Text;
    }

    // 원본 문자열을 참조하는 토큰을 반환한다.
    FStringView TakeToken(FStringView& Text)
    {
        while (!Text.empty() && IsSpace(Text.front()))
        {
            Text.remove_prefix(1);
        }
        if (Text.empty())
        {
            return {};
        }

        const char* Begin = Text.data();
        const char* Cursor = Begin;
        const char* End = Begin + Text.size();

        while (Cursor < End && !IsSpace(*Cursor))
        {
            ++Cursor;
        }

        const size_t Length = static_cast<size_t>(Cursor - Begin);
        Text.remove_prefix(Length);
        return FStringView(Begin, Length);
    }

    // 경로 토큰 하나를 복사 없이 읽고 Text를 전진시킨다. 큰따옴표로 묶인 경로의 형식도 검사한다.
    FStringView TakePathToken(FStringView& Text,const std::filesystem::path& Path,size_t LineNumber)
    {
        Text = Trim(Text);
        if (Text.empty() || Text.front() != '"')
        {
            return TakeToken(Text);
        }
        Text.remove_prefix(1);
        const size_t ClosingQuote = Text.find('"');
        if (ClosingQuote == FStringView::npos)
        {
            ParseError(Path, LineNumber, "Unclosed path quote");
        }

        const FStringView Result = Text.substr(0, ClosingQuote);
        Text.remove_prefix(ClosingQuote + 1);

        if (Result.empty() || (!Text.empty() && !IsSpace(Text.front())))
        {
            ParseError(Path, LineNumber, "Invalid quoted path");
        }

        return Result;
    }

    //obj파일의 각 라인을 읽는 함수
    template<typename Callback>
    void ForEachLine(FStringView Text, const std::filesystem::path& Path, Callback&& ProcessLine)
    {
        // UTF-8 BOM 건너뛰기.
        if (Text.size() >= 3
            && static_cast<unsigned char>(Text[0]) == 0xEF
            && static_cast<unsigned char>(Text[1]) == 0xBB
            && static_cast<unsigned char>(Text[2]) == 0xBF)
        {
            Text.remove_prefix(3);
        }

        if (Text.empty())
        {
            return;
        }

        const char* Cursor = Text.data();
        const char* End = Cursor + Text.size();
        size_t LineNumber = 0;

        while (Cursor < End)
        {
            ++LineNumber;
            const char* Begin = Cursor;

            //커서를 문장 끝까지 이동시킴
            while (Cursor < End && *Cursor != '\n')
            {
                ++Cursor;
            }

            //한줄 읽음
            FStringView Line(Begin,static_cast<size_t>(Cursor - Begin));

            //다음줄이 있으면 커서를 다음줄로 이동
            if (Cursor < End)
            {
                ++Cursor;
            }

            //주석 여부 확인
            const size_t Comment = Line.find('#');

            if (Comment != FStringView::npos)
            {
                Line = Line.substr(0, Comment);
            }

            // 앞뒤 공백 제거
            Line = Trim(Line);

            if (Line.empty())
            {
                continue;
            }

            // 줄이음은 지원하지 않음
            if (Line.back() == '\\')
            {
                ParseError(Path, LineNumber,"Line continuation is not supported");
            }
            // 예외처리 후, 등록한 ProcessLine 함수 호출
            ProcessLine(Line, LineNumber);
        }
    }

    // 들어온 값을 숫자 형식으로 전환
    template<typename T>
    T ReadNumber(FStringView Token, const std::filesystem::path& Path, size_t LineNumber)
    {
        // 비어있는 토큰 처리
        if (Token.empty())
        {
            ParseError(Path, LineNumber, "Missing number");
        }

        // from_chars가 받지 않는 선행 '+' 처리
        if (Token.front() == '+')
        {
            Token.remove_prefix(1);

            if (Token.empty() || Token.front() == '+' || Token.front() == '-')
            {
                ParseError(Path, LineNumber, "Invalid number");
            }
        }

        T Value{};
        const char* Begin = Token.data();
        const char* End = Begin + Token.size();

        //from_chars를 통해 토큰을 숫자로 전환
        const auto Result = std::from_chars(Begin, End, Value);

        //변환 실패
        if (Result.ec != std::errc{} || Result.ptr != End)
        {
            ParseError(Path, LineNumber, "Invalid number");
        }


        if constexpr (std::is_floating_point_v<T>)
        {
            //무한대 처리
            if (!std::isfinite(Value))
            {
                ParseError(Path, LineNumber, "Non-finite number");
            }
        }

        return Value;
    }

    // float 읽어오기
    float TakeFloat( FStringView& Text,const std::filesystem::path& Path, size_t LineNumber)
    {
        return ReadNumber<float>(TakeToken(Text), Path, LineNumber);
    }

    // 벡터 읽어오기
    FVector TakeVector3(FStringView& Text, const std::filesystem::path& Path, size_t LineNumber)
    {
        FVector Result;
        Result.X = TakeFloat(Text, Path, LineNumber);
        Result.Y = TakeFloat(Text, Path, LineNumber);
        Result.Z = TakeFloat(Text, Path, LineNumber);
        return Result;
    }

    // 필요한 값을 읽은 뒤 남은 내용이 공백뿐인지 검사하는 함수
    void RequireEnd(FStringView Text,const std::filesystem::path& Path,size_t LineNumber)
    {
        if (!Trim(Text).empty())
        {
            ParseError(Path, LineNumber, "Unexpected extra values");
        }
    }

    // obj의 1기반 인덱스 를 c++의 0기반 인덱스로 전환
    int32 ResolveIndex(FStringView Token, int32 Count, const std::filesystem::path& Path, size_t LineNumber)
    {
        const int32 ObjIndex = ReadNumber<int32>(Token, Path, LineNumber);

        const int64 Index = ObjIndex > 0 ? static_cast<int64>(ObjIndex) - 1: static_cast<int64>(Count) + ObjIndex;

        if (ObjIndex == 0 || Index < 0 || Index >= Count)
        {
            ParseError(Path, LineNumber, "OBJ index out of range");
        }

        return static_cast<int32>(Index);
    }

    //f 인덱스 집합 부분을 읽어 위치, uv, 법선 인덱스로 분리
    FObjVertexIndex ReadCorner(FStringView Token, const FObjInfo& Info, const std::filesystem::path& Path,
        size_t LineNumber)
    {
        FObjVertexIndex Result;
        const size_t Slash1 = Token.find('/');

        Result.PositionIndex = ResolveIndex(Token.substr(0, Slash1),Info.Positions.Num(), Path, LineNumber);

        if (Slash1 == FStringView::npos)
        {
            return Result; // v
        }

        const size_t Slash2 = Token.find('/', Slash1 + 1);

        if (Slash2 == FStringView::npos)
        {
            Result.UVIndex = ResolveIndex(Token.substr(Slash1 + 1), Info.TexCoords.Num(), Path, LineNumber);

            return Result; // v/vt
        }

        if (Token.find('/', Slash2 + 1) != FStringView::npos)
        {
            ParseError(Path, LineNumber, "Too many index separators");
        }

        const FStringView UVToken = Token.substr(Slash1 + 1, Slash2 - Slash1 - 1);

        if (!UVToken.empty())
        {
            Result.UVIndex = ResolveIndex(UVToken, Info.TexCoords.Num(), Path, LineNumber);
        }

        Result.NormalIndex = ResolveIndex(Token.substr(Slash2 + 1),Info.Normals.Num(), Path, LineNumber);

        return Result; // v//vn 또는 v/vt/vn
    }

    // 재질의 이름으로 마테리얼을 맵에 등록하거나 키값을 가져와서, 검색용 인덱스를 뱉는 함수
    int32 GetOrAddMaterial(FObjInfo& Info, TMap<FString, int32>& MaterialLookup, FStringView Name)
    {
        FString Key(Name);

        if (const int32* FoundIndex = MaterialLookup.Find(Key))
        {
            return *FoundIndex;
        }

        const int32 NewIndex = Info.Materials.Num();

        FObjMaterialInfo Material;
        Material.Name = std::move(Key);

        Info.Materials.Add(std::move(Material));

        MaterialLookup.Add(Info.Materials[NewIndex].Name, NewIndex);

        return NewIndex;
    }

    //파일 경로를 연결하는 함수
    std::filesystem::path ResolvePath(const std::filesystem::path& Parent,FStringView Name)
    {
        const std::filesystem::path Relative = std::filesystem::u8path(Name.begin(), Name.end());

        return (Parent / Relative).lexically_normal();
    }

    // MTL 파일을 읽어 재질 이름,Diffuse 색상,불투명도,텍스처 경로를 채움
    void ReadMtl(const std::filesystem::path& Path, FObjInfo& Info, TMap<FString, int32>& MaterialLookup)
    {
        const FString Text = File::ReadTextFromPath(Path);
        int32 CurrentMaterial = -1;

        ForEachLine(Text, Path,[&](FStringView Line, size_t LineNumber)
            {
                const FStringView Prefix = TakeToken(Line);

                if (Prefix == "newmtl")
                {
                    const FStringView Name = Trim(Line);

                    if (Name.empty())
                    {
                        ParseError(Path, LineNumber, "Missing material name");
                    }

                    CurrentMaterial = GetOrAddMaterial(Info, MaterialLookup,Name);

                    FObjMaterialInfo& Material = Info.Materials[CurrentMaterial];

                    // 같은 이름을 재정의하면 나중 정의 사용.
                    Material.DiffuseColor = { 1.0f, 1.0f, 1.0f };
                    Material.Opacity = 1.0f;
                    Material.DiffuseTexturePath.clear();
                    Material.bDefined = true;
                }
                else if (Prefix == "Kd" || Prefix == "map_Kd" || Prefix == "d" || Prefix == "Tr")
                {
                    if (CurrentMaterial < 0)
                    {
                        ParseError(Path, LineNumber,"Material property appears before newmtl");
                    }

                    FObjMaterialInfo& Material = Info.Materials[CurrentMaterial];

                    if (Prefix == "Kd")
                    {
                        Material.DiffuseColor = TakeVector3(Line, Path, LineNumber);

                        RequireEnd(Line, Path, LineNumber);
                    }
                    else if (Prefix == "map_Kd")
                    {
                        FStringView TextureName = Trim(Line);

                        if (TextureName.empty())
                        {
                            ParseError(Path, LineNumber,"Missing diffuse texture path");
                        }

                        if (TextureName.front() == '-')
                        {
                            ParseError(Path, LineNumber,"map_Kd options are not supported");
                        }

                        if (TextureName.front() == '"')
                        {
                            FStringView Remaining = TextureName;

                            TextureName = TakePathToken(Remaining, Path, LineNumber);

                            RequireEnd(Remaining, Path, LineNumber);
                        }

                        // 텍스처 경로는 MTL 파일 위치 기준.
                        Material.DiffuseTexturePath = ResolvePath(Path.parent_path(), TextureName);
                    }
                    else
                    {
                        const float Value = TakeFloat(Line, Path, LineNumber);

                        RequireEnd(Line, Path, LineNumber);

                        if (Value < 0.0f || Value > 1.0f)
                        {
                            ParseError(Path, LineNumber,"Opacity must be between 0 and 1");
                        }

                        Material.Opacity = Prefix == "Tr" ? 1.0f - Value : Value;
                    }
                }

                // Ka/Ks/Ns/illum 등은 현재 사용하지 않는다.
            });
    }
}
// OBJ와 참조 MTL을 읽고 면을 부채꼴로 삼각분할하여 FObjInfo를 반환하는 함수
FObjInfo FObjImporter::Import(const std::filesystem::path& Path)
{
    //리턴값
    FObjInfo Info;
    Info.PathFileName = Path.lexically_normal();
    
    //머티리얼 배열 인덱스
    TMap<FString, int32> MaterialLookup;

    //파일 전체 txt
    const FString Text = File::ReadTextFromPath(Path);

    // 현재 객체/머티리얼/스무딩 그룹.
    int32 CurrentObject = -1;
    int32 CurrentMaterial = -1;
    uint32 CurrentSmoothingGroup = 0;

    // 각 라인에 대해
    ForEachLine(Text, Path,
        [&](FStringView Line, size_t LineNumber)
        {
            const FStringView Prefix = TakeToken(Line);

            //정점 정보
            if (Prefix == "v")
            {
                float Values[6]{};
                int32 Count = 0;
                while (true)
                {
                    const FStringView Token = TakeToken(Line);
                    if (Token.empty()){ break; }
                    if (Count >= 6)
                    {
                        ParseError(Path, LineNumber,"Too many vertex values");
                    }
                    Values[Count] = ReadNumber<float>(Token, Path, LineNumber);
                    ++Count;
                }

                //정점 개수가 3,4,6이 아닐때
                if (Count != 3 && Count != 4 && Count != 6)
                {
                    ParseError(Path, LineNumber,"Expected xyz, xyzw, or xyzrgb");
                }

                const FVector Position{ Values[0], Values[1], Values[2] };
                FVector4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
                // W값 존재
                if (Count == 4)
                {
                    // 기존 w 처리 정책 유지.
                    if (Values[3] != 1.0f)
                    {
                        ParseError(Path, LineNumber, "Non-unit vertex weight is not supported");
                    }
                }
                //RGB값 존재
                else if (Count == 6)
                {
                    // RGB가 0~1임을 가정
                    for (int32 Index = 3; Index < 6; ++Index)
                    {
                        if (Values[Index] < 0.0f || Values[Index] > 1.0f)
                        {
                            ParseError(Path, LineNumber, "Vertex RGB must be between 0 and 1");
                        }
                    }

                    Color = FVector4{Values[3], Values[4], Values[5], 1.0f};
                }
                Info.Positions.Add(Position);
                Info.VertexColors.Add(Color);
            }

            //UV값
            else if (Prefix == "vt")
            {
                FVector UVW{};
                UVW.X = TakeFloat(Line, Path, LineNumber);
                const FStringView VToken = TakeToken(Line);
                if (!VToken.empty())
                {
                    UVW.Y = ReadNumber<float>(VToken, Path, LineNumber);
                }
                const FStringView WToken = TakeToken(Line);
                if (!WToken.empty())
                {
                    UVW.Z = ReadNumber<float>(WToken, Path, LineNumber);
                }
                RequireEnd(Line, Path, LineNumber);
                Info.TexCoords.Add(UVW);
            }
            //법선벡터값
            else if (Prefix == "vn")
            {
                const FVector Normal = TakeVector3(Line, Path, LineNumber);
                RequireEnd(Line, Path, LineNumber);
                Info.Normals.Add(Normal);
            }
            //각 꼭짓점의 참조를 읽는 부분
            else if (Prefix == "f")
            {
                // 별도 다각형 배열 없이 3개의 꼭짓점만 유지.
                const FObjVertexIndex First = ReadCorner(TakeToken(Line), Info, Path, LineNumber);
                FObjVertexIndex Previous = ReadCorner(TakeToken(Line), Info, Path, LineNumber);
                FObjVertexIndex Current = ReadCorner(TakeToken(Line), Info, Path, LineNumber);
                
                // o 선언 전에 나온 면은 이름 없는 기본 객체에 소속시킨다.
                if (CurrentObject < 0)
                {
                    FObjObjectInfo Object;
                    CurrentObject = Info.Objects.Num();
                    Info.Objects.Add(std::move(Object));
                }

                //현재는 볼록다각형까지만 처리가능, 오목다각형은 처리불가능
                while (true)
                {
                    FObjTriangle Triangle;
                    Triangle.Corners[0] = First;
                    Triangle.Corners[1] = Previous;
                    Triangle.Corners[2] = Current;
                    Triangle.ObjectIndex = CurrentObject;
                    Triangle.MaterialIndex = CurrentMaterial;
                    Triangle.SmoothingGroup = CurrentSmoothingGroup;
                    Info.Triangles.Add(Triangle);
                    const FStringView Next = TakeToken(Line);
                    if (Next.empty()) { break; }
                    Previous = Current;
                    Current = ReadCorner(Next, Info, Path, LineNumber);
                }
            }
            //객체 구분을 읽는 부분
            else if (Prefix == "o")
            {
                FObjObjectInfo Object;

                // Text가 소멸한 뒤에도 필요하므로 이름은 소유 복사.
                Object.Name = FString(Trim(Line));

                CurrentObject = Info.Objects.Num();
                Info.Objects.Add(std::move(Object));
            }

            //머티리얼을 읽는 부분
            else if (Prefix == "mtllib")
            {
                bool bHasLibrary = false;
                while (true)
                {
                    const FStringView LibraryName = TakePathToken(Line, Path, LineNumber);
                    if (LibraryName.empty()) { break; }

                    bHasLibrary = true;

                    // MTL 경로는 OBJ 파일 위치 기준.
                    const std::filesystem::path MtlPath = ResolvePath(Path.parent_path(), LibraryName);

                    ReadMtl(MtlPath, Info, MaterialLookup);
                }

                if (!bHasLibrary)
                {
                    ParseError(Path, LineNumber, "Missing MTL file name");
                }
            }
            else if (Prefix == "usemtl")
            {
                const FStringView Name = Trim(Line);

                if (Name.empty())
                {
                    ParseError(Path, LineNumber,"Missing material name");
                }

                CurrentMaterial = GetOrAddMaterial(Info, MaterialLookup,Name);
            }
            else if (Prefix == "s")
            {
                const FStringView Group = TakeToken(Line);

                if (Group == "off" || Group == "0")
                {
                    CurrentSmoothingGroup = 0;
                }
                else if (Group == "on")
                {
                    CurrentSmoothingGroup = 1;
                }
                else
                {
                    CurrentSmoothingGroup = ReadNumber<uint32>(Group, Path, LineNumber);
                }

                RequireEnd(Line, Path, LineNumber);
            }
            else if (Prefix == "p"|| Prefix == "l"|| Prefix == "curv"|| Prefix == "curv2" || Prefix == "surf")
            {
                ParseError(Path, LineNumber,"Only polygon faces are supported");
            }

            // g 그룹 이름은 현재 수집하지 않는다.
        });

    if (Info.Triangles.IsEmpty())
    {
        ParseError(Path, 0, "No polygon faces found");
    }

    for (const FObjMaterialInfo& Material : Info.Materials)
    {
        if (!Material.bDefined)
        {
            ParseError(Path, 0,"Undefined material: " + Material.Name);
        }
    }

    return Info;
}