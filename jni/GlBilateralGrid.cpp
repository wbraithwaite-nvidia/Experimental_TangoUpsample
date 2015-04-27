
#include "GlBilateralGrid.h"

const char* vs_bilateralSplatRgbd =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec4 vPosition;
out vec4 fInputValue;

uniform sampler2D texture0; // src RGBD image
uniform vec2 inputSize;
uniform vec4 gridSize;
uniform vec4 gridPadding;
uniform vec4 sigma;
uniform float inputTime;

vec4 sigmaInv = vec4(1.0) / sigma;

//---------------------------------------------------
// splat and slice must match the functions that determine bilateral input range.
// (0, 0) to (inputSize.z-1, inputSize.w-1) inclusive
vec2 createInputRange(in vec3 rgbSample, float t)
{
	vec2 inputRange = vec2((rgbSample.r+rgbSample.g)/2.0, 0.0);
	//vec2 inputRange = vec2((0.2126*rgbSample.r + 0.7152*rgbSample.g + 0.0722*rgbSample.b), 0.0);
	//return inputRange * (inputSize.zw - vec2(1.0));
	return inputRange * (gridSize.zw - vec2(1.0));
}
//---------------------------------------------------

vec4 encodeGridSample(vec4 gridSample)
{
	// encode the grid sample as [red, green, depth, weight]	
	// weight has to be alpha as we are using glBlend.	
	return vec4(gridSample.r, gridSample.g, gridSample.a, 1.0);
}

vec4 decodeGridSample(vec4 gridSample)
{
	return vec4(gridSample.r, gridSample.g, 0.0, gridSample.b);
}

// downsample high-res input-coord into low-res grid coords.
vec4 gridInputToGridCoord(in vec4 inputCoord, in vec4 sigmaInv, in vec4 gridPadding)
{
	return floor((inputCoord + vec4(0.5)) * sigmaInv) + gridPadding;
}

// project grid coordinate into 2D space.
vec2 gridCoordToRaster(in vec4 gridCoord, in vec4 gridSize)
{
	vec2 invGridRasterSize = vec2(1.0) / vec2(gridSize.x*gridSize.z, gridSize.y*gridSize.w);
	return (gridCoord.xy + vec2(0.5) + vec2(gridCoord.z * gridSize.x, 0.0) + vec2(0.0, gridCoord.w * gridSize.y)) * invGridRasterSize;
}

void main()
{
	// NB:
	// vPosition is at pixel centers of a unit point mesh, i.e. (0.5/inputSize.x, 0.5/inputSize.y) to (1-0.5/inputSize.x, 1-0.5/inputSize.y)
	
	// get unit uvCoord of input (i.e. not the pixel centers)
	vec2 inputHalfPixel = vec2(0.5)/inputSize.xy;
	vec2 inputUvPos = vec2(vPosition.x, vPosition.y) - inputHalfPixel.xy;
	
	// get input position and value from the sample...
	vec4 rgbdSample = texture2D(texture0, inputUvPos + inputHalfPixel.xy);
	vec2 inputRange = createInputRange(rgbdSample.rgb, 0.0); // (0, 0) to (gridSize.z-1, gridSize.w-1)

	// find position in grid and get raster coords...
	vec2 xyCoord = inputUvPos.xy * inputSize.xy; // (0, 0) to (inputSize.x-1, inputSize.y-1)
	vec4 inputCoord = vec4(xyCoord.xy, inputRange); // (0) to (inputSize-1) inclusive
	vec4 gridCoord = gridInputToGridCoord(inputCoord, sigmaInv, gridPadding); // (0) to (gridSize-1) inclusive
	vec2 gridRasterCoord = gridCoordToRaster(gridCoord, gridSize); // (0) to (0.999...)

	// debug:
	//gridRasterCoord = inputUvPos;
	//vec4 debugValue = vec4(inputRange.xy, 0.0,1.0);
	//fInputValue = debugValue;	
	fInputValue = rgbdSample;
	
	// project grid into uv-space...
	gl_Position = vec4(gridRasterCoord.xy * vec2(2.0) - vec2(1.0), 0.0, 1.0);
	gl_PointSize = 1.0;
	/*
	// debug:
	gl_Position = vec4((vPosition.xy*2.0-vec2(1.0)), 0.5, 1.0);
	gl_PointSize = 2.0;
	fInputValue = vec4(inputUvPos.xy,0.0,1.0);
	*/
}
);

const char* fs_bilateralSplat =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec4 fInputValue;
uniform float inputWeight;

vec4 encodeGridSample(vec4 gridSample)
{
	// encode the grid sample as [red, green, depth, weight]	
	// weight has to be alpha as we are using glBlend.	
	return vec4(gridSample.r, gridSample.g, gridSample.a, 1.0);
}

vec4 decodeGridSample(vec4 gridSample)
{
	return vec4(gridSample.r, gridSample.g, 0.0, gridSample.b);
}

//---------------------------------------------------
void main()
{	
	if (fInputValue.a <= 0.0)// || fInputValue.a >= 1.0) // don't splat invalid pixels.
	{
		// debug:
		//gl_FragColor = vec4(1.0,0.0,0.0,0.0);
		//return;
		
		//discard; // assuming we clear to black
		gl_FragColor = vec4(0.0,0.0,0.0,0.0);
		return;
	}
	
	gl_FragColor = encodeGridSample(fInputValue);
	return;
}
);


const char* fs_bilateralSlice =
"#version 300 es \n"
"#extension GL_EXT_gpu_shader4 : enable \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(

uniform ivec2 resolution;
uniform sampler2D texture0; // color-reference image
uniform sampler2D texture1; // bilateral-grid 


uniform vec2 inputSize;
uniform vec4 gridSize;
uniform vec4 gridPadding;
uniform vec4 sigma;

vec4 sigmaInv = vec4(1.0) / sigma;

//---------------------------------------------------
// splat and slice must match the functions that determine bilateral input range.
// (0, 0) to (inputSize.z-1, inputSize.w-1) inclusive
vec2 createInputRange(in vec3 rgbSample, float t)
{
	vec2 inputRange = vec2((rgbSample.r+rgbSample.g)/2.0, rgbSample.g);
	//vec2 inputRange = vec2((0.2126*rgbSample.r + 0.7152*rgbSample.g + 0.0722*rgbSample.b), 0.0);
	//return inputRange * (inputSize.zw - vec2(1.0));
	return inputRange * (gridSize.zw - vec2(1.0));
}
//---------------------------------------------------

vec4 encodeGridSample(vec4 gridSample)
{
	// encode the grid sample as [red, green, depth, weight]	
	// weight has to be alpha as we are using glBlend.	
	return vec4(gridSample.r, gridSample.g, gridSample.a, 1.0);
}

vec4 decodeGridSample(vec4 gridSample)
{
	return vec4(gridSample.r, gridSample.g, 0.0, gridSample.b);
}

// downsample high-res input-coord into low-res grid coords.
vec4 gridInputToGridCoord(in vec4 inputCoord, in vec4 sigmaInv, in vec4 gridPadding)
{
	return ((inputCoord + vec4(0.5)) * sigmaInv) + gridPadding;
}

// project grid coordinate into 2D space.
vec2 gridCoordToRaster(in vec4 gridCoord, in vec4 gridSize)
{
	vec2 invGridRasterSize = vec2(1.0) / vec2(gridSize.x*gridSize.z, gridSize.y*gridSize.w);
	return (gridCoord.xy + vec2(0.5) + vec2(gridCoord.z * gridSize.x, 0.0) + vec2(0.0, gridCoord.w * gridSize.y)) * invGridRasterSize;
}

// project grid coordinate into 2D space. alternative layout with dimensionsZW as rasterXY
vec2 gridCoordToRaster2(in vec4 gridCoord, in vec4 gridSize)
{
	vec2 invGridRasterSize = vec2(1.0) / vec2(gridSize.x*gridSize.z, gridSize.y*gridSize.w);
	return (vec2(0.5) + gridCoord.zw + vec2(gridCoord.x * gridSize.z, 0.0) + vec2(0.0, gridCoord.y * gridSize.w)) * invGridRasterSize;
}

vec4 sampleGrid(in sampler2D gridTexture, in vec4 gridCoord, in vec4 gridSize)
{
	vec2 gridRasterCoord = gridCoordToRaster((vec4(gridCoord.xy-vec2(0.5),floor(gridCoord.zw))), gridSize);
	return texture2D(gridTexture, gridRasterCoord);
}

// doesn't work yet. still need to debug.
vec4 sampleGridLinear(in sampler2D gridTexture, in vec4 gridCoord, in vec4 gridSize)
{
	vec2 invGridRasterSize = vec2(1.0) / vec2(gridSize.x*gridSize.z, gridSize.y*gridSize.w);

	vec2 icoord = floor(gridCoord.zw);		// the integer component
	vec2 fcoord = fract(gridCoord.zw);		// the fractional component

	vec2 xyCoord00 = gridCoord.xy + vec2(gridSize.x * icoord[0], gridSize.y * icoord[1]);
	vec2 xyCoord10 = xyCoord00 + vec2(gridSize.x, 0.0);
	vec2 xyCoord01 = xyCoord00 + vec2(0.0, gridSize.y);
	vec2 xyCoord11 = xyCoord00 + vec2(gridSize.x, gridSize.y);

	vec4 value00 = texture2D(gridTexture, (xyCoord00) * invGridRasterSize);
	vec4 value10 = texture2D(gridTexture, (xyCoord10) * invGridRasterSize);
	vec4 value01 = texture2D(gridTexture, (xyCoord01) * invGridRasterSize);
	vec4 value11 = texture2D(gridTexture, (xyCoord11) * invGridRasterSize);
/*
	vec4 result = vec4(0.0);
	if (value00.a > 0.0)
		result += vec4(value00.rgb, 1.0);
	if (value01.a > 0.0)
		result += vec4(value01.rgb, 1.0);
	if (value10.a > 0.0)
		result += vec4(value10.rgb, 1.0);
	if (value11.a > 0.0)
		result += vec4(value11.rgb, 1.0);	
	if (result.a > 0.0)
		result /= result.a;
	return result;
	*/
	//return mix(value00, value10, fcoord[0]);
	//return value10;
	return value00;
	//return vec4(icoord, fcoord);
	//return mix(value00, value10, fcoord[0]);
	//return mix(value00, value01, fcoord[1]);
	return mix(mix(value00, value01, fcoord[1]), mix(value10, value11, fcoord[1]), fcoord[0]);
}

void main()
{
	vec2 inputHalfPixel = vec2(0.5)/inputSize.xy;
	vec2 inputUvPos = vec2(ivec2(gl_FragCoord.xy))/vec2(resolution);

	// bilateral-position is color intensity
	vec2 referenceSize = vec2(textureSize2D(texture0, 0));
	vec4 referenceSample = texture2D(texture0, inputUvPos.xy + vec2(0.5)/referenceSize);
	//vec4 referenceSample = textureLod(texture0, inputUvPos.xy + vec2(0.5)/referenceSize, 4.0);
	vec2 inputRange = createInputRange(referenceSample.rgb, 0.0);  // (0, 0) to (gridSize.z-1, gridSize.w-1)

	
	vec2 xyCoord = inputUvPos.xy * inputSize.xy; // (0, 0) to (inputSize.x-1, inputSize.y-1)
	vec4 inputCoord = vec4(xyCoord.xy, inputRange);
	vec4 gridCoord = gridInputToGridCoord(inputCoord, sigmaInv, gridPadding);

	vec4 gridSample;
	gridSample = sampleGrid(texture1, gridCoord, gridSize);
	//gridSample = sampleGridLinear(texture1, gridCoord, gridSize);

	//debug:
	vec4 debugValue = vec4(inputRange.xy,0.0,0.0);
	//gl_FragColor = debugValue;
	//return;
	
	if (gridSample.a == 0.0)
	{
		// there is no data in the grid for this pixel.
		gl_FragColor = vec4(1.0,0.0,0.0,0.0);
		//gl_FragColor = thisSparseRgbd;
		return;
	}	

	//gl_FragColor = vec4(referenceSample);
	//return;
	
	// decode the grid sample from [red, green, depth, weight]	
	gl_FragColor = decodeGridSample(gridSample);
}
);

const char* fs_bilateralSliceMerge =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec2 fTexCoords;

uniform sampler2D texture1; // bilateral-grid 
uniform sampler2D texture0; // color-reference image
uniform sampler2D texture2; // this level's rgbd image
uniform sampler2D texture3; // previous upsampled image
uniform vec4 inputSize;
uniform vec4 gridSize;
uniform vec4 gridPadding;
uniform vec4 sigmaInv;

//-----------------------------------------
// splat and slice must match the functions that determine bilateral input range.
// (0, 0) to (inputSize.z-1, inputSize.w-1) inclusive
vec2 createInputRange(in vec3 rgb, float t)
{
	//vec2 inputRange = vec2((0.2126*rgb.r + 0.7152*rgb.g + 0.0722*rgb.b), 0.0);
	//vec2 inputRange = vec2((rgb.r + rgb.g + rgb.b)/3.0, 0.0);
	vec2 inputRange = vec2(clamp(rgb.r*1.0, 0.0, 1.0), clamp(rgb.g*1.0, 0.0, 1.0));
	return inputRange * (inputSize.zw - vec2(1.0));
}
//-----------------------------------------

vec4 gridInputToGridCoord(in vec4 inputCoord, in vec4 sigmaInv, in vec4 gridPadding)
{
	return ((inputCoord + vec4(0.5)) * sigmaInv) + gridPadding;
}

// project grid coordinate into 2D space.
vec2 gridCoordToRaster(in vec4 gridCoord, in vec4 gridSize)
{
	vec2 invGridRasterSize = vec2(1.0) / vec2(gridSize.x*gridSize.z, gridSize.y*gridSize.w);
	return (gridCoord.xy + vec2(0.5) + vec2(gridCoord.z * gridSize.x, gridCoord.w * gridSize.y)) * invGridRasterSize;
}

vec4 sampleGrid(in sampler2D gridTexture, in vec4 gridCoord, in vec4 gridSize)
{
	vec2 gridRasterCoord = gridCoordToRaster(floor(gridCoord), gridSize);
	return texture2D(gridTexture, gridRasterCoord);
}

vec4 sampleGridLinear(in sampler2D gridTexture, in vec4 gridCoord, in vec4 gridSize)
{
	vec2 invGridRasterSize = vec2(1.0) / vec2(gridSize.x*gridSize.z, gridSize.y*gridSize.w);

	vec2 icoord = floor(gridCoord.zw);		// the integer component
	vec2 fcoord = fract(gridCoord.zw);		// the fractional component

	vec2 xyCoord00 = gridCoord.xy + vec2(0.5) + vec2(gridSize.x * icoord[0], gridSize.y * icoord[1]);
	vec2 xyCoord10 = xyCoord00 + vec2(gridSize.x, 0.0);
	vec2 xyCoord01 = xyCoord00 + vec2(0.0, gridSize.y);
	vec2 xyCoord11 = xyCoord00 + vec2(gridSize.x, gridSize.y);

	vec4 value00 = texture2D(gridTexture, xyCoord00 * invGridRasterSize);
	vec4 value10 = texture2D(gridTexture, xyCoord10 * invGridRasterSize);
	vec4 value01 = texture2D(gridTexture, xyCoord01 * invGridRasterSize);
	vec4 value11 = texture2D(gridTexture, xyCoord11 * invGridRasterSize);

	return value00;
	//return mix(value00, value10, fcoord[0]);
	//return mix(value00, value01, fcoord[1]);
	//return mix(mix(value00, value01, fcoord[1]), mix(value10, value11, fcoord[1]), fcoord[0]);
}

void main()
{
	vec2 xyCoord = fTexCoords.xy * inputSize.xy; // (0, 0) to (inputSize.x-1, inputSize.y-1)

	// get bilateral-position from color.
	vec4 colorSample = texture2D(texture0, fTexCoords.xy);
	//vec4 colorSample = textureLod(texture0, fTexCoords.xy, 4.0);
	vec2 inputRange = createInputRange(colorSample.rgb, 0.0);  // (0, 0) to (inputSize.z-1, inputSize.w-1)

	vec4 inputCoord = vec4(xyCoord.xy, inputRange);
	vec4 gridCoord = gridInputToGridCoord(inputCoord, sigmaInv, gridPadding);
	vec4 gridValue = sampleGrid(texture1, gridCoord, gridSize);
	//vec4 gridValue = sampleGridLinear(texture1, gridCoord, gridSize);

	vec4 prevUpsampledRgbd = texture2D(texture3, fTexCoords.xy);
	vec4 sparseRgbd = texture2D(texture2, fTexCoords.xy);

	if (gridValue.a == 0.0)
	{
		// there are no samples at this position in the grid.
		//gl_FragColor = vec4(1.0);
		gl_FragColor = prevUpsampledRgbd;
		return;
	}
	/*
	if (gridValue.r >= 1.0)
	{
		// make sure that the depth isn't out of range.
		gridValue.r = 0.99;
	}
	*/
	vec4 newCol;

	if (prevUpsampledRgbd.a >= 1.0)
	{
		// the parent image (lower res) has invalid depth.
		// this color shows up on the edges of the AR image.
		gl_FragColor = prevUpsampledRgbd;//vec4(0.0,0.0,0.0,1.1);
		//discard;
		return;
	}

	
	if (sparseRgbd.a < 1.0)
	{
		// this pixel has a valid color from the pointcloud, so we should use it.
		newCol = sparseRgbd.rgba;
	}
	else
	{
		// otherwise it is a new pixel, so we take the color from the reference color image.
		newCol = vec4(colorSample.rgb, gridValue.r);
	}

	// debug:
	//if (newCol.a >= 0.95)
	//	newCol = vec4(1.0, 1.0, 0.0, 1.1);

	gl_FragColor = newCol;
}
);

const char* fs_gaussianBlur =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(

uniform sampler2D texture0; // bilateral-grid
uniform ivec2 delta;		// delta of blur
uniform float weight;		// blurring weight (this can be used to show confidence in these values)

void main()
{
	ivec2 coord = ivec2(gl_FragCoord.xy);
	vec4 d0 = texelFetch(texture0, coord -delta * 2, 0);
	vec4 d1 = texelFetch(texture0, coord -delta, 0);
	vec4 d2 = texelFetch(texture0, coord, 0);
	vec4 d3 = texelFetch(texture0, coord + delta, 0);
	vec4 d4 = texelFetch(texture0, coord + delta * 2, 0);
	gl_FragColor = vec4(weight * (0.13533528323661 * (d0 + d4) + 0.60653065971263 * (d1 + d3)) + d2);
}

);

const char* fs_unpremultiplyAlpha =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec2 fTexCoords;

uniform sampler2D texture0;

void main()
{
	vec4 col = texelFetch(texture0, ivec2(gl_FragCoord.xy), 0);
	vec3 normRgb = (col.a == 0.0) ? vec3(0.0, 0.0, 0.0) : col.rgb / col.a;
	gl_FragColor = vec4(normRgb, 1.0);
}
);


GlBilateralGrid::GlBilateralGrid()
	:
	bilateralSplatRgbd_(vs_bilateralSplatRgbd, fs_bilateralSplat),
	bilateralSlice_(vs_simpleTexture, fs_bilateralSlice),
	bilateralSliceMerge_(vs_simpleTexture, fs_bilateralSliceMerge),
	gaussianBlur_(vs_simpleTexture, fs_gaussianBlur),
	normalizeMaterial_(vs_simpleTexture, fs_unpremultiplyAlpha)
{
	gridRasterWidth = gridRasterHeight = 0;
	gridMesh_ = 0;
}

GlBilateralGrid::~GlBilateralGrid()
{
	delete gridMesh_;
}

void GlBilateralGrid::setup(const glm::vec4& inputSize, const glm::vec4& sigma, const glm::vec4& padding)
{
	gridInputSize[0] = std::max(1.f, inputSize[0]);
	gridInputSize[1] = std::max(1.f, inputSize[1]);
	gridInputSize[2] = std::max(1.f, inputSize[2]);
	gridInputSize[3] = std::max(1.f, inputSize[3]);
	gridPadding[0] = std::max(0.f, padding[0]);
	gridPadding[1] = std::max(0.f, padding[1]);
	gridPadding[2] = std::max(0.f, padding[2]);
	gridPadding[3] = std::max(0.f, padding[3]);
	gridSigma[0] = std::max(1.f, sigma[0]); // spatialX
	gridSigma[1] = std::max(1.f, sigma[1]); // spatialY
	gridSigma[2] = std::max(1.f, sigma[2]); // range1
	gridSigma[3] = std::max(1.f, sigma[3]); // range2

	// compute grid size...
	gridSize[0] = int((gridInputSize[0] - 1) / gridSigma[0]) + 1 + 2 * gridPadding[0];
	gridSize[1] = int((gridInputSize[1] - 1) / gridSigma[1]) + 1 + 2 * gridPadding[1];
	gridSize[2] = int((gridInputSize[2] - 1) / gridSigma[2]) + 1 + 2 * gridPadding[2];
	gridSize[3] = int((gridInputSize[3] - 1) / gridSigma[3]) + 1 + 2 * gridPadding[3];

	gridRasterWidth = gridSize[0] * gridSize[2];
	gridRasterHeight = gridSize[1] * gridSize[3];
	/*
	// print for debugging:
	CT4(gridInputSize[0], gridInputSize[1], gridInputSize[2], gridInputSize[3]);
	CT4(gridSigma[0], gridSigma[1], gridSigma[2], gridSigma[3]);
	CT4(gridPadding[0], gridPadding[1], gridPadding[2], gridPadding[3]);
	CT4(gridSize[0], gridSize[1], gridSize[2], gridSize[3]);
	CT2(gridRasterWidth, gridRasterHeight);
	*/

	fbo_ = GlFramebufferPtr::create();
	gridTextures_[0] = GlTexturePtr::create(GL_TEXTURE_2D, gridRasterWidth, gridRasterHeight, GL_RGBA32F);
	gridTextures_[1] = GlTexturePtr::create(GL_TEXTURE_2D, gridRasterWidth, gridRasterHeight, GL_RGBA32F);

	gridMesh_ = new GlPlaneMesh(gridInputSize[0], gridInputSize[1]);
	quad_ = new GlQuad();
}

void GlBilateralGrid::clear()
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[0]->id, 0);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlBilateralGrid::splatRgbd(const GlTexturePtr& srcRgbdTexture, float inputTime, float weight)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);

	glDisable(GL_DEPTH_TEST);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[0]->id, 0);
	GlUtil::checkFramebuffer();

	glViewport(0, 0, gridRasterWidth, gridRasterHeight);

	// we splat high-res pixels with accumulation
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);

	// this allows us to have non binary weights specified in the alpha channel.
	// the shader will have to be changed to not premultiply. (see the splat shader)
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE); 

	// this enables us to ensure we have 1 pixel for the splat (scatter) pass.
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	glUseProgram(bilateralSplatRgbd_.shader_program_);

	// predefine some uniforms...
	GLuint loc;
	loc = glGetUniformLocation(bilateralSplatRgbd_.shader_program_, "sigma");
	glUniform4f(loc, gridSigma[0], gridSigma[1], gridSigma[2], gridSigma[3]);
	loc = glGetUniformLocation(bilateralSplatRgbd_.shader_program_, "gridPadding");
	glUniform4f(loc, gridPadding[0], gridPadding[1], gridPadding[2], gridPadding[3]);
	loc = glGetUniformLocation(bilateralSplatRgbd_.shader_program_, "gridSize");
	glUniform4f(loc, gridSize[0], gridSize[1], gridSize[2], gridSize[3]);
	loc = glGetUniformLocation(bilateralSplatRgbd_.shader_program_, "inputSize");
	glUniform2f(loc, gridInputSize[0], gridInputSize[1]);
	loc = glGetUniformLocation(bilateralSplatRgbd_.shader_program_, "inputWeight");
	glUniform1f(loc, weight);
	loc = glGetUniformLocation(bilateralSplatRgbd_.shader_program_, "inputTime");
	glUniform1f(loc, inputTime);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcRgbdTexture->id);

	gridMesh_->render(glm::mat4(1.0), glm::mat4(1.0), bilateralSplatRgbd_);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	blurAndNormalize_();

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlBilateralGrid::splatRgbAndDepth(GLuint srcColorTextureId, GLuint srcDepthTextureId, float inputTime, float weight)
{
	assert(srcColorTextureId != 0 && srcDepthTextureId != 0);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);

	// splat pass...
	// scatter the sparse, high-res range-data into the low-res bilateral-grid.
	// QUESTION:
	// should we use low-res downsampled range-data here or raw high-res?
	{
		glDisable(GL_DEPTH_TEST);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[0]->id, 0);
		GlUtil::checkFramebuffer();

		glViewport(0, 0, gridRasterWidth, gridRasterHeight);

		// we splat high-res pixels with accumulation
		glEnable(GL_BLEND);
		//glBlendFunc(GL_SRC_ALPHA, GL_ONE); // this allows us to have non binary weights
		glBlendFunc(GL_ONE, GL_ONE); // debugging: this will show all cells that have ANY values (the normalize pass later will zero empty grid cells)
		glBlendEquation(GL_FUNC_ADD);

		glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
		glUseProgram(bilateralSplat_.shader_program_);
		{
			GLuint loc;
			loc = glGetUniformLocation(bilateralSplat_.shader_program_, "sigma");
			glUniform4f(loc, gridSigma[0], gridSigma[1], gridSigma[2], gridSigma[3]);
			loc = glGetUniformLocation(bilateralSplat_.shader_program_, "gridPadding");
			glUniform4f(loc, gridPadding[0], gridPadding[1], gridPadding[2], gridPadding[3]);
			loc = glGetUniformLocation(bilateralSplat_.shader_program_, "gridSize");
			glUniform4f(loc, gridSize[0], gridSize[1], gridSize[2], gridSize[3]);
			loc = glGetUniformLocation(bilateralSplat_.shader_program_, "inputSize");
			glUniform4f(loc, gridInputSize[0], gridInputSize[1], gridInputSize[2], gridInputSize[3]);
			loc = glGetUniformLocation(bilateralSplat_.shader_program_, "inputWeight");
			glUniform1f(loc, weight);
			loc = glGetUniformLocation(bilateralSplat_.shader_program_, "inputTime");
			glUniform1f(loc, inputTime);
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, srcColorTextureId);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, srcDepthTextureId);

		gridMesh_->render(glm::mat4(1.0), glm::mat4(1.0), bilateralSplat_);

		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);


	}

	blurAndNormalize_();

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlBilateralGrid::blurAndNormalize_()
{
	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);

	// blur pass...
	if (1)
	{
		GLuint deltaLoc, textureSize0Loc, weightLoc;

		weightLoc = glGetUniformLocation(gaussianBlur_.shader_program_, "weight");
		deltaLoc = glGetUniformLocation(gaussianBlur_.shader_program_, "delta");

		glUseProgram(gaussianBlur_.shader_program_);

		

		// do passes for spatial gaussian blur
		glUniform1f(weightLoc, 1.0);

		for (int p = 0; p < 3; ++p)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[1]->id, 0);
			GlUtil::checkFramebuffer();
			glUniform2i(deltaLoc, 1, 0);
			glBindTexture(GL_TEXTURE_2D, gridTextures_[0]->id);
			quad_->render(glm::mat4(1.0), glm::mat4(1.0), gaussianBlur_);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[0]->id, 0);
			GlUtil::checkFramebuffer();
			glUniform2i(deltaLoc, 0, 1);
			glBindTexture(GL_TEXTURE_2D, gridTextures_[1]->id);
			quad_->render(glm::mat4(1.0), glm::mat4(1.0), gaussianBlur_);
		}

		// passes across intensity are less weight, so spatial blur takes priority.
		glUniform1f(weightLoc, 0.001);

		for (int p = 0; p < 3; ++p)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[1]->id, 0);
			GlUtil::checkFramebuffer();
			glUniform2i(deltaLoc, gridSize[0], 0);
			glBindTexture(GL_TEXTURE_2D, gridTextures_[0]->id);
			quad_->render(glm::mat4(1.0), glm::mat4(1.0), gaussianBlur_);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[0]->id, 0);
			GlUtil::checkFramebuffer();
			glUniform2i(deltaLoc, 0, gridSize[1]);
			glBindTexture(GL_TEXTURE_2D, gridTextures_[1]->id);
			quad_->render(glm::mat4(1.0), glm::mat4(1.0), gaussianBlur_);
		}

		glUseProgram(0);
	}

	// normalize...
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTextures_[1]->id, 0);
	GlUtil::checkFramebuffer();
	glBindTexture(GL_TEXTURE_2D, gridTextures_[0]->id);
	quad_->render(glm::mat4(1.0), glm::mat4(1.0), normalizeMaterial_);

	glUseProgram(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void GlBilateralGrid::slice(GLuint referenceTextureId, const GlTexturePtr& dstTexture)
{
	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTexture->id, 0);
	GlUtil::checkFramebuffer();

	glViewport(0, 0, dstTexture->width, dstTexture->height);

	glUseProgram(bilateralSlice_.shader_program_);
	GLuint loc;
	loc = glGetUniformLocation(bilateralSlice_.shader_program_, "sigma");
	glUniform4f(loc, gridSigma[0], gridSigma[1], gridSigma[2], gridSigma[3]);
	loc = glGetUniformLocation(bilateralSlice_.shader_program_, "gridPadding");
	glUniform4f(loc, gridPadding[0], gridPadding[1], gridPadding[2], gridPadding[3]);
	loc = glGetUniformLocation(bilateralSlice_.shader_program_, "gridSize");
	glUniform4f(loc, gridSize[0], gridSize[1], gridSize[2], gridSize[3]);
	loc = glGetUniformLocation(bilateralSlice_.shader_program_, "inputSize");
	glUniform2f(loc, gridInputSize[0], gridInputSize[1]);
	loc = glGetUniformLocation(bilateralSlice_.shader_program_, "resolution");
	glUniform2i(loc, dstTexture->width, dstTexture->height);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, referenceTextureId);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gridTextures_[1]->id);

	quad_->render(glm::mat4(1.0), glm::mat4(1.0), bilateralSlice_);

	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void GlBilateralGrid::sliceMerge(const GlTexturePtr& refRgbTexture, const GlTexturePtr& prevUpsampleTexture, const GlTexturePtr& rgbdTexture, const GlTexturePtr& resultRgbdTexture)
{
	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resultRgbdTexture->id, 0);
	GlUtil::checkFramebuffer();

	glViewport(0, 0, resultRgbdTexture->width, resultRgbdTexture->height);
	glClearColor(0, 0, 0, 1.1);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(bilateralSliceMerge_.shader_program_);
	GLuint loc;
	loc = glGetUniformLocation(bilateralSlice_.shader_program_, "sigma");
	glUniform4f(loc, gridSigma[0], gridSigma[1], gridSigma[2], gridSigma[3]);
	loc = glGetUniformLocation(bilateralSliceMerge_.shader_program_, "gridPadding");
	glUniform4f(loc, gridPadding[0], gridPadding[1], gridPadding[2], gridPadding[3]);
	loc = glGetUniformLocation(bilateralSliceMerge_.shader_program_, "gridSize");
	glUniform4f(loc, gridSize[0], gridSize[1], gridSize[2], gridSize[3]);
	loc = glGetUniformLocation(bilateralSliceMerge_.shader_program_, "inputSize");
	glUniform2f(loc, gridInputSize[0], gridInputSize[1]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, refRgbTexture->id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gridTextures_[1]->id);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, rgbdTexture->id);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, prevUpsampleTexture->id);

	quad_->render(glm::mat4(1.0), glm::mat4(1.0), bilateralSliceMerge_);

	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}
