#define MAX_FRAGMENT_COUNT 10

struct fragment_input
{
	float4 Position : SV_POSITION;
};

struct OITNode
{
    float4 color;
    float depth;
    uint next;
};
[[vk::binding(0, 0)]] RWStructuredBuffer<OITNode> oitNodes;
[[vk::binding(1, 0)]] Texture2D<uint> headIndexImage;
[[vk::binding(2, 0)]] Texture2D depthImage;

[shader("vertex")]
fragment_input vertex(uint VertexIndex: SV_VertexID)
{
    fragment_input output;
    float2 UV = float2((VertexIndex << 1) & 2, VertexIndex & 2);
    output.Position = float4(UV * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{
    OITNode fragments[MAX_FRAGMENT_COUNT];
    int count = 0;

    uint2 pixelPosition = uint2(input.Position.xy);
    float pixelDepth = depthImage[pixelPosition].r;
    uint nodeIdx = headIndexImage[pixelPosition].r;

    while (nodeIdx != 0xffffffff && count < MAX_FRAGMENT_COUNT)
    {
        // Depth check - maybe ignore sample
        if (oitNodes[nodeIdx].depth > pixelDepth)
        {
            nodeIdx = oitNodes[nodeIdx].next;
            continue;
        }

        fragments[count] = oitNodes[nodeIdx];
        nodeIdx = fragments[count].next;
        ++count;
    }

    if (count == 0)
        discard;
    
    // Do the insertion sort
    for (uint i = 1; i < count; ++i)
    {
        OITNode insert = fragments[i];
        uint j = i;
        while (j > 0 && insert.depth > fragments[j - 1].depth)
        {
            fragments[j] = fragments[j - 1];
            --j;
        }
        fragments[j] = insert;
    }

    // Blend the fragments back-to-front
    float4 color = float4(0.0, 0.0, 0.0, 0.0f);
    for (uint f = 0; f < count; ++f)
    {
        float4 src = fragments[f].color;
        color.rgb = lerp(color.rgb, src.rgb, src.a);
        color.a = color.a + src.a * (1.0f - color.a);
    }

    return float4(color.rgb, color.a);
}