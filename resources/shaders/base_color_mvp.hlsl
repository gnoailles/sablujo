struct ModelViewProjection
{
    matrix Model;
	matrix ViewProjection;
};
 
ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexInput
{
    float4 Position : POSITION;
    float3 Normal   : NORMAL;
};

struct FSInput
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
};

FSInput VS(VertexInput Vertex)
{
    FSInput Result;

    Result.Position = mul(mul(ModelViewProjectionCB.Model, ModelViewProjectionCB.ViewProjection), Vertex.Position);
    Result.Normal = Vertex.Normal;

    return Result;
}

float4 FS(FSInput Input) : SV_TARGET
{
    return float4(Input.Normal, 1.0);
}