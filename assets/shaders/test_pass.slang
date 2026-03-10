struct VSInput {
    float3 inPosition : POSITION;
};

struct VSOutput {
    float4 pos : SV_Position;
};

struct TestData {
    float4 color1;
    float4 color2;
    float4 color3;
};

struct TestPushConstant {
    uint flag;
};

[[vk::binding(0, 0)]]
ConstantBuffer<TestData> testData;

[[vk::push_constant]]
ConstantBuffer<TestPushConstant> pushConstants;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.inPosition, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET {
    switch (pushConstants.flag % 3) {
    case 0:
        return testData.color1;
    case 1:
        return testData.color2;
    default:
        return testData.color3;
    }
}
