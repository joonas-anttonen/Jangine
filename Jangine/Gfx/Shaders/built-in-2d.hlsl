struct PerCommand
{
	float2 Scale;
    bool Smoothing;
};

[[vk::push_constant]] PerCommand command;

[[vk::binding(0, 0)]] Texture2D commandTexture;
[[vk::binding(1, 0)]] SamplerState commandSampler;

struct vertex_input
{
	float2 Position : POSITION0;
	float2 UV : TEXCOORD0;
	float4 Color : COLOR0;
};

struct fragment_input
{
	float4 Position : SV_POSITION;
	float2 UV : TEXCOORD0;
	float4 Color : COLOR0;
};

[shader("vertex")]
fragment_input vertex(vertex_input input, in uint vertexIndex : SV_VertexID)
{
    fragment_input output = (fragment_input)0;
    output.Position = float4(input.Position * command.Scale + float2(-1, -1), 0.0, 1.0);
	output.UV = input.UV;
	output.Color = input.Color;
	return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{ 
	float4 geometryColor = input.Color;
	float4 textureColor = commandTexture.Sample(commandSampler, input.UV);

    float distanceFromOutline = textureColor.a - 0.5;
    float distanceChangePerFragment = 0.5 * fwidth(distanceFromOutline);
    float alpha = command.Smoothing 
				  ? (textureColor.a >= 0.5 ? 1 : smoothstep(-distanceChangePerFragment, +distanceChangePerFragment, distanceFromOutline))
				  : distanceFromOutline >= 0.0 ? 1.0 : 0.0;

    float3 rgb = geometryColor.rgb * textureColor.rgb;
    return float4(rgb, alpha);
}