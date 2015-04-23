#include "MaterialShaders.h"

const char* vs_simpleTexture =
"#version 300 es \n"
STRINGIFY(
in vec4 vPosition;
in vec2 vTexCoords;
out vec2 fTexCoords;
uniform mat4 worldToViewProjMat;
void main() {
	fTexCoords = vTexCoords;
	gl_Position = worldToViewProjMat * vPosition;
}
);

const char* fs_color =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform vec4 color;
void main()
{
	gl_FragColor = color;
}
);

const char* fs_simpleTexture2d =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0;
uniform vec4 color;
in vec2 fTexCoords;
void main()
{
	gl_FragColor = color * texture2D(texture0, fTexCoords);
}
);

const char* fs_lodTexture2d =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0;
uniform vec4 color;
in vec2 fTexCoords;
uniform float lod;
void main()
{
	gl_FragColor = color * textureLod(texture0, fTexCoords, lod);
}
);

const char* fs_simpleTexture2d_r =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0;
uniform vec4 color;
in vec2 fTexCoords;
void main()
{
	float val = texture2D(texture0, fTexCoords).r;
	gl_FragColor = color * vec4(val, val, val, (val<1.0)?1.0:0.0);
}
);

const char* fs_simpleTextureExternal =
"#version 300 es \n"
"#extension GL_OES_EGL_image_external : require\n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform samplerExternalOES texture0;
uniform vec4 color;
in vec2 fTexCoords;
void main()
{
	gl_FragColor = color * texture2D(texture0, vec2(fTexCoords.x, 1.0 - fTexCoords.y));
}
);

const char* fs_correctColorDistortion =
"#version 300 es \n"
"#extension GL_OES_EGL_image_external : require\n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform samplerExternalOES texture0;
uniform vec2 textureSize0;
uniform vec4 color;
in vec2 fTexCoords;
uniform vec2 focalLength;
uniform vec2 principalPoint;
uniform vec4 kCoefficients;
uniform float kCoefficients2;

void main()
{
	// from tango docs:
	// x_corr_px = x_px (1 + k1 * r2 + k2 * r4 + k3 * r6)
	// y_corr_px = y_px (1 + k1 * r2 + k2 * r4 + k3 * r6)

	vec2 uvCoord = vec2(fTexCoords.x, 1.0 - fTexCoords.y) * textureSize0;

	vec2 planeCoords = (uvCoord - principalPoint) / focalLength;
	float r2 = dot(planeCoords, planeCoords);
	float r4 = r2 * r2;
	float r6 = r4 * r2;
	float coeff = (kCoefficients.x * r2 + kCoefficients.y * r4 + kCoefficients.z * r6);

	uvCoord = ((planeCoords + planeCoords * vec2(coeff, coeff)) * focalLength) + principalPoint;

	if (uvCoord.y <= 1.5)
	{
		gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	gl_FragColor = color.rgba * texture2D(texture0, uvCoord / textureSize0);
}
);

const char* fs_correctFisheyeDistortion =
"#version 300 es \n"
"#extension GL_OES_EGL_image_external : require\n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform samplerExternalOES texture0;
uniform vec2 textureSize0;
uniform vec4 color;
in vec2 fTexCoords;
uniform highp vec2 focalLength;
uniform highp vec2 principalPoint;
uniform highp vec4 kCoefficients;
uniform highp float kCoefficients2;

void main()
{
	vec2 uvCoord = vec2(fTexCoords.x, 1.0 - fTexCoords.y) * textureSize0;

	vec2 planeCoords = (uvCoord - principalPoint) / focalLength;

	float omega = kCoefficients.x;

	float ru = sqrt(dot(planeCoords, planeCoords));
	float rd = atan(2.0 * ru * tan(omega * 0.5)) / omega;
	uvCoord = (planeCoords * rd / ru) * focalLength + principalPoint;

	gl_FragColor = color.rgba * texture2D(texture0, uvCoord / textureSize0);
}
);

const char* fs_rgbdTextureWithDepth =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0;
uniform vec4 color;
uniform int srcDepthType;
uniform vec2 srcClipRange;
uniform vec2 dstClipRange;
uniform int debugMode;
in vec2 fTexCoords;

