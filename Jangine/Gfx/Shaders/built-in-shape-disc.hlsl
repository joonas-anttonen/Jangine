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

#define TAU 6.28318530718
#define VERY_SMOL 0.00006103515625 // smallest positive normal number for float precision floats

#define IP_uv0 intp0.xy
#define IP_pxCoverage intp0.z
#define IP_innerRadiusFraction intp0.w
#define IP_centerRadiusMeters intp1.x
#define IP_thicknessMeters intp1.y
#define IP_pxPerMeter intp1.z
#define IP_uniformScale intp1.w

inline float PD( float value )
{
	float2 pd = float2(ddx(value), ddy(value));
	return sqrt( dot( pd, pd ) );
}

inline float PD( float2 value )
{
	float2 pd = fwidth(value);
	return sqrt( dot( pd, pd ) );
}

inline float StepThresholdPD( float value, float pd ) {
    return saturate( value / max( 0.00001, pd ) + 0.5 ); // sooooooooooooo this is complicated, whether it should be +0 or +.5
}
inline float StepThresholdPDAAOffset( float value, float pd, float aaOffset ) {
    return saturate( value / max( 0.00001, pd ) + aaOffset );
}
inline float StepAAExplicitPD( float value, float pdValue ){
    return StepThresholdPD( value, PD( pdValue ) );
}
float StepAAManualPD( float2 coords, float sdf, float thresh, float2 pdCoordSpace ){
	float2 pdScreenSpace = pdCoordSpace * PD( coords ); // Transform uv to screen space (does not support rotations, I think)
	float pdMag = length( pdScreenSpace ); // Get the magnitude of change
	float sub = sdf - thresh;
	return 1.0-saturate( sub / pdMag );
}
inline float StepAA( float value ) {
    return StepAAExplicitPD( value, value );
}
inline float StepAA( float thresh, float value ){
	return StepAA( value - thresh );
}

float3 GetObjectScale()
{
    float3x3 m = (float3x3)perMesh.Transform;
    return float3(
        length( float3( m[0][0], m[1][0], m[2][0] ) ),
        length( float3( m[0][1], m[1][1], m[2][1] ) ),
        length( float3( m[0][2], m[1][2], m[2][2] ) )
    );
}
float2 GetObjectScaleXY()
{
    float3x3 m = (float3x3)perMesh.Transform;
    return float2(
        length( float3( m[0][0], m[1][0], m[2][0] ) ),
        length( float3( m[0][1], m[1][1], m[2][1] ) )
    );
}
float GetUniformScale(float3 s)
{
    return ( s.x + s.y + s.z ) / 3;
}
float GetUniformScale(float2 s)
{
    return ( s.x + s.y ) / 2;
}
float GetUniformScale()
{
    return GetUniformScale(GetObjectScale());
}

struct ScaleData
{
    float thicknessPixelsTarget; // 1 when thicker than 1 px, px thickness when smaller
    float thicknessMeters; // vertex position thickness. includes LAA padding
    float aaPaddingScale; // multiplier used to correct UVs for LAA padding
    float pxPerMeter; // might be useful idk
};

/*inline float4 WorldToClipPos( in float3 worldPos ) {
    return mul( UNITY_MATRIX_VP, float4( worldPos, 1 ) );
}
inline float4 ViewToClipPos( in float3 viewPos ) {
    return mul( UNITY_MATRIX_P, float4( viewPos, 1 ) );
}
inline float4 LocalToClipPos( in float3 localPos ) {
    return UnityObjectToClipPos( float4( localPos, 1 ) );
}
inline float3 LocalToWorldPos( in float3 localPos ){
    return mul( UNITY_MATRIX_M, float4( localPos, 1 )).xyz; 
}*/

float4x4 inverse(float4x4 m)
{
    // Calculate the matrix of cofactors, then the determinant, then the adjugate, then divide by determinant.
    // This is a direct translation of the standard algorithm for 4x4 matrix inversion.
    float4x4 cof;
    float det;

    // Compute the cofactors for each element (tedious, but necessary for HLSL)
    // For brevity, you can use a pre-made implementation like this:
    // Source: https://stackoverflow.com/a/44446912

    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3];
    float a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3];

    float b00 = a00 * a11 - a01 * a10;
    float b01 = a00 * a12 - a02 * a10;
    float b02 = a00 * a13 - a03 * a10;
    float b03 = a01 * a12 - a02 * a11;
    float b04 = a01 * a13 - a03 * a11;
    float b05 = a02 * a13 - a03 * a12;
    float b06 = a20 * a31 - a21 * a30;
    float b07 = a20 * a32 - a22 * a30;
    float b08 = a20 * a33 - a23 * a30;
    float b09 = a21 * a32 - a22 * a31;
    float b10 = a21 * a33 - a23 * a31;
    float b11 = a22 * a33 - a23 * a32;

    det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

    float4x4 inv;
    inv[0][0] =  a11 * b11 - a12 * b10 + a13 * b09;
    inv[0][1] = -a01 * b11 + a02 * b10 - a03 * b09;
    inv[0][2] =  a31 * b05 - a32 * b04 + a33 * b03;
    inv[0][3] = -a21 * b05 + a22 * b04 - a23 * b03;

    inv[1][0] = -a10 * b11 + a12 * b08 - a13 * b07;
    inv[1][1] =  a00 * b11 - a02 * b08 + a03 * b07;
    inv[1][2] = -a30 * b05 + a32 * b02 - a33 * b01;
    inv[1][3] =  a20 * b05 - a22 * b02 + a23 * b01;

    inv[2][0] =  a10 * b10 - a11 * b08 + a13 * b06;
    inv[2][1] = -a00 * b10 + a01 * b08 - a03 * b06;
    inv[2][2] =  a30 * b04 - a31 * b02 + a33 * b00;
    inv[2][3] = -a20 * b04 + a21 * b02 - a23 * b00;

    inv[3][0] = -a10 * b09 + a11 * b07 - a12 * b06;
    inv[3][1] =  a00 * b09 - a01 * b07 + a02 * b06;
    inv[3][2] = -a30 * b03 + a31 * b01 - a32 * b00;
    inv[3][3] =  a20 * b03 - a21 * b01 + a22 * b00;

    return inv / det;
}

