#ifndef QUADTREE_H
#define QUADTREE_H

#include "sablujo_maths.h"
struct AABB
{
    vector2 Min;
    vector2 Max;
};

// Regions are in this order
//  _____________
// |      |      |
// | 0 NW | 1 NE |
// |______|______|
// |      |      |
// | 2 SW | 3 NE |
// |______|______|

#define MAX_BOID_PER_REGION 128
struct boid;

struct quadtree
{
    AABB Boundary;
    quadtree* Regions[4] {};
    
    uint32_t BoidCount {0};
    //TODO Move these data out of the struct
    boid* Boids[MAX_BOID_PER_REGION];
    
    quadtree(AABB Boundary_);
    
    void Subdivide();
    bool Insert(boid* Boid);
    void QueryRange(AABB Range, boid** BoidsInRange, uint32_t* FoundBoids);
};

void ResetQuadtree();
#endif //QUADTREE_H