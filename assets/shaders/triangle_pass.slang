struct VSInput {
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float3 color;
    [[vk::location(2)]] float2 uv;
};

struct VSOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float3 color;
};

struct TriangleUniformData {
    float4x4 transform;
};

struct TrianglePushConstant {
    float4 tint;
};

[[vk::binding(0, 0)]]
ConstantBuffer<TriangleUniformData> triangleUniforms;

[[vk::push_constant]]
ConstantBuffer<TrianglePushConstant> trianglePush;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.position =
        mul(triangleUniforms.transform, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
    return float4(input.color * trianglePush.tint.rgb, 1.0);
}
