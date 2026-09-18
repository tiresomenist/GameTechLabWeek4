#pragma once

#include <concepts>

#include "Object.h"
#include "ObjectStatics.h"
#include "Core/Container/Array.h"

class UObject;

// 엔진 전체에 있는 T 타입의 오브젝트들을 순회하는 반복자
// 순서 보장 없음
// 생성 시점의 스냅샷을 순회함
template <typename T> requires std::derived_from<T, UObject>
class TObjectIterator final
{
public:
	explicit TObjectIterator(bool bIncludeDerivedClasses = true);

	void operator++() { Advance(); }
	explicit operator bool() const { return Index >= 0 && Index < ObjectArray.Num(); }
	bool operator!() const { return !static_cast<bool>(*this); }
	T* operator*() const { return static_cast<T*>(ObjectArray[Index]); }
	T* operator->() const { return static_cast<T*>(ObjectArray[Index]); }
	bool operator==(const TObjectIterator& Other) const { return ObjectArray == Other.ObjectArray && Index == Other.Index; }
	bool operator!=(const TObjectIterator& Other) const { return !(*this == Other); }

private:
	bool Advance();

protected:
	TArray<UObject*> ObjectArray;
	int32 Index;
};

template <typename T> requires std::derived_from<T, UObject>
TObjectIterator<T>::TObjectIterator(bool bIncludeDerivedClasses) : Index(-1)
{
	ObjectArray = GObjectStatics::GetObjectsOfClass(T::GetClass(), bIncludeDerivedClasses);
	Advance();
}

template <typename T> requires std::derived_from<T, UObject>
bool TObjectIterator<T>::Advance()
{
	while (++Index < ObjectArray.Num())
	{
		if (ObjectArray[Index] != nullptr)
		{
			return true;
		}
	}
	return false;
}
