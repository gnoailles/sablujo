#ifndef SABLUJO_MEMORY_H
#define SABLUJO_MEMORY_H

using byte = uint8_t;

struct memory_area
{
    byte* Memory;
    uint32_t Size;
    byte* FreeArea;
};

template<typename T>
T* Allocate(memory_area* Area)
{
    Assert(Area->FreeArea - Area->Memory + sizeof(T) <= Area->Size);
    T* NewMemory = (T*)Area->FreeArea;
    Area->FreeArea += sizeof(T);
    *NewMemory = {};
    return NewMemory;
}

template<typename T>
T* AllocateUninitialized(memory_area* Area)
{
    Assert(Area->FreeArea - Area->Memory + sizeof(T) <= Area->Size);
    T* NewMemory = (T*)Area->FreeArea;
    Area->FreeArea += sizeof(T);
    return NewMemory;
}

inline memory_area GetSubArea(memory_area* Area, uint32_t Size)
{
    Assert(Area->FreeArea - Area->Memory + Size <= Area->Size);
    
    memory_area NewArea = {};
    NewArea.Memory = Area->FreeArea;
    NewArea.FreeArea = NewArea.Memory;
    NewArea.Size = Size;
    
    Area->FreeArea += Size;
    return NewArea;
}

template<typename T>
struct handle
{
    uint32_t Index;
    //uint32_t Generation;
};

template <typename T, typename H>
struct pool
{
    T* Data;
    uint32_t MaxCount;
    uint32_t Count;
};

template <typename T, typename H>
pool<T, H> AllocatePool(memory_area* Area, uint32_t PoolCount)
{
    Assert(Area->FreeArea - Area->Memory + (PoolCount * sizeof(T)) <= Area->Size);
    
    pool<T, H> Pool = {};
    Pool.Data = (T*)Area->FreeArea;
    Pool.MaxCount = PoolCount;
    
    Area->FreeArea += PoolCount * sizeof(T);
    return Pool;
}


template <typename T, typename H>
handle<H> AllocatePoolInstance(pool<T, H>* Pool)
{
    Assert(Pool->Count < Pool->MaxCount);
    handle<H> Result;
    Result.Index = Pool->Count++;
    return Result;
}

template <typename T, typename H>
T* GetPoolInstance(pool<T, H>* Pool, handle<H> Handle)
{
    Assert(Pool->Count > Handle.Index);
    return (T*)Pool->Data + Handle.Index;
}

#endif //SABLUJO_MEMORY_H