float linearizeFragDepth(float fragDepth, float n, float f)
{
	return (2.0 * n) / (f + n - fragDepth * (f - n));	
    //return (n * fragDepth) / ( f - fragDepth * (f - n) );
}

vec3 pseudoTemperature(float t)
{
	if (t < 0.0 || t > 1.0)
		return vec3(1.0, 1.0, 1.0);

	float b = t < 0.25f ? smoothstep(-0.25f, 0.25f, t) : 1.0f - smoothstep(0.25f, 0.5f, t);
	float g = t < 0.5f ? smoothstep(0.0f, 0.5f, t) : (t < 0.75f ? 1.0f : 1.0f - smoothstep(0.75f, 1.0f, t));
	float r = smoothstep(0.5f, 0.75f, t);
	return vec3(r, g, b);
}

float getEyeZFromUnitDepth(float unitDepth, float n, float f)
{
	return -(n + (unitDepth * (f - n)));
}

float getFragDepthFromEyeZ(float eyeZ, float n, float f)
{
	// A = gl_ProjectionMatrix[2][2]
	// B = gl_ProjectionMatrix[3][2]
	// Zn = (A*Ze + B) / -Ze
	// Ze = B / (-Zn - A)

	// compute projection matrix elements from near and far distances.
	float projA = -(f + n) / (f - n);
	float projB = -(2.0 * f * n) / (f - n);

	float ndcZ = (projA*eyeZ + projB) / (-eyeZ);

	float depthFar = gl_DepthRange.far;
	float depthNear = gl_DepthRange.near;
	float fragDepth = (((depthFar - depthNear) * ndcZ) + depthNear + depthFar) * 0.5;

	return clamp(fragDepth, depthNear, depthFar);
}

void main()
{
	vec4 texel = texture2D(texture0, fTexCoords.xy);

	float depth = texel.a;

	// CAVEAT: if depth is zero, then assume zero alpha too!
	if (depth <= 0.0 || depth >= 1.0)
	{
		if (debugMode == 0)
			discard;
		else
		{
			gl_FragColor = vec4(1.0,0.0,1.0,1.0);
			gl_FragDepth = 0.0;
			return;
		}

		discard;
	}

	if (srcDepthType == 0)
	{
		float linearDepth = linearizeFragDepth(depth, srcClipRange.x, srcClipRange.y);

		// it is already in normalized-fragment space.
		// TODO: convert if it is not the same clipping-range
		//depth = getEyeZFromFragDepth(depth, srcClipRange.x, srcClipRange.y);
		//depth = getFragDepthFromEyeZ(depth, dstClipRange.x, dstClipRange.y);
		if (linearDepth <= 0.0 || linearDepth >= 0.95)
		{
			gl_FragColor = vec4(0.0,1.0,0.0,1.0);
			gl_FragDepth = 1.0;
			//discard;
			return;
		}
	}
	if (srcDepthType == 1)
	{
		/*if (debugMode == 1)
		{
			gl_FragColor = vec4(pseudoTemperature(depth), 1.0);
			return;
		}*/

		if (depth >= 1.0)
		{
			//depth = gl_DepthRange.far;
			gl_FragColor = vec4(0.0,1.0,1.0,1.0);
			gl_FragDepth = 0.0;
			return;
		}
		else
		{
			// convert to fragment-space from normalized-linear space
			depth = getEyeZFromUnitDepth(depth, srcClipRange.x, srcClipRange.y);
			depth = getFragDepthFromEyeZ(depth, dstClipRange.x, dstClipRange.y);
		}

		if (depth >= 1.0)
		{
			//discard;
			gl_FragColor = vec4(0.0,1.0,1.0,1.0);
			gl_FragDepth = 0.0;
			return;
		}
	}

	if (debugMode == 0)
	{
		gl_FragColor = color.rgba * vec4(texel.rgb, 1.0);
		gl_FragDepth = depth;
	}
	else
	{
		// debugging:
		gl_FragColor = vec4(pseudoTemperature(depth), 1.0);
		gl_FragDepth = depth;
	}
}
);



