struct VertexOutput {
    float4 position   : SV_Position;
    float3 color      : COLOR0;
    float  point_size : SV_PointSize;
};

struct FragmentInput {
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID) {
    uint width = 32;

    uint x = vertexID % width;
    uint y = vertexID / width;

    float2 pixel_pos = float2(x, y);

    float2 norm_pos =
        pixel_pos / float(width - 1);

    float2 ndc_pos =
        norm_pos * 2.0 - 1.0;

    VertexOutput output;
    output.position = float4(ndc_pos, 0.0, 1.0);
    output.color = float3(
        1.0 - norm_pos.x,
        norm_pos.x,
        norm_pos.y
    );
    output.point_size = 4.0f;

    return output;
}

[shader("fragment")]
float4 fragmentMain(FragmentInput input) : SV_Target0 {
    return float4(input.color, 1.0);
}
