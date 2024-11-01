#include "quadtree.h"

#include "sablujo.h"
#include <new>

global_variable alignas(quadtree) uint8_t QuadPool[sizeof(quadtree) * (BOID_COUNT * 4)] = {};
global_variable quadtree* NextQuad = (quadtree*)QuadPool;

void ResetQuadtree()
{
    NextQuad = (quadtree*)QuadPool;
}

bool IsPointInRegion(AABB Region, vector2 Point)
{
    
    return Point.X >= Region.Min.X && Point.X < Region.Max.X &&
        Point.Y >= Region.Min.Y && Point.Y < Region.Max.Y;
}

bool AreRangesIntersecting(AABB A, AABB B)
{
    return (A.Min.X <= B.Max.X && A.Max.X >= B.Min.X) &&
    (A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y);
}

quadtree::quadtree(AABB Boundary_) : 
Boundary{Boundary_} 
{
};




void quadtree::Subdivide()
{
    vector2 HalfExtent = (Boundary.Max - Boundary.Min) / 2;
    vector2 MiddlePoint = Boundary.Max - HalfExtent;
    
    uint64_t UsedRegions = NextQuad - (quadtree*)QuadPool;
    Assert(UsedRegions < sizeof(quadtree) * (BOID_COUNT / MAX_BOID_PER_REGION));
    Regions[0] = new (NextQuad++) quadtree(AABB{Boundary.Min, MiddlePoint});
    
    vector2 NEMin = vector2{MiddlePoint.X, Boundary.Min.Y};
    Regions[1] = new (NextQuad++) quadtree(AABB{NEMin,NEMin + HalfExtent});
    
    vector2 SWMin = vector2{Boundary.Min.X, MiddlePoint.Y};
    Regions[2] = new (NextQuad++) quadtree(AABB{SWMin, SWMin + HalfExtent});
    
    Regions[3] = new (NextQuad++) quadtree(AABB{MiddlePoint, Boundary.Max});
    
    for(uint8_t i = 0; i < BoidCount;++i)
    {
        if(Regions[0]->Insert(Boids[i]))
        {
            continue;
        }
        if(Regions[1]->Insert(Boids[i]))
        {
            continue;
        }
        if(Regions[2]->Insert(Boids[i]))
        {
            continue;
        }
        if(Regions[3]->Insert(Boids[i]))
        {
            continue;
        }
        
        Assert(false);
        
    }
}

bool quadtree::Insert(boid* Boid)
{
    if(!IsPointInRegion(Boundary, Boid->Position))
    {
        return false;
    }
    
    bool hasSubregions = Regions[0] != nullptr;
    if(BoidCount < MAX_BOID_PER_REGION && !hasSubregions)
    {
        Boids[BoidCount++] = Boid;
        return true;
    }
    
    if(!hasSubregions)
    {
        Subdivide();
    }
    
    if(Regions[0]->Insert(Boid)) 
    {
        return true;
    }
    if(Regions[1]->Insert(Boid)) 
    {
        return true;
    }
    if(Regions[2]->Insert(Boid)) 
    {
        return true;
    }
    if(Regions[3]->Insert(Boid)) 
    {
        return true;
    }
    
    Assert(false);
    return false;
}

void quadtree::QueryRange(AABB Range, boid** BoidsInRange, uint32_t* FoundBoids)
{
    bool hasSubregions = Regions[0] != nullptr;
    if(!AreRangesIntersecting(Boundary, Range))
    {
        return;
    }
    
    if(!hasSubregions)
    {
        for(uint8_t i = 0; i < BoidCount;++i)
        {
            if(IsPointInRegion(Range, Boids[i]->Position))
            {
                BoidsInRange[(*FoundBoids)++] = Boids[i];
            }
        }
    }
    else
    {
        
        Regions[0]->QueryRange(Range, BoidsInRange, FoundBoids);
        Regions[1]->QueryRange(Range, BoidsInRange, FoundBoids);
        Regions[2]->QueryRange(Range, BoidsInRange, FoundBoids);
        Regions[3]->QueryRange(Range, BoidsInRange, FoundBoids);
    }
}