const char* fs_rgbdSoftDepthComposite =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(

uniform sampler2D texture0;	// srcColor+srcdepth (RGBD)
uniform sampler2D texture1;	// dstDepth (RGBD)
uniform vec4 color;
uniform int srcDepthType;
uniform vec2 srcClipRange;
uniform int dstDepthType;
uniform vec2 dstClipRange;
uniform int debugMode;
uniform float softness; // (distance units)
in vec2 fTexCoords;

vec3 pseudoTemperature(float t)
{
	if (t < 0.0 || t > 1.0)
		return vec3(1.0, 1.0, 1.0);

	float b = t < 0.25f ? smoothstep(-0.25f, 0.25f, t) : 1.0f - smoothstep(0.25f, 0.5f, t);
	float g = t < 0.5f ? smoothstep(0.0f, 0.5f, t) : (t < 0.75f ? 1.0f : 1.0f - smoothstep(0.75f, 1.0f, t));
	float r = smoothstep(0.5f, 0.75f, t);
	return vec3(r, g, b);
}

float getEyeZFromUnitDepth(float unitDepth, float n, float f)
{
	return -(n + (unitDepth * (f - n)));
}

float getFragDepthFromEyeZ(float eyeZ, float n, float f)
{
	// A = gl_ProjectionMatrix[2][2]
	// B = gl_ProjectionMatrix[3][2]
	// Zn = (A*Ze + B) / -Ze
	// Ze = B / (-Zn - A)

	// compute projection matrix elements from near and far distances.
	float projA = -(f + n) / (f - n);
	float projB = -(2.0 * f * n) / (f - n);

	float ndcZ = (projA*eyeZ + projB) / (-eyeZ);

	float depthFar = gl_DepthRange.far;
	float depthNear = gl_DepthRange.near;
	float fragDepth = (((depthFar - depthNear) * ndcZ) + depthNear + depthFar) * 0.5;

	return clamp(fragDepth, depthNear, depthFar);
}

float linearizeFragDepth(float fragDepth, float n, float f)
{
	return (2.0 * n) / (f + n - fragDepth * (f - n));	
    //return (n * fragDepth) / ( f - fragDepth * (f - n) );
}

void main()
{
	vec4 texel = texture2D(texture0, fTexCoords.xy);
	float depth = texel.a;
	float dstDepth = texture2D(texture1, fTexCoords.xy).a; 

	// CAVEAT: if depth is zero, then assume zero alpha too!
	if (depth <= 0.0)
	{
		if (debugMode == 0)
			discard;
		else
		{
			gl_FragColor = vec4(0,1.0,1.0,1.0);
			return;
		}
	}

	if (dstDepth <= 0.0 || dstDepth >= 1.0)
	{
		// if dst is undefined, then just use the src
		gl_FragColor = color.rgba * vec4(texel.rgb, 1.0);
		return;
	}

	if (srcDepthType == 0)
	{
		// it is already in normalized-fragment space.
		// TODO: convert if it is not the same clipping-range
		//depth = getEyeZFromFragDepth(depth, srcClipRange.x, srcClipRange.y);
		//depth = getFragDepthFromEyeZ(depth, dstClipRange.x, dstClipRange.y);
	}
	if (srcDepthType == 1)
	{
		if (debugMode == 1)
		{
			gl_FragColor = vec4(pseudoTemperature(depth), 1.0);
			return;
		}

		if (depth > 0.99)
		{
			depth = gl_DepthRange.far;
		}
		else
		{
			// convert to fragment-space from normalized-linear space
			depth = getEyeZFromUnitDepth(depth, srcClipRange.x, srcClipRange.y);
			depth = getFragDepthFromEyeZ(depth, dstClipRange.x, dstClipRange.y);
		}
	}

	if (debugMode == 0)
	{
		float dstLinearDepth = linearizeFragDepth(dstDepth,dstClipRange.x, dstClipRange.y);
		float srcLinearDepth = linearizeFragDepth(depth,dstClipRange.x, dstClipRange.y);

		float minDepth = min(srcLinearDepth, dstLinearDepth);
		gl_FragColor = vec4(pseudoTemperature(minDepth), 1.0);
		//gl_FragDepth = depth;

		float zFade = clamp(softness * (dstLinearDepth-srcLinearDepth), 0.0, 1.0);
		gl_FragColor = color.rgba * vec4(texel.rgb, zFade);

		

		// hack to get rid of bg pixels.
		if (depth >= 1.0)
			discard;
	}
	else
	{
		// debugging:
		gl_FragColor = vec4(pseudoTemperature(depth), 1.0);
	}
}
);

