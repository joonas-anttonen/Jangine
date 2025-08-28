struct PerSceneData
{
	float4x4 ViewProjection;

	float4x4 View;
	float4x4 ViewInverse;
	
	float3 ViewPosition;

	float2 Screen;
};

struct PerMaterialData
{
    float4 BaseColor;
	float Metalness;
	float Roughness;
};

struct PerMeshData
{
	float4x4 Transform;
    int MaterialIndex;
};

[[vk::binding(0, 0)]] ConstantBuffer<PerSceneData> perScene;
[[vk::binding(1, 0)]] ConstantBuffer<PerMeshData> perMesh;
[[vk::binding(2, 0)]] StructuredBuffer<PerMaterialData> perMaterial;

struct vertex_input
{
	float3 Position : POSITION0;
    float3 Normal : NORMAL0;
	float2 UV : TEXCOORD0;
};

struct fragment_input
{
	float4 Position : SV_POSITION;

	float3 Position_World : POSITION_WORLD;
	float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    
    int MaterialIndex : MATERIAL_INDEX;
};

[shader("vertex")]
fragment_input vertex(vertex_input input)
{
    fragment_input output;

    float4 worldPosition = mul(perMesh.Transform, float4(input.Position, 1.0));
    output.Position = mul(perScene.ViewProjection, worldPosition);
    output.UV = input.UV;
    output.Normal = normalize(mul(perMesh.Transform, float4(input.Normal, 0.0)).xyz);
    output.MaterialIndex = perMesh.MaterialIndex;

    return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{ 
    PerMaterialData material = perMaterial[input.MaterialIndex];

    return float4(material.BaseColor.rgb, 1.0);
}