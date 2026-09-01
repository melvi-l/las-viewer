struct VertexInput {
    [[vk::location(0)]]
    int3 position;
    [[vk::location(1)]]
    uint4 color;
}
struct VertexOutput {
    float4 position   : SV_Position;
    [[vk::location(0)]]
    float4 color; 
    float  point_size : SV_PointSize;
};

struct FragmentInput {
    float4 position : SV_Position;
    [[vk::location(0)]]
    float4 color;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;

    float3 position = float3(input.position) / 32767.0;
    output.position = float4(position, 1.0);

    output.color = clamp(
        float4(input.color) / 255,
        0.0,
        1.0);

    output.point_size = 4.0;
    return output;
}


[shader("fragment")]
float4 fragmentMain(FragmentInput input) : SV_Target {
    return input.color;
}
