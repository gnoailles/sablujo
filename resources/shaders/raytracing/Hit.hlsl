#include "Common.hlsl"

struct STriVertex
{ 
	float4 vertex; 
	float3 color;
};
StructuredBuffer<STriVertex> BTriVertex : register(t0);

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attribs) 
{
  float3 barycentrics = float3(1.0 - attribs.bary.x - attribs.bary.y, attribs.bary.x, attribs.bary.y);
  uint vertId = 3 * PrimitiveIndex();
  float3 hitColor = (BTriVertex[vertId + 0].color * barycentrics.x + 
					BTriVertex[vertId + 1].color * barycentrics.y + 
					BTriVertex[vertId + 2].color * barycentrics.z).xyz;
  payload.colorAndDistance = float4(hitColor, RayTCurrent());
}
