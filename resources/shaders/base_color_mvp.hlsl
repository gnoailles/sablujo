struct Model
{
	matrix Model;
};

struct ViewProjection
{
	matrix ViewProjection;
	matrix InverseView;
    matrix InverseProjection;
};
 
ConstantBuffer<Model> ModelCB : register(b0);
ConstantBuffer<ViewProjection> ViewProjectionCB : register(b1);

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

    Result.Position = mul(mul(ViewProjectionCB.ViewProjection, ModelCB.Model), Vertex.Position);
    Result.Normal = Vertex.Normal;

    return Result;
}

float4 FS(FSInput Input) : SV_TARGET
{
    return float4(Input.Normal, 1.0);
}