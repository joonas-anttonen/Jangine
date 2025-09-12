#include "built-in-utilities.hlsl"

struct PerSceneData
{
	float4x4 ViewProjection;

	float4x4 View;
	float4x4 ViewInverse;
	
	float3 ViewPosition;

	float2 Screen;
};

[[vk::binding(0, 0)]] ConstantBuffer<PerSceneData> perScene;

struct vertex_input
{
	float3 Position : POSITION0;

	float4x4 Transform : INSTANCE0;
	float3 Start : INSTANCE1;
	float3 End : INSTANCE2;
	uint32_t ColorStart : INSTANCE3;
	uint32_t ColorEnd : INSTANCE4;
	float Thickness : INSTANCE5;
	uint32_t ScaleSpace : INSTANCE6;
	uint32_t Alignment : INSTANCE7;
};

struct fragment_input
{
    float4 Position : SV_POSITION;
    float4 intp0 : TEXCOORD0;
	float4 intp1 : TEXCOORD1;
	float4 colorStart : TEXCOORD2;
	float4 colorEnd : TEXCOORD3;
};

#define IP_innerRadiusFraction intp0.w
#define IP_centerRadiusMeters intp1.x
#define IP_thicknessMeters intp1.y
#define IP_pxPerMeter intp1.z
#define IP_uniformScale intp1.w

#define IP_dash_coord intp0.x
#define IP_dash_spacePerPeriod intp0.y
#define IP_dash_thicknessPerPeriod intp0.z
#define IP_pxCoverage intp0.w
#define IP_uv0 intp1.xy
#define IP_tColor intp1.z

inline float4 WorldToClipPos(in float3 worldPos) {
    return mul(perScene.ViewProjection, float4(worldPos, 1));
}
inline float3 LocalToWorldPos(in float4x4 transform, in float3 localPos){
    return mul(transform, float4(localPos, 1)).xyz; 
}

[shader("vertex")]
fragment_input vertex(vertex_input input)
{
    fragment_input output = (fragment_input)0;
	output.colorStart = UnpackColor(input.ColorStart);
	output.colorEnd = UnpackColor(input.ColorEnd);

	float3 aLocal = input.Start;
    float3 bLocal = input.End;
    int alignment = input.Alignment;
    aLocal.z *= saturate(alignment); // flatten Z if _Alignment == ALIGNMENT_FLAT
    bLocal.z *= saturate(alignment);
	float3 a = LocalToWorldPos(input.Transform, aLocal);
	float3 b = LocalToWorldPos(input.Transform, bLocal);
	float3 vertOrigin = input.Position.y < 0 ? a : b;

	float lineLengthVisual;
	float3 tangent;
	GetDirMag(b - a, /*out*/ tangent, /*out*/ lineLengthVisual);

    float3 normal;
	if (alignment == 0) {
		float3 localZ = normalize(float3(input.Transform[0].z, input.Transform[1].z, input.Transform[2].z));
		normal = cross(tangent, localZ);
	} else {
		float3 camForward = -normalize(perScene.ViewPosition - input.Transform._m03_m13_m23);
		normal = normalize(cross(tangent, camForward));
	}

    int scaleMode = input.ScaleSpace;
    float uniformScale = GetUniformScale(input.Transform);
	float scaleThickness = input.ScaleSpace == 0 ? uniformScale : 1;

	float thickness = input.Thickness * scaleThickness;
	ScaleData widthData = GetScreenSpaceWidthData(perScene.ViewProjection, perScene.Screen, vertOrigin, normal, thickness, input.ScaleSpace);
	
	output.IP_uv0 = input.Position.xy;
	float verticalPaddingTotal = 2 / widthData.pxPerMeter;
	
	output.IP_uv0.x *= widthData.aaPaddingScale; // scale compensate width
	output.IP_uv0.y *= (lineLengthVisual + verticalPaddingTotal ) / lineLengthVisual; // scale compensate height
	
	output.IP_pxCoverage = widthData.thicknessPixelsTarget;
	float radiusVtx = widthData.thicknessMeters / 2;
	
	float3 vertPos = vertOrigin + normal * (input.Position.x * radiusVtx) + tangent * (input.Position.y * verticalPaddingTotal * 0.5);
	
    float radiusVisuals = 0.5*widthData.thicknessPixelsTarget / widthData.pxPerMeter;
    
	// dashes
	/*if( IsDashed() ) {
		float projDist = dot( tangent, vertPos - a ); // distance along line
		DashConfig dash = GetDashConfig(uniformScale);
		DashCoordinates dashCoords = GetDashCoordinates( dash, projDist, lineLengthVisual, 2*radiusVisuals, widthData.pxPerMeter );
		output.IP_dash_coord = dashCoords.coord;
		output.IP_dash_spacePerPeriod = dashCoords.spacePerPeriod;
		output.IP_dash_thicknessPerPeriod = dashCoords.thicknessPerPeriod;
	}*/
	
	// color
	output.IP_tColor = input.Position.y/2+0.5;

	output.Position = WorldToClipPos( vertPos.xyz );

	return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{ 
	float4 colorStart = input.colorStart;
    float4 colorEnd = input.colorEnd;
	float4 shape_color = lerp(colorStart, colorEnd, saturate(input.IP_tColor));

	float shape_mask = 1;
	
	// edge masking
	float maskEdges = GetLineLocalAA(input.IP_uv0.x, input.IP_pxCoverage);
	shape_mask = min(shape_mask, maskEdges);	
	shape_mask = min(shape_mask, GetLineLocalAA(input.IP_uv0.y, input.IP_pxCoverage));

    /*int dashType = PROP(_DashType);
	float dashModifier = PROP(_DashShapeModifier);
	DashCoordinates dashCoords;
	dashCoords.coord = input.IP_dash_coord;
	dashCoords.spacePerPeriod = input.IP_dash_spacePerPeriod;
	dashCoords.thicknessPerPeriod = input.IP_dash_thicknessPerPeriod;
	ApplyDashMask(shape_mask, dashCoords, input.IP_uv0.x, dashType, dashModifier);
	*/

	shape_mask *= saturate(input.IP_pxCoverage);
	return float4(shape_color.rgb, shape_mask * shape_color.a);
}