inline float3 WorldToLocalVec( in float3 worldVec ) {
    return mul((float3x3)inverse(perMesh.Transform), worldVec);
}
/*inline float3 LocalToWorldVec( in float3 localVec ){
    return mul( (float3x3)UNITY_MATRIX_M, localVec ); 
}
/nline float3 CameraToWorldVec( float3 camVec ){
    return mul( (float3x3)UNITY_MATRIX_I_V, camVec );
}
float2 WorldToScreenSpace( float3 worldPos )
{
    float4 clipSpace = UnityObjectToClipPos( float4( worldPos, 1 ) );
    float2 normalizedScreenspace = clipSpace.xy / clipSpace.w;
    return 0.5 * (normalizedScreenspace + 1.0) * perScene.Screen;
}*/

float CalcPixelsPerMeter( float3 worldPos, float3 worldDir )
{
    float w = dot(perScene.ViewProjection._m30_m31_m32_m33, float4(worldPos, 1));
    float2 clipVec = float2(
        dot(perScene.ViewProjection._m00_m01_m02, worldDir ) / w,
        dot(perScene.ViewProjection._m10_m11_m12, worldDir ) / w
    );
    return length(clipVec * perScene.Screen) / 2;
}

// line utils
inline void ConvertToPixelThickness(float3 vertOrigin, float3 normal, float thickness, int thicknessSpace, out float pxPerMeter, out float pxWidth)
{
    // calculate pixels per meter
	pxPerMeter = CalcPixelsPerMeter(vertOrigin, normal); // 1 unit in world space
	
	// figure out target width in pixels
	switch( thicknessSpace )
	{
	    case 0: 
		{
	        pxWidth = thickness * pxPerMeter; // this specifically should not have the + extraWidth
	        break;
	    }
	    case 1:
		{
	        pxWidth = thickness;
	        break;
	    }
    }
}

void GetPaddingData(float thicknessPixelsTarget, out float aaPaddingScale, out float pxWidthVert)
{
    // for vertex width, we need to clamp at 1px wide to prevent wandering ants and we don't want ants now do we
    pxWidthVert = max(1, thicknessPixelsTarget + 2);
    aaPaddingScale = pxWidthVert / max(VERY_SMOL, thicknessPixelsTarget); // how much extra we got from the padding, as a multiplier
}

ScaleData GetScreenSpaceWidthData(float3 vertOrigin, float3 normal, float thickness, int thicknessSpace)
{
    ScaleData data;
    ConvertToPixelThickness( vertOrigin, normal, thickness, thicknessSpace, /*out*/ data.pxPerMeter, /*out*/ data.thicknessPixelsTarget );
	
	float pxWidthVert;
	GetPaddingData( data.thicknessPixelsTarget, /*out*/ data.aaPaddingScale, /*out*/ pxWidthVert );
	
	// when using pixel size, scale to match pixels
	data.thicknessMeters = pxWidthVert / data.pxPerMeter; // clamps at 1px wide, then converts to meters
    
    return data;
}

ScaleData GetScreenSpaceWidthDataSimple(float3 vertOrigin, float3 normal, float thickness, int thicknessSpace)
{
    ScaleData data;
    ConvertToPixelThickness( vertOrigin, normal, thickness, thicknessSpace, /*out*/ data.pxPerMeter, /*out*/ data.thicknessPixelsTarget );
    float pxWidthVert = max( 1, data.thicknessPixelsTarget );
    data.aaPaddingScale = 1; 
	data.thicknessMeters = pxWidthVert / data.pxPerMeter; // clamps at 1px wide, then converts to meters
    return data;
}

inline float ArcLengthToAngle(float radius, float arcLength)
{
    return arcLength / radius;
}

float2 Rotate(float2 v, float ang)
{
	float2 a = float2(cos(ang), sin(ang));
	return float2(
		a.x * v.x - a.y * v.y,
		a.y * v.x + a.x * v.y
	);
}

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

inline float InverseLerp(float a, float b, float v) {     return (v - a) / (b - a); }
inline float2 InverseLerp(float2 a, float2 b, float2 v) { return (v - a) / (b - a); }
inline float3 InverseLerp(float3 a, float3 b, float v) {  return (v - a) / (b - a); }
float2 Remap(float2 iMin, float2 iMax, float2 oMin, float2 oMax, float2 v) {
    float2 t = InverseLerp(iMin, iMax, v);
    return lerp(oMin, oMax, t);
}
float Remap(float iMin, float iMax, float oMin, float oMax, float v) {
    float t = InverseLerp(iMin, iMax, v);
    return lerp(oMin, oMax, t);
}

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

	float uniformScale = GetUniformScale();
	float radius = perMesh._Radius * uniformScale;
    ScaleData widthDataRadius = GetScreenSpaceWidthDataSimple(OBJ_ORIGIN, CAM_RIGHT, radius * 2, perMesh._ScaleSpace);
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
	    ScaleData widthDataThickness = GetScreenSpaceWidthDataSimple(OBJ_ORIGIN, CAM_RIGHT, thickness, perMesh._ScaleSpace);
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
		const float3 rightLocal = WorldToLocalVec(right);
		const float3 upLocal = WorldToLocalVec(up);
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