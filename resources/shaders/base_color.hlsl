struct FSInput
{
    float4 position : SV_POSITION;
    float3 normals : NORMAL;
};

FSInput VS(float4 position : POSITION, float3 normals : NORMAL)
{
    FSInput result;

    result.position = position;
    result.normals = normals;

    return result;
}

float4 FS(FSInput input) : SV_TARGET
{
    return float4(input.normals, 1.0);
}