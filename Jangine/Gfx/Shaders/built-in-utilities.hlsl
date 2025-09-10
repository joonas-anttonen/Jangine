#define TAU 6.28318530718
#define VERY_SMOL 0.00006103515625 // smallest positive normal number for float precision floats

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

void GetDirMag(in float2 v, out float2 dir, out float mag)
{
	mag = length(v);
	dir = v / mag; // Normalize
}
void GetDirMag(in float3 v, out float3 dir, out float mag)
{
	mag = length(v);
	dir = v / mag; // Normalize
}

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

inline float GetLineLocalAA(float coord, float pxCoverage, float pxOffset = 0)
{
    float ddxy = PD(coord);
    float sdf = abs(coord) - 1;
    float aaOffset = saturate(InverseLerp(0, 1.1, pxCoverage)) * 0.5; // the 1.1 here is very much a hack but it looks good okay THIN LINES ARE HARD
    return 1.0 - StepThresholdPDAAOffset(sdf, ddxy, aaOffset + pxOffset);
}

float3 GetObjectScale(float4x4 transform)
{
    float3x3 m = (float3x3)transform;
    return float3(
        length( float3( m[0][0], m[1][0], m[2][0] ) ),
        length( float3( m[0][1], m[1][1], m[2][1] ) ),
        length( float3( m[0][2], m[1][2], m[2][2] ) )
    );
}
float2 GetObjectScaleXY(float4x4 transform)
{
    float3x3 m = (float3x3)transform;
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
float GetUniformScale(float4x4 transform)
{
    return GetUniformScale(GetObjectScale(transform));
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

inline float3 WorldToLocalVec(float4x4 transform, in float3 worldVec ) {
    return mul((float3x3)inverse(transform), worldVec);
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

float CalcPixelsPerMeter(float4x4 viewProjection, float2 screenSize, float3 worldPos, float3 worldDir )
{
    float w = dot(viewProjection._m30_m31_m32_m33, float4(worldPos, 1));
    float2 clipVec = float2(
        dot(viewProjection._m00_m01_m02, worldDir ) / w,
        dot(viewProjection._m10_m11_m12, worldDir ) / w
    );
    return length(clipVec * screenSize) / 2;
}

// line utils
inline void ConvertToPixelThickness(float4x4 viewProjection, float2 screenSize,float3 vertOrigin, float3 normal, float thickness, int thicknessSpace, out float pxPerMeter, out float pxWidth)
{
    // calculate pixels per meter
	pxPerMeter = CalcPixelsPerMeter(viewProjection, screenSize, vertOrigin, normal); // 1 unit in world space
	
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

ScaleData GetScreenSpaceWidthData(float4x4 viewProjection, float2 screenSize, float3 vertOrigin, float3 normal, float thickness, int thicknessSpace)
{
    ScaleData data;
    ConvertToPixelThickness(viewProjection, screenSize, vertOrigin, normal, thickness, thicknessSpace, /*out*/ data.pxPerMeter, /*out*/ data.thicknessPixelsTarget );

	float pxWidthVert;
	GetPaddingData( data.thicknessPixelsTarget, /*out*/ data.aaPaddingScale, /*out*/ pxWidthVert );
	
	// when using pixel size, scale to match pixels
	data.thicknessMeters = pxWidthVert / data.pxPerMeter; // clamps at 1px wide, then converts to meters
    
    return data;
}

ScaleData GetScreenSpaceWidthDataSimple(float4x4 viewProjection, float2 screenSize, float3 vertOrigin, float3 normal, float thickness, int thicknessSpace)
{
    ScaleData data;
    ConvertToPixelThickness(viewProjection, screenSize, vertOrigin, normal, thickness, thicknessSpace, /*out*/ data.pxPerMeter, /*out*/ data.thicknessPixelsTarget );
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