const char* fs_showDepth =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(

uniform sampler2D texture0;	// rgbd
uniform int srcDepthType; // fragment = 0, normalized-linear = 1
uniform int srcImageType; // depth = 0, rgbd = 1
uniform vec2 srcClipRange;

in vec2 fTexCoords;

vec3 pseudoTemperature(float t)
{
	if (t < 0.0 || t > 1.0)
		return vec3(1.0, 1.0, 1.0);

	float b = t < 0.25f ? smoothstep(-0.25f, 0.25f, t) : 1.0f - smoothstep(0.25f, 0.5f, t);
	float g = t < 0.5f ? smoothstep(0.0f, 0.5f, t) : (t < 0.75f ? 1.0f : 1.0f - smoothstep(0.75f, 1.0f, t));
	float r = smoothstep(0.5f, 0.75f, t);
	return vec3(r, g, b);
}

float linearizeFragDepth(float fragDepth, float n, float f)
{
	return (2.0 * n) / (f + n - fragDepth * (f - n));	
    //return (n * fragDepth) / ( f - fragDepth * (f - n) );
}

void main()
{
	vec4 texel = texture2D(texture0, fTexCoords.xy);
	float depth = texel.r;
	if (srcImageType == 1)
		depth = texel.a;

	// CAVEAT: if depth is zero, then assume zero alpha too!
	if (depth <= 0.0 || depth >= 1.0)
	{
		gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
		gl_FragDepth = 0.0;
		return;
	}

	if (srcDepthType == 0)
	{
		depth = linearizeFragDepth(depth, srcClipRange.x, srcClipRange.y);
	}
	if (srcDepthType == 1)
	{
	}

	gl_FragColor = vec4(pseudoTemperature(depth), 1.0);
	gl_FragDepth = 0.0;
}
);

const char* fs_reduceRgbd =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0; 
in vec2 fTexCoords; 

//---------------------------------------------------\n
void main()
{ 
	// NB. gl_FragCoord is for a half-size viewport.
	ivec2 coord = ivec2(gl_FragCoord.xy-vec2(0.5,0.5))*ivec2(2);
	
	vec4 ll = texelFetch(texture0, coord, 0); 
	vec4 lr = texelFetchOffset(texture0, coord, 0, ivec2(1, 0)); 
	vec4 ul = texelFetchOffset(texture0, coord, 0, ivec2(0, 1)); 
	vec4 ur = texelFetchOffset(texture0, coord, 0, ivec2(1, 1)); 
	
	// get the farthest valid point...
	vec4 result = vec4(0.0,0.0,0.0,1.0);
	if (ll.a < 1.0 && result.a >= ll.a)
		result = ll.rgba;
	if (lr.a < 1.0 && result.a >= lr.a)
		result = lr.rgba;
	if (ul.a < 1.0 && result.a >= ul.a)
		result = ul.rgba;
	if (ur.a < 1.0 && result.a >= ur.a)
		result = ur.rgba;

	// if all pixels are undefined, then result is invalid
	if (ll.a + lr.a + ul.a + ur.a == 0.0)
	{
		// no need to set.
		//result = vec4(0.0,0.0,0.0,1.0);
	}

	gl_FragColor = result;
}
);

