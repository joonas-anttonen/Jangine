#include "built-in-utilities.hlsl"

struct PerSceneData
{
	float4x4 ViewProjection;

	float4x4 View;
	float4x4 ViewInverse;
	
	float3 ViewPosition;

	float2 Screen;
};

struct PerMeshData
{
	float4x4 Transform;

	float4 _Color;
	float4 _ColorOuterStart;
	float4 _ColorInnerEnd;
	float4 _ColorOuterEnd;
	float _Radius;
	float _Thickness;
	float _AngleStart;
	float _AngleEnd;
	int _ScaleSpace;
	int _Alignment;
};

[[vk::binding(0, 0)]] ConstantBuffer<PerSceneData> perScene;
[[vk::binding(1, 0)]] ConstantBuffer<PerMeshData> perMesh;

struct vertex_input
{
	float3 Position : POSITION0;
	float2 UV : TEXCOORD0;
};

struct fragment_input
{
    float4 Position : SV_POSITION;
    float4 intp0 : TEXCOORD0;
	float4 intp1 : TEXCOORD1;
};

#define IP_uv0 intp0.xy
#define IP_pxCoverage intp0.z
#define IP_innerRadiusFraction intp0.w
#define IP_centerRadiusMeters intp1.x
#define IP_thicknessMeters intp1.y
#define IP_pxPerMeter intp1.z
#define IP_uniformScale intp1.w

/*inline void ApplyDashes( inout float mask, VertexOutput i, float t, float tRadial, float angularSpan )
{
	if( IsDashed() ) {
		float radiusMeters = i.IP_centerRadiusMeters;
		float dist = t * radiusMeters * TAU; // arc length in meters right now
		float distTotal = radiusMeters * angularSpan;
		DashConfig dash = GetDashConfig( i.IP_uniformScale );
		DashCoordinates dashCoords = GetDashCoordinates( dash, dist, distTotal, i.IP_thicknessMeters, i.IP_pxPerMeter );
		ApplyDashMask(mask, dashCoords, tRadial*2-1, dash.type, dash.modifier );
	}
}*/

inline void ApplyRadialMask(inout float mask, fragment_input i, out float tRadial)
{
    float len = length(i.IP_uv0);
    mask = min(mask, StepAA(len, 1)); // outer radius

	// TODO: Specialization constant
	bool hasInnerRadius = perMesh._Thickness > 0;
	if (hasInnerRadius)
	{
		mask = min(mask, 1.0 - StepAA(len, i.IP_innerRadiusFraction)); // inner radius
		tRadial = saturate(InverseLerp(i.IP_innerRadiusFraction, 1, len));
	}
	else
	{
	    tRadial = saturate(len);
	}
}

inline void ApplyAngularMask(inout float mask, float2 uv, out float tAngularFull, out float tAngular, out float sectorSize )
{
	float ang, angStart, angEnd;
	float2 coord;

	bool isSector = perMesh._AngleEnd > 0;
	if (isSector)
	{
		angStart = perMesh._AngleStart;
		angEnd = perMesh._AngleEnd;

		// Rotate so that the -pi/pi seam is opposite of the visible segment
		// 0 is the center of the segment post-rotate
		float angOffset = -(angEnd + angStart) * 0.5;
		coord = Rotate(uv, angOffset);
		angStart += angOffset;
		angEnd += angOffset;
	}
	else
	{
	    // required for angular gradients on rings and discs
		angStart = 0;
        angEnd = TAU;
        coord = -uv;
	}

	float angDelta = clamp(angEnd - angStart, -TAU, TAU);
	coord.y *= sign(angDelta); // since start/end is flipped for reversed situations
	ang = atan2(coord.y, coord.x); // -pi to pi

	sectorSize = abs(angDelta);
	tAngular = saturate(ang / sectorSize + 0.5); // angular interpolator for color
	
	if (isSector)
	{        
	    float segmentMask;

		// TODO: Specialization constant
		bool hasInnerRadius = perMesh._Thickness > 0;
		if (hasInnerRadius)
		{
			float2 pdCoordSpace = float2(-coord.y, coord.x) / dot(coord, coord);
			segmentMask = StepAAManualPD(coord, abs(ang), sectorSize * 0.5, pdCoordSpace);
		}
		
		// Adjust if close to 0 or TAU radians, fade in or out completely
		float THRESH_INVIS = 0.001;
		float THRESH_VIS = 0.002;
		float fadeInMask = saturate(InverseLerp(TAU - THRESH_VIS, TAU - THRESH_INVIS, sectorSize));
		float fadeOutMask = saturate(InverseLerp(THRESH_INVIS, THRESH_VIS, sectorSize));
		mask *= lerp(segmentMask * fadeOutMask, 1, fadeInMask);
	}
	
	// TODO: Specialization constant
	bool hasInnerRadius = perMesh._Thickness > 0;
	if (hasInnerRadius)
	{
	    tAngularFull = (ang + sectorSize / 2) / TAU;
	}
	else
	{
	    tAngularFull = 0;
	}
}

