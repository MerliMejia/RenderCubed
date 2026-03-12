static const float PI = 3.14159265359;

struct VSInput {
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float2 uv;
};

struct VSOutput {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv;
};

struct PbrPushConstant {
    float4x4 invProj;
    float4 lightDirection;
    float4 lightColor;
};

[[vk::binding(0, 0)]] Sampler2D gbufferAlbedo;
[[vk::binding(1, 0)]] Sampler2D gbufferNormal;
[[vk::binding(2, 0)]] Sampler2D gbufferMaterial;
[[vk::binding(3, 0)]] Sampler2D gbufferEmissive;
[[vk::binding(4, 0)]] Sampler2D gbufferDepth;

[[vk::push_constant]]
ConstantBuffer<PbrPushConstant> pbrPush;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput o;
    o.position = float4(input.position, 1.0);
    o.uv = input.uv;
    return o;
}

float3 DecodeNormal(float3 encodedNormal) {
    return normalize(encodedNormal * 2.0 - 1.0);
}

float3 ReconstructViewPosition(float2 uv, float depth) {
    float2 ndcXY = uv * 2.0 - 1.0;
    float ndcZ = depth * 2.0 - 1.0;

    float4 clipPos = float4(ndcXY, ndcZ, 1.0);
    float4 viewPos = mul(pbrPush.invProj, clipPos);
    return viewPos.xyz / viewPos.w;
}

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-5);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / lerp(NdotV, 1.0, k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_TARGET {
    float depth = gbufferDepth.Sample(input.uv).r;
    if (depth >= 0.9999) {
        return float4(0.02, 0.02, 0.02, 1.0);
    }

    float4 albedoSample = gbufferAlbedo.Sample(input.uv);
    float3 albedo = albedoSample.rgb;
    float3 normal = DecodeNormal(gbufferNormal.Sample(input.uv).rgb);
    float3 material = gbufferMaterial.Sample(input.uv).rgb;
    float3 emissive = gbufferEmissive.Sample(input.uv).rgb;

    float metallic = saturate(material.r);
    float roughness = clamp(material.g, 0.045, 1.0);
    float ao = saturate(material.b);

    float3 viewPos = ReconstructViewPosition(input.uv, depth);
    float3 V = normalize(-viewPos);
    float3 L = normalize(-pbrPush.lightDirection.xyz);
    float3 H = normalize(V + L);
    float3 radiance = pbrPush.lightColor.rgb;

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, V, L, roughness);

    float NdotV = max(dot(normal, V), 0.0);
    float NdotL = max(dot(normal, L), 0.0);
    float3 numerator = NDF * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 1e-4);
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    float3 diffuse = kD * albedo / PI;

    float3 directLighting = (diffuse + specular) * radiance * NdotL;
    float3 ambient = 0.03 * albedo * ao;
    float3 color = ambient + directLighting + emissive;
    return float4(color, albedoSample.a);
}