const char* fs_reduceColor =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0; \n
in vec2 fTexCoords; \n
\n
//---------------------------------------------------\n
void main()\n
{ \n
	// NB. gl_FragCoord is for a half-size viewport.\n
	ivec2 coord = ivec2(gl_FragCoord.xy-vec2(0.5,0.5))*ivec2(2);\n
	\n
	vec4 ll = texelFetch(texture0, coord, 0).rgba; \n
	vec4 lr = texelFetchOffset(texture0, coord, 0, ivec2(1, 0)).rgba; \n
	vec4 ul = texelFetchOffset(texture0, coord, 0, ivec2(0, 1)).rgba; \n
	vec4 ur = texelFetchOffset(texture0, coord, 0, ivec2(1, 1)).rgba; \n
	\n
	vec3 color;\n
	color = ll.rgb;\n
	color += lr.rgb;\n
	color += ul.rgb;\n
	color += ur.rgb;\n
	color *= 0.25;
	\n
	gl_FragColor = vec4(color, 1.0);\n	
}
);

const char* fs_reduceTexture_max =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0; \n
in vec2 fTexCoords; \n
\n
//---------------------------------------------------\n
void main()\n
{ \n
	// the gl_FragCoord is for a half-size viewport.\n
	ivec2 coord = ivec2(gl_FragCoord.xy*vec2(2.0)); \n
	float ll = texelFetch(texture0, coord, 0).r; \n
	float lr = texelFetchOffset(texture0, coord, 0, ivec2(1, 0)).r; \n
	float ul = texelFetchOffset(texture0, coord, 0, ivec2(0, 1)).r; \n
	float ur = texelFetchOffset(texture0, coord, 0, ivec2(1, 1)).r; \n

	// this method uses only the samples which have alpha > 0
	float result = 0.0;
	if (ll < 1.0)
		result = max(result, ll);
	if (lr < 1.0)
		result = max(result, lr);
	if (ul < 1.0)
		result = max(result, ul);
	if (ur < 1.0)
		result = max(result, ur);

	if (ll * lr * ul * ur >= 1.0)
		result = 1.0;

	gl_FragColor = vec4(result, result, result, 1.0);
/*
// MAX: this method discards the entire result if any samples have alpha < 1.0
vec4 result = max(max(max(ll, lr), ul), ur);
if (ll.a * lr.a * ul.a * ur.a < 1.0)
	result = vec4(1.0,0.0,0.0,0.0);
*/
/*
// MIN: this method discards the entire result if any samples have alpha < 1.0
vec4 result = vec4(1.0);
//if (ll.a > 0.0)
	result = min(result, ll);
//if (lr.a > 0.0)
	result = min(result, lr);
//if (ul.a > 0.0)
	result = min(result, ul);
//if (ur.a > 0.0)
	result = min(result, ur);
//if (ll.a + lr.a + ul.a + ur.a > 0.0)
	result.a = 1.0;
	*/

}
);

// project color into depth to create an RGB image...
const char* fs_projectColorFromDepth =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0;
uniform sampler2D texture1;
in vec2 fTexCoords;
uniform mat4 depthViewToWorldMat;
uniform mat4 depthViewProjInvMat;
uniform mat4 colorWorldToViewMat;
uniform mat4 colorViewProjMat;

vec3 pseudoTemperature(float t)
{
	float b = t < 0.25f ? smoothstep(-0.25f, 0.25f, t) : 1.0f - smoothstep(0.25f, 0.5f, t);
	float g = t < 0.5f ? smoothstep(0.0f, 0.5f, t) : (t < 0.75f ? 1.0f : 1.0f - smoothstep(0.75f, 1.0f, t));
	float r = smoothstep(0.5f, 0.75f, t);
	return vec3(r, g, b);
}

vec2 getUvFromFragDepth(float unitCoordX, float unitCoordY, float fragDepth)
{
	// convert the fragment depth to a world-position,
	// and project the world-position into the dst-uv-space
	vec3 srcNdcPos = vec3((unitCoordX * 2.0 - 1.0), (unitCoordY * 2.0 - 1.0), (fragDepth * 2.0 - 1.0));
	vec4 srcViewPos = depthViewProjInvMat * vec4(srcNdcPos, 1.0);

	vec4 dstNdcPos = colorViewProjMat * colorWorldToViewMat * depthViewToWorldMat * vec4(srcViewPos.xyz / srcViewPos.w, 1.0);
	dstNdcPos.xyz = dstNdcPos.xyz / dstNdcPos.w;
	vec2 dstUvPos = dstNdcPos.xy * vec2(0.5) + vec2(0.5);
	return dstUvPos;
}

void main()
{
	// read depth
	vec4 depthAndConfidence = texture2D(texture0, fTexCoords);
	float srcFragDepth = depthAndConfidence.r;
	vec2 dstUvPos = getUvFromFragDepth(fTexCoords.x, fTexCoords.y, srcFragDepth);
	vec4 dstTexel = texture2D(texture1, dstUvPos);
	vec4 rgbd = vec4(dstTexel.rgb, depthAndConfidence.a);

	gl_FragColor = rgbd;

	// debugging:
	//gl_FragColor.rgb = pseudoTemperature(srcFragDepth); // show the depth
	//gl_FragColor.rgb = pseudoTemperature(rgbd.a); // show the confidence
}
);

// merge color into depth to create an RGB image...
const char* fs_mergeColorFromDepth =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0; // current depth
uniform sampler2D texture1; // new color
uniform sampler2D texture2; // current color
in vec2 fTexCoords;
uniform mat4 depthViewToWorldMat;
uniform mat4 depthViewProjInvMat;
uniform mat4 colorWorldToViewMat;
uniform mat4 colorViewProjMat;

vec3 pseudoTemperature(float t)
{
	float b = t < 0.25f ? smoothstep(-0.25f, 0.25f, t) : 1.0f - smoothstep(0.25f, 0.5f, t);
	float g = t < 0.5f ? smoothstep(0.0f, 0.5f, t) : (t < 0.75f ? 1.0f : 1.0f - smoothstep(0.75f, 1.0f, t));
	float r = smoothstep(0.5f, 0.75f, t);
	return vec3(r, g, b);
}

vec2 getUvFromFragDepth(float unitCoordX, float unitCoordY, float fragDepth)
{
	// convert the fragment depth to a world-position,
	// and project the world-position into the dst-uv-space
	vec3 srcNdcPos = vec3((unitCoordX * 2.0 - 1.0), (unitCoordY * 2.0 - 1.0), (fragDepth * 2.0 - 1.0));
	vec4 srcViewPos = depthViewProjInvMat * vec4(srcNdcPos, 1.0);

	vec4 dstNdcPos = colorViewProjMat * colorWorldToViewMat * depthViewToWorldMat * vec4(srcViewPos.xyz / srcViewPos.w, 1.0);
	dstNdcPos.xyz = dstNdcPos.xyz / dstNdcPos.w;
	vec2 dstUvPos = dstNdcPos.xy * vec2(0.5) + vec2(0.5);
	return dstUvPos;
}

void main()
{
	float srcFragDepth = texture2D(texture0, fTexCoords).r;
	vec2 dstUvPos = getUvFromFragDepth(fTexCoords.x, fTexCoords.y, srcFragDepth);
	vec4 dstTexel = texture2D(texture1, dstUvPos);
	vec4 rgbd = vec4(dstTexel.rgba);//, srcFragDepth);

	vec4 currentTexel = texture2D(texture2, fTexCoords);

	/*
	// for debugging:
	if (rgbd.a >= 1.0 || dstTexel.a == 0.0)
	{
		rgbd.a = 0.0;
		discard;
		//dstTexel = vec4(1.0,0.0,0.0,0.0);
	}
	*/
	gl_FragColor = rgbd;

	// debugging:
	//gl_FragColor.rgb = pseudoTemperature(dstTexel.a);
}
);

const char* vs_warpMeshRGBD =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0; // depth
in vec4 vPosition;
out vec3 fTexCoords;
uniform mat4 worldToViewProjMat;
uniform float srcNearClip;
uniform float srcFarClip;
uniform mat4 srcViewToWorldMat;
uniform mat4 srcViewProjInvMat;
uniform float pointSize;
uniform int srcImageMode;

float getEyeZFromFragDepth(float fragDepth, float n, float f)
{
	float ndcZ = (fragDepth * 2.0 - 1.0);

	// http://www.songho.ca/opengl/gl_projectionmatrix.html
	// A = gl_ProjectionMatrix[2][2]
	// B = gl_ProjectionMatrix[3][2]
	// Zn = (A*Ze + B) / -Ze
	// Ze = B / (-Zn - A)

	// compute projection matrix elements from near and far distances.
	float projA = -(f + n) / (f - n);
	float projB = -(2.0 * f * n) / (f - n);

	float eyeZ = projB / (-ndcZ - projA);
	return eyeZ;
}

vec3 eyePositionFromFragDepth(float unitCoordX, float unitCoordY, float fragDepth, float n, float f)
{
	float aspect = 1.777;
	float vfov = 90.0 * 3.142 / 180.0;
	float xmin;
	float xmax;
	float ymin;
	float ymax;

	float sx = (unitCoordX);
	float sy = (unitCoordY);

	// get the xy NDC coordinates...
	float ndcx = (sx * 2.0 - 1.0);
	float ndcy = (sy * 2.0 - 1.0);

	vec3 eye;
	eye.z = getEyeZFromFragDepth(fragDepth, n, f);


	ymax = n * tan(vfov*0.5);
	ymin = -ymax;
	xmax = ymax * aspect;
	xmin = -xmax;

	eye.x = (-ndcx * eye.z) * (xmax - xmin) / (2.0 * n) - eye.z * (xmax + xmin) / (2.0 * n);
	eye.y = (-ndcy * eye.z) * (ymax - ymin) / (2.0 * n) - eye.z * (ymax + ymin) / (2.0 * n);

	return eye;
}

void main()
{
	vec2 uv = vec2(vPosition.x, vPosition.y);

	float fragDepth;
	if (srcImageMode == 0)
		fragDepth = texture2D(texture0, uv).r;
	else if (srcImageMode == 1)
		fragDepth = texture2D(texture0, uv).a;

	//vec3 vpos = eyePositionFromFragDepth(uv.x, uv.y, fragDepth, nearClip, farClip);

	/*if (fragDepth >= 1.0 || fragDepth <= 0.0)
	{
	// put vertex on farclip plane.
	gl_Position = vec4(uv.x*2.0-1.0, uv.y*2.0-1.0, 0.0, 1.0);
	fTexCoords.z = 1.0;
	gl_PointSize = 1.0;
	return;
	}*/

	vec3 ndc = vec3((uv.x * 2.0 - 1.0), (uv.y * 2.0 - 1.0), (fragDepth * 2.0 - 1.0));
	vec4 vpos = srcViewProjInvMat * vec4(ndc, 1.0);
	vpos.xyz = vpos.xyz / vpos.w;
	vec4 wpos = srcViewToWorldMat * vec4(vpos.xyz, 1.0);

	gl_Position = worldToViewProjMat * vec4(wpos.xyz, 1.0);

	fTexCoords.xy = uv;
	fTexCoords.z = fragDepth;

	gl_PointSize = pointSize;
}
);

const char *fs_setRgbd =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0; // depth
uniform sampler2D texture1; // color
in vec2 fTexCoords;

void main()
{
	gl_FragColor.rgb = texture2D(texture1, fTexCoords).rgb;
	gl_FragColor.a = texture2D(texture0, fTexCoords).r;
}

);
const char* fs_warpRGBD =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture1; // color
uniform vec4 color;
in vec3 fTexCoords;

vec3 pseudoTemperature(float t)
{
	float b = t < 0.25f ? smoothstep(-0.25f, 0.25f, t) : 1.0f - smoothstep(0.25f, 0.5f, t);
	float g = t < 0.5f ? smoothstep(0.0f, 0.5f, t) : (t < 0.75f ? 1.0f : 1.0f - smoothstep(0.75f, 1.0f, t));
	float r = smoothstep(0.5f, 0.75f, t);
	return vec3(r, g, b);
}

void main()
{
	if (fTexCoords.z <= 0.0 || fTexCoords.z >= 1.0)
	{
		// if the depth is 1.0 then discard this point.
		discard;
		//gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	//gl_FragColor = vec4(pseudoTemperature(fTexCoords.z), 1.0);
	gl_FragColor = texture2D(texture1, fTexCoords.xy);
}
);


const char* fs_holeFill =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0; // rgbd
in vec3 fTexCoords;

void main()
{
	ivec2 vTexCoord = ivec2(gl_FragCoord.xy);

	vec4 c  = texelFetch(texture0, vTexCoord.xy, 0);
	float depth = c.a;  

	if (depth < 0.99)
	{
		// only process bg pixels.
		gl_FragColor = vec4(c.rgb, depth);
		//gl_FragDepth = depth;
		return;
	}
	else
	{
		// this is a bg pixel...
		
		// debugging:
		//gl_FragColor = vec4(1.0, 1.0, 0.0, 0.0);
        //gl_FragDepth = depth;
		//return;
	}

	vec4 cTL = texelFetch(texture0, vTexCoord.xy+ivec2(-1, 1), 0);  
	vec4 cT  = texelFetch(texture0, vTexCoord.xy+ivec2( 0, 1), 0);  
	vec4 cTR = texelFetch(texture0, vTexCoord.xy+ivec2( 1, 1), 0);  
	vec4 cR  = texelFetch(texture0, vTexCoord.xy+ivec2( 1, 0), 0);  
	vec4 cBR = texelFetch(texture0, vTexCoord.xy+ivec2( 1,-1), 0);  
	vec4 cB  = texelFetch(texture0, vTexCoord.xy+ivec2( 0,-1), 0);  
	vec4 cBL = texelFetch(texture0, vTexCoord.xy+ivec2(-1,-1), 0);  
	vec4 cL  = texelFetch(texture0, vTexCoord.xy+ivec2(-1, 0), 0);  

	// get (1-fragDepth). This is not linear, but it is normalized 0 to 1, where 0 is far depth.
	const float depthScale = 1.0;
	float d   = 1.0-clamp(depthScale*c.a, 0.0, 1.0); 
    float dTL = 1.0-clamp(depthScale*cTL.a, 0.0, 1.0);  
    float dT  = 1.0-clamp(depthScale*cT.a, 0.0, 1.0);  
    float dTR = 1.0-clamp(depthScale*cTR.a, 0.0, 1.0);   
    float dR  = 1.0-clamp(depthScale*cR.a, 0.0, 1.0);    
    float dBR = 1.0-clamp(depthScale*cBR.a, 0.0, 1.0);  
    float dB  = 1.0-clamp(depthScale*cB.a, 0.0, 1.0);   
    float dBL = 1.0-clamp(depthScale*cBL.a, 0.0, 1.0);   
    float dL  = 1.0-clamp(depthScale*cL.a, 0.0, 1.0);   

	float mask0 = dT + dTR + dR + dBR + dB;
	float mask1 = dL + dTL + dT + dTR + dR;
	float mask2 = dB + dBL + dL + dTL + dT;
	float mask3 = dR + dBR + dB + dBL + dL;
	float mask4 = dTL + dT + dTR + dR + dBR;
	float mask5 = dBL + dL + dTL + dT + dTR;
	float mask6 = dBR + dB + dBL + dL + dTL;
	float mask7 = dTR + dR + dBR + dB + dBL;

	float totalMask = mask0 * mask1 * mask2 * mask3 * mask4 * mask5 * mask6 * mask7;

	if (totalMask == 0.0)
	{
		// one of the masks was fulfilled, so this is the still considered background...
		gl_FragColor = vec4(c.rgb, 1.0);
		//gl_FragDepth = d;
		discard;
		return;
	}
	else
	{
		// fill this bg pixel...

		// find the nearest depth, color and depth neighbor.

		vec3 minc = cT.rgb;
		float mind = dT;
		if (dTL > mind)
			{minc = cTL.rgb; mind = dTL;}
		if (dL > mind)
			{minc = cL.rgb; mind = dL;}
		if (dBL > mind)
			{minc = cBL.rgb; mind = dBL;}
		if (dB > mind)
			{minc = cB.rgb; mind = dB;}
		if (dBR > mind)
			{minc = cBR.rgb; mind = dBR;}
		if (dR > mind)
			{minc = cR.rgb; mind = dR;}
		if (dTR > mind)
			{minc = cTR.rgb; mind = dTR;}

		// "mind" should NEVER be zero now.
		float newDepth = (1.0-mind);

		gl_FragColor = vec4(minc.rgb, newDepth); // store rgbd
		//gl_FragDepth = newDepth; // store depth
	}
}
);

