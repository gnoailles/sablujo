struct FSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

FSInput VS(float4 position : POSITION, float4 color : COLOR)
{
    FSInput result;

    result.position = position;
    result.color = color;

    return result;
}

float4 FS(FSInput input) : SV_TARGET
{
    return input.color;
}