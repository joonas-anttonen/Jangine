
struct PerCommand
{
	float scale;
    float strength;
};

[[vk::push_constant]] PerCommand command;
[[vk::constant_id(0)]] const int direction = 0;

[[vk::binding(0, 0)]] Texture2D commandTexture;
[[vk::binding(1, 0)]] SamplerState commandSampler;

[[vk::binding(2, 0)]] Texture2D noiseTexture;
[[vk::binding(3, 0)]] SamplerState noiseSampler;

struct fragment_input
{
	float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

[shader("vertex")]
fragment_input vertex(uint VertexIndex: SV_VertexID)
{
    fragment_input output;
    output.UV = float2((VertexIndex << 1) & 2, VertexIndex & 2);
    output.Position = float4(output.UV * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{
	float weight[5];
	weight[0] = 0.227027;
	weight[1] = 0.1945946;
	weight[2] = 0.1216216;
	weight[3] = 0.054054;
	weight[4] = 0.016216;

    float scale = command.scale * 5.0; // just playing

	float2 textureSize;
	commandTexture.GetDimensions(textureSize.x, textureSize.y);
	float2 tex_offset = 1.0 / textureSize * scale; // gets size of single texel
	float4 result = commandTexture.Sample(commandSampler, input.UV).rgba * weight[0]; // current fragment's contribution

    float totalWeight = weight[0];

	for(int i = 1; i < 5; ++i)
	{
        float w = weight[i] * command.strength;
        totalWeight += w * 2.0;

		if (direction == 1)
		{
			// H
			result += commandTexture.Sample(commandSampler, input.UV + float2(tex_offset.x * i, 0.0)).rgba * w;
			result += commandTexture.Sample(commandSampler, input.UV - float2(tex_offset.x * i, 0.0)).rgba * w;
		}
		else
		{
			// V
			result += commandTexture.Sample(commandSampler, input.UV + float2(0.0, tex_offset.y * i)).rgba * w;
			result += commandTexture.Sample(commandSampler, input.UV - float2(0.0, tex_offset.y * i)).rgba * w;
		}
	}

	float4 color = result / totalWeight;

    if (direction == 1)
    {
        // Apply a subtle blue tint during the horizontal pass
        float4 tintColor = float4(0.13725491, 0.15294118, 0.19215687, 0.8);
        color = lerp(color, float4(tintColor.rgb, color.a), tintColor.a);

        // Sample noise and average all 4 channels
        float4 noiseSample = noiseTexture.Sample(noiseSampler, input.UV * (4.0));
        float noise = (noiseSample.r + noiseSample.g + noiseSample.b + noiseSample.a) * 0.25;

        // Soften noise using smoothstep
        float noiseAmount = 0.04;
        float smoothNoise = smoothstep(0.1, 0.2, noise); // Adjust thresholds for desired softness

        //color.rgb += (smoothNoise - 0.5) * noiseAmount;
        //color.a += (smoothNoise - 0.5) * noiseAmount;
    }

    return color;
}