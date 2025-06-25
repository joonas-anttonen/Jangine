struct fragment_input
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD0;
};

[[vk::binding(0, 0)]] Texture2D commandTexture;
[[vk::binding(1, 0)]] SamplerState commandSampler;

[shader("vertex")]
fragment_input vertex(in uint vertexIndex : SV_VertexID)
{
    fragment_input output = (fragment_input)0;
    output.UV = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    output.Position = float4(output.UV * 2.0f - 1.0f, 0.0f, 1.0f);
	return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{
	return commandTexture.Sample(commandSampler, input.UV);
}