#define CAM_RIGHT      perScene.ViewInverse._m00_m10_m20
#define CAM_UP         perScene.ViewInverse._m01_m11_m21
#define CAM_FORWARD    perScene.ViewInverse._m02_m12_m22
#define OBJ_ORIGIN     perMesh.Transform._m03_m13_m23

float4 UnityObjectToClipPos(float3 pos)
{
	return mul(perScene.ViewProjection, mul(perMesh.Transform, float4(pos, 1.0)));
}

[shader("vertex")]
fragment_input vertex(vertex_input input)
{
    fragment_input output = (fragment_input)0;

	float uniformScale = GetUniformScale(perMesh.Transform);
	float radius = perMesh._Radius * uniformScale;
    ScaleData widthDataRadius = GetScreenSpaceWidthDataSimple(perScene.ViewProjection, perScene.Screen, OBJ_ORIGIN, CAM_RIGHT, radius * 2, perMesh._ScaleSpace);
    output.IP_pxCoverage = widthDataRadius.thicknessPixelsTarget;

	// padding correction
	float paddingMeters = 2 / widthDataRadius.pxPerMeter;
    float radiusInMeters = widthDataRadius.thicknessMeters / 2; // actually, center radius
	float vertexRadius;
	float outerRadiusFraction;

	// TODO: Specialization constant
	bool hasInnerRadius = perMesh._Thickness > 0;
	if (hasInnerRadius)
	{
        output.IP_pxPerMeter = widthDataRadius.pxPerMeter;
	    float thickness = perMesh._Thickness * uniformScale;
	    ScaleData widthDataThickness = GetScreenSpaceWidthDataSimple(perScene.ViewProjection, perScene.Screen, OBJ_ORIGIN, CAM_RIGHT, thickness, perMesh._ScaleSpace);
	    float thicknessRadius = widthDataThickness.thicknessMeters / 2;
	    output.IP_thicknessMeters = widthDataThickness.thicknessMeters;
	    output.IP_pxCoverage = widthDataThickness.thicknessPixelsTarget;
	    float radiusOuter = radiusInMeters + widthDataThickness.thicknessMeters / 2;
		vertexRadius = radiusOuter + paddingMeters;
		outerRadiusFraction = radiusOuter / vertexRadius;
	    output.IP_innerRadiusFraction = (radiusOuter - widthDataThickness.thicknessMeters) / radiusOuter;
	    output.IP_centerRadiusMeters = radiusInMeters;
	    output.IP_uniformScale = uniformScale;
	}
	else
	{
		vertexRadius = radiusInMeters + paddingMeters;
		outerRadiusFraction = radiusInMeters / vertexRadius;
	}
	
	input.Position.xy = input.UV * vertexRadius;
	input.UV /= outerRadiusFraction;
	output.IP_uv0 = input.UV;

	// TODO: Specialization constant
	bool isBillboard = perMesh._Alignment > 0;
	if (isBillboard) 
	{
		const float3 frw = -normalize(perScene.ViewPosition - OBJ_ORIGIN);
		const float3 right = normalize(cross(CAM_UP, frw));
		const float3 up = cross(frw, right); // already normalized
		const float3 rightLocal = WorldToLocalVec(perMesh.Transform, right);
		const float3 upLocal = WorldToLocalVec(perMesh.Transform, up);
		input.Position.xyz = input.Position.x * rightLocal + input.Position.y * upLocal;
		output.Position = UnityObjectToClipPos(input.Position); // scale already taken into account
	} 
	else 
	{
		output.Position = UnityObjectToClipPos(input.Position / uniformScale);	
	}

	return output;
}

[shader("pixel")]
float4 fragment(fragment_input input) : SV_TARGET
{ 
	float tRadial;
	float tAngular;
    float tAngularFull; // angular gradient 0 to 1, used for dashes
	float sectorSize; // used to snap dash coords
	
	float mask = 1;
	ApplyRadialMask(mask, input, tRadial);
	ApplyAngularMask(mask, input.IP_uv0, tAngularFull, tAngular, sectorSize);

	// TODO: Specialization constant
	bool hasInnerRadius = perMesh._Thickness > 0;
	if (hasInnerRadius)
	{
	    //ApplyDashes(mask, input, tAngularFull, tRadial, sectorSize);
	}
	mask *= saturate(input.IP_pxCoverage); // pixel fade

	// Compute color
	float4 colInnerStart = perMesh._Color;
    float4 colOuterStart = perMesh._ColorOuterStart;
    float4 colInnerEnd = perMesh._ColorInnerEnd;
    float4 colOuterEnd = perMesh._ColorOuterEnd;
	float4 colorStart = lerp(colInnerStart, colOuterStart, tRadial);
	float4 colorEnd = lerp(colInnerEnd, colOuterEnd, tRadial);
	float4 color = lerp(colorStart, colorEnd, tAngular);

	return float4(color.rgb, mask * color.a);
}