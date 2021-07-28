#include "sablujo_geometry.h"

void CreateSphere(uint32_t LatitudeCount, uint32_t LongitudeCount, 
                  vector4* OutputVertices, vector3* OutputNormals, uint32_t* OutputIndices,
                  uint32_t OutVerticesSize, uint32_t OutIndicesSize)
{
    Assert(OutVerticesSize >= LatitudeCount * LongitudeCount);
    Assert(OutIndicesSize >= LongitudeCount * 3 * 2 + (LatitudeCount - 1) * (LongitudeCount - 1) * 6);
    
    float Radius = 0.5f;
    uint32_t OutputOffset = 0;
    
    //North Cap
    OutputVertices[OutputOffset] = {0.0f, Radius, 0.0f, 1.0f};
    OutputNormals[OutputOffset] = {0.0f, 1.0f, 0.0f};
    ++OutputOffset;
    
    for (uint32_t Latitude = 0; Latitude < LatitudeCount; Latitude++)
    {
        const double Theta = (double)Latitude * PI / LatitudeCount;
        const double SinTheta = sin(Theta);
        const double CosTheta = cos(Theta);
        
        for (uint32_t Longitude = 0; Longitude < LongitudeCount; Longitude++)
        {
            Assert(OutputOffset < OutVerticesSize);
            const double Phi = (double)Longitude * 2 * PI / LongitudeCount;
            const double SinPhi = sin(Phi);
            const double CosPhi = cos(Phi);
            
            const float X = (float)(CosPhi * SinTheta);
            const float Z = (float)(SinPhi * SinTheta);
            
            OutputVertices[OutputOffset] = {X * Radius, (float)CosTheta * Radius, Z * Radius, 1.0f};
            OutputNormals[OutputOffset] = {X, (float)CosTheta, Z, 0.0f};
            ++OutputOffset;
        }
    }
    //South Cap
    OutputVertices[OutputOffset] = {0.0f, -Radius, 0.0f, 1.0f};
    OutputNormals[OutputOffset] = {0.0f, -1.0f, 0.0f};
    
    OutputOffset = 0;
    for (uint32_t Latitude = 0; Latitude < LatitudeCount; Latitude++)
    {
        for (uint32_t Longitude = 0; Longitude < LongitudeCount; Longitude++)
        {
            Assert(OutputOffset < OutIndicesSize);
            
            int32_t first = 1 + ((Latitude - 1) * LongitudeCount) + Longitude;
            int32_t second = first + LongitudeCount;
            
            if(Latitude == 0)
            {
                OutputIndices[OutputOffset++] = (1 + Longitude + 1) % LongitudeCount;
                OutputIndices[OutputOffset++] = 1 + Longitude;
                OutputIndices[OutputOffset++] = Latitude; // 0
            }
            else if(Latitude == LatitudeCount - 1)
            {
                OutputIndices[OutputOffset++] = first + 1;
                OutputIndices[OutputOffset++] = 1 + Latitude * LongitudeCount;
                OutputIndices[OutputOffset++] = first;
            }
            else
            {
                
                OutputIndices[OutputOffset++] = first + 1;
                OutputIndices[OutputOffset++] = second;
                OutputIndices[OutputOffset++] = first;
                
                OutputIndices[OutputOffset++] = first + 1;
                OutputIndices[OutputOffset++] = second + 1;
                OutputIndices[OutputOffset++] = second;
            }
        }
    }
    OutputOffset++;
}