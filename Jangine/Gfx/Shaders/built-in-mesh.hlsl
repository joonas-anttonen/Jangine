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
    float4 Color;
    int MaterialIndex;
};

[[vk::binding(0, 0)]] ConstantBuffer<PerSceneData> perScene;
[[vk::binding(1, 0)]] ConstantBuffer<PerMeshData> perMesh;
[[vk::binding(2, 0)]] StructuredBuffer<PerMaterialData> perMaterial;

// --------------------- OIT

struct OITData
{
    uint count;
    uint maxNodeCount;
};
struct OITNode
{
    uint color;
    float depth;
    uint next;
};
[[vk::binding(3, 0)]] RWStructuredBuffer<OITData> oitData;
[[vk::binding(4, 0)]] RWStructuredBuffer<OITNode> oitNodes;
[[vk::binding(5, 0)]] RWTexture2D<uint> oitNodeHeadImage;

uint PackColor(float4 color)
{
    uint r = (uint)(saturate(color.r) * 255.0f + 0.5f);
    uint g = (uint)(saturate(color.g) * 255.0f + 0.5f);
    uint b = (uint)(saturate(color.b) * 255.0f + 0.5f);
    uint a = (uint)(saturate(color.a) * 255.0f + 0.5f);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

// ---------------------

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
    output.Position_World = worldPosition.xyz;
    output.UV = input.UV;
    output.Normal = normalize(mul(perMesh.Transform, float4(input.Normal, 0.0)).xyz);
    output.MaterialIndex = perMesh.MaterialIndex;

    return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{ 
    PerMaterialData material = perMaterial[max(0, input.MaterialIndex)];
    if (input.MaterialIndex < 0)
    {
        material.BaseColor = perMesh.Color;
    }

    float3 ambient = float3(0.22, 0.22, 0.2);

    // Simple lighting where light comes from the view direction
    float3 L = normalize(perScene.ViewPosition - input.Position_World);
    float NdotL = min(max(dot(input.Normal, L), 0.2), 0.8);

    float3 diffuse = material.BaseColor.rgb * NdotL * 2;
    float3 combined = saturate(ambient + diffuse);

    // --------------------- OIT
    if (material.BaseColor.a < 1.0f)
    {
        // Increase the node count
        uint nodeIdx;
        InterlockedAdd(oitData[0].count, 1, nodeIdx);

        // Check LinkedListSBO is full
        if (nodeIdx < oitData[0].maxNodeCount)
        {
            // Exchange new head index and previous head index
            uint prevHeadIdx;
            InterlockedExchange(oitNodeHeadImage[uint2(input.Position.xy)], nodeIdx, prevHeadIdx);

            // Store node data
            oitNodes[nodeIdx].color = PackColor(float4(combined, material.BaseColor.a));
            oitNodes[nodeIdx].depth = input.Position.z;
            oitNodes[nodeIdx].next = prevHeadIdx;
        }

        discard;
    }
    // ---------------------

    return float4(combined, 1.0);
}