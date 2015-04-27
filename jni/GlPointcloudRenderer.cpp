
#include "GlPointcloudRenderer.h"

static const char kVertexShader[] =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec4 vPosition;
in vec3 vColor;
uniform mat4 worldToViewProjMat;
out vec3 fColor;
uniform float pointSize;

void main() 
{
	gl_PointSize = pointSize;
	gl_Position = worldToViewProjMat * vec4(vPosition.xyz, 1.0);
	/*float depth = gl_Position.z / gl_Position.w;
	float nearClip = 0.5 * 1.0;
	float farClip = 6.0 * 1.0;
	float linearDepth = ((gl_Position.w) - nearClip) / (farClip - nearClip);*/
	fColor = vColor;
});

static const char kFragmentShader[] =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec3 fColor;
void main() {
	gl_FragColor = vec4(fColor.rgb, 1.0);//gl_FragCoord.z);
});


static const char* vs_colorFromTexture =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec3 vPosition;
out vec3 fColor;
uniform mat4 worldToViewProjMat;

uniform sampler2D texture0;
uniform mat4 depthViewToWorldMat;
uniform mat4 colorWorldToViewMat;
uniform mat4 colorViewProjMat;

vec3 pseudoTemperature(float t)
{
	float b = t < 0.25f ? smoothstep( -0.25f, 0.25f, t ) : 1.0f-smoothstep( 0.25f, 0.5f, t );
	float g = t < 0.5f  ? smoothstep( 0.0f, 0.5f, t ) : (t < 0.75f ? 1.0f : 1.0f-smoothstep( 0.75f, 1.0f, t ));
	float r = smoothstep( 0.5f, 0.75f, t );
	return vec3( r, g, b );
}

void main() 
{
	vec4 dstNdcPos = colorViewProjMat * colorWorldToViewMat * depthViewToWorldMat * vec4(vPosition.xyz, 1.0);
	dstNdcPos.xyz = dstNdcPos.xyz / dstNdcPos.w;
	vec2 dstUvPos = dstNdcPos.xy * vec2(0.5) + vec2(0.5);
	vec3 dstTexel = texture2D(texture0, dstUvPos.xy).rgb;
	//vec3 dstTexel = textureLod(texture0, dstUvPos.xy, 4.0).rgb;

	fColor = dstTexel;
	/*
	// DEBUG: use depth temperature...
	float nearClip = 0.5 * 1.0;
	float farClip = 6.0 * 1.0;
	float linearDepth = ((dstNdcPos.w) - nearClip) / (farClip - nearClip);
	fColor = pseudoTemperature(linearDepth).rgb;*/
});

static const char fs_compositeShader[] =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0;
uniform vec4 color;
in vec2 fTexCoords;
uniform float confidence;

vec3 sobel(sampler2D tex, ivec2 center)
{
	vec3 tleft = texelFetchOffset(tex, center, 0, ivec2(-1, 1)).rgb;
	vec3 left = texelFetchOffset(tex, center, 0, ivec2(-1, 0)).rgb;
	vec3 bleft = texelFetchOffset(tex, center, 0, ivec2(-1, -1)).rgb;
	vec3 top = texelFetchOffset(tex, center, 0, ivec2(0, 1)).rgb;
	vec3 bottom = texelFetchOffset(tex, center, 0, ivec2(0, -1)).rgb;
	vec3 tright = texelFetchOffset(tex, center, 0, ivec2(1, 1)).rgb;
	vec3 right = texelFetchOffset(tex, center, 0, ivec2(1, 0)).rgb;
	vec3 bright = texelFetchOffset(tex, center, 0, ivec2(1, -1)).rgb;

	//        1 0 -1     -1 -2 -1
	//    X = 2 0 -2  Y = 0  0  0
	//        1 0 -1      1  2  1

	vec3 dx = tleft + 2.0*left + bleft - tright - 2.0*right - bright;
	vec3 dy = -tleft - 2.0*top - tright + bleft + 2.0 * bottom + bright;
	vec3 grad = dx * dx + dy * dy;

	// returned the squared result.
	return vec3((grad.r), (grad.g), (grad.b));
}

void main()
{
	vec4 tex = texture2D(texture0, fTexCoords).rgba;
	vec3 edgeFactor = vec3(0.0);//sobel(texture0, ivec2(fTexCoords.xy*textureSize0.xy));
	if (edgeFactor.x > 0.3 || tex.r == 1.0)
	{
		tex.r = 1.0;
		discard;
	}
	gl_FragColor.rgb = color.rgb * tex.rgb;
	gl_FragColor.a = confidence;// (edgeFactor.x > 0.5)?0.0:1.0;
}
);




static const char fs_warpColorToRGBD[] =
"#version 300 es \n"
"#extension GL_OES_EGL_image_external : require\n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
uniform sampler2D texture0;
uniform samplerExternalOES texture1;
in vec2 fTexCoords;
uniform mat4 depthViewToWorldMat;
uniform mat4 depthViewProjInvMat;
uniform mat4 colorWorldToViewMat;
uniform mat4 colorViewProjMat;

void main()
{
	// read depth
	float fragDepth = texture2D(texture0, fTexCoords).r;
	// convert the fragment depth to a world-position...
	vec3 ndc = vec3((fTexCoords.x * 2.0 - 1.0), (fTexCoords.y * 2.0 - 1.0), (fragDepth * 2.0 - 1.0));
	vec4 vpos = depthViewProjInvMat * vec4(ndc, 1.0);
	vpos.xyz = vpos.xyz / vpos.w;
	vec4 wpos = depthViewToWorldMat * vec4(vpos.xyz, 1.0);
	// project the world-position into the color-image
	vec4 cPosColor = colorViewProjMat * colorWorldToViewMat * wpos;
	cPosColor.xyz = cPosColor.xyz / cPosColor.w;
	vec2 colorTexCoords = cPosColor.xy * vec2(0.5, -0.5) + vec2(0.5);
	// read color
	vec4 colorTexel = texture2D(texture1, colorTexCoords).rgba;

	gl_FragColor.rgb = vec3(fragDepth, 0.0, 0.0);//colorTexel.rgb;
	// store depth in alpha
	gl_FragColor.a = 1.0;//fragDepth;

	//gl_FragDepth = fragDepth;
}
);

static const glm::mat4 inverse_z_mat = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, -1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f);

GlPointcloudRenderer::GlPointcloudRenderer(int width, int height, int numLevels)
	:
	setColorMaterial_(vs_simpleTexture, fs_simpleTexture2d),
	compositeMaterial_(vs_simpleTexture, fs_compositeShader),
	passThruMaterial_(vs_simpleTexture, fs_simpleTexture2d_r),
	reduceMaterial_(vs_simpleTexture, fs_reduceTexture_max),
	reduceRgbdMaterial_(vs_simpleTexture, fs_reduceRgbd),
	reduceColorMaterial_(vs_simpleTexture, fs_reduceColor),
	mergeColorMaterial_(vs_simpleTexture, fs_mergeColorFromDepth),
	setRgbdMaterial_(vs_simpleTexture, fs_setRgbd),
	holeFillMaterial_(vs_simpleTexture, fs_holeFill)
{
	assert(width > 0 && height > 0 && numLevels > 0);

	width_ = width;
	height_ = height;
	numLevels_ = numLevels;

	mesh_ = new GlPlaneMesh(width, height);

	warpMeshMaterial = new GlMaterial(vs_warpMeshRGBD, fs_warpRGBD);
	warpColorMaterial_ = new GlMaterial(vs_simpleTexture, fs_warpColorToRGBD);
	mergeRgbdMaterial_ = new GlMaterial(vs_simpleTexture, fs_projectColorFromDepth);


	colorTexturePyramid_ = new GlTexturePtr[numLevels_];
	depthTexturePyramid_ = new GlTexturePtr[numLevels_];
	depthUpsampleTexture_ = new GlTexturePtr[numLevels_];

	for (int l = 0; l < numLevels_; ++l)
	{		
		depthTexturePyramid_[l] = GlTexturePtr::create(GL_TEXTURE_2D, width_ >> l, height_ >> l, GL_RGBA32F);	
		//glBindTexture(GL_TEXTURE_2D, depthTexturePyramid_[l]->id);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		depthUpsampleTexture_[l] = GlTexturePtr::create(GL_TEXTURE_2D, width_ >> l, height_ >> l, GL_RGBA32F);
		//glBindTexture(GL_TEXTURE_2D, depthUpsampleTexture_[l]->id);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		colorTexturePyramid_[l] = GlTexturePtr::create(GL_TEXTURE_2D, width_ >> l, height_ >> l, GL_RGBA32F);
		//glBindTexture(GL_TEXTURE_2D, colorTexturePyramid_[l]->id);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	fbo_ = GlFramebufferPtr::create();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTexturePyramid_[0]->id, 0);

	/*
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextureIds_[0], 0);
	*/

	glGenRenderbuffers(1, &depthRenderBufferId_);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBufferId_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width_, height_);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBufferId_);
	GlUtil::checkFramebuffer();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	quad_ = new GlQuad();

	rgbdTexture_ = GlTexturePtr::create(GL_TEXTURE_2D, width_, height_, GL_RGBA32F);

	GlUtil::CheckGlError("GlPointcloudRenderer()");
}

GlPointcloudRenderer::~GlPointcloudRenderer()
{
	delete[] depthTexturePyramid_;
	delete quad_;
	quad_ = 0;
}

void GlPointcloudRenderer::renderToTextureClear(GLuint destColorTextureId, GLuint destDepthTextureId, int destWidth, int destHeight)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, destDepthTextureId, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destColorTextureId, 0);
	GlUtil::checkFramebuffer();

	glClearDepthf(1);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, destWidth, destHeight);
}

void GlPointcloudRenderer::renderToTexture(GlPointcloud* pointcloud, float pointSize, GLuint destColorTextureId, GLuint destDepthTextureId, int destWidth, int destHeight,
	glm::mat4 viewProjectionMat, glm::mat4 worldToViewMat,
	const GlMaterial& mat)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, destDepthTextureId, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destColorTextureId, 0);	
	GlUtil::checkFramebuffer();

	pointcloud->render(viewProjectionMat, worldToViewMat, mat, pointSize);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
}

void GlPointcloudRenderer::buildRgbdPyramid(const GlTexturePtr& srcColorTexture, const GlTexturePtr& srcDepthTexture, int numLevels)
{
	if (numLevels > numLevels_)
		numLevels = numLevels_;

	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		
	// copy the depthTexture into the first level.
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTexturePyramid_[0]->id, 0);
	glViewport(0, 0, depthTexturePyramid_[0]->width, depthTexturePyramid_[0]->height);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcDepthTexture->id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, srcColorTexture->id);
	quad_->render(glm::mat4(1.0), glm::mat4(1.0), setRgbdMaterial_);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	// reduce using the MIN operator (to always take the nearer depth value from the valid pixels)...
	for (int l = 1; l < numLevels; ++l)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTexturePyramid_[l]->id, 0);
		glViewport(0, 0, depthTexturePyramid_[0]->width >> l, depthTexturePyramid_[0]->height >> l);

		glBindTexture(GL_TEXTURE_2D, depthTexturePyramid_[l-1]->id);
		quad_->render(glm::mat4(1.0), glm::mat4(1.0), reduceRgbdMaterial_);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlPointcloudRenderer::buildColorPyramid(const GlTexturePtr& srcColorTexture, int numLevels)
{
	if (numLevels > numLevels_)
		numLevels = numLevels_;

	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		
	// copy the texture into the first level.
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexturePyramid_[0]->id, 0);
	glViewport(0, 0, colorTexturePyramid_[0]->width, colorTexturePyramid_[0]->height);
	
	glBindTexture(GL_TEXTURE_2D, srcColorTexture->id);
	quad_->render(glm::mat4(1.0), glm::mat4(1.0), setColorMaterial_);
	glBindTexture(GL_TEXTURE_2D, 0);

	// reduce using the MIN operator (to always take the nearer depth value from the valid pixels)...
	for (int l = 1; l < numLevels; ++l)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexturePyramid_[l]->id, 0);
		glViewport(0, 0, colorTexturePyramid_[0]->width >> l, colorTexturePyramid_[0]->height >> l);

		glBindTexture(GL_TEXTURE_2D, colorTexturePyramid_[l-1]->id);
		quad_->render(glm::mat4(1.0), glm::mat4(1.0), reduceColorMaterial_);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlPointcloudRenderer::fillHoles(GLuint destTextureId, int destWidth, int destHeight, GLuint srcDepthTextureId)
{
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		
	// copy the depthTexture into the first level.
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTexturePyramid_[0]->id, 0);
	glViewport(0, 0, width_, height_);
	glBindTexture(GL_TEXTURE_2D, srcDepthTextureId);
	quad_->render(glm::mat4(1.0), glm::mat4(1.0), passThruMaterial_);

	// reduce using the MAX operator (to always take the farther depth value from the non transparent pixels)...
	for (int l = 1; l < numLevels_; ++l)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTexturePyramid_[l]->id, 0);
		glViewport(0, 0, width_ >> l, height_ >> l);

		glBindTexture(GL_TEXTURE_2D, depthTexturePyramid_[l-1]->id);
		quad_->render(glm::mat4(1.0), glm::mat4(1.0), reduceMaterial_);
	}

	// now merge into composite depth image...

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTextureId, 0);
	GlUtil::checkFramebuffer();

	glClearColor(1, 1, 1, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, destWidth, destHeight);

	glDisable(GL_BLEND);

	// use additive blending
	//glEnable(GL_BLEND);
	//glBlendEquation(GL_FUNC_ADD);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// use minimum blending
	//glEnable(GL_BLEND);
	//glBlendFunc( GL_ZERO, GL_ZERO );
	//glBlendEquation(GL_MIN);

	glUseProgram(compositeMaterial_.shader_program_);
	GLuint confidenceLoc = glGetUniformLocation(compositeMaterial_.shader_program_, "confidence");
	

	for (int l = numLevels_ - 1; l >= 0; --l)
	{
		glUniform1f(confidenceLoc, float(numLevels_-l)/numLevels_);
		glBindTexture(GL_TEXTURE_2D, depthTexturePyramid_[l]->id);
		quad_->render(glm::mat4(1.0), glm::mat4(1.0), compositeMaterial_);
	}

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlPointcloudRenderer::projectColor(GLuint destTextureId, int destWidth, int destHeight,
	const glm::mat4& depthViewToWorldMat, const glm::mat4& depthViewProjMat, GLuint depthTextureId,
	const glm::mat4& colorViewToWorldMat, const glm::mat4& colorViewProjMat, GLuint colorTextureId
	)
{
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTextureId, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	GlUtil::checkFramebuffer();

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glViewport(0, 0, destWidth, destHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthTextureId);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, colorTextureId);

	glUseProgram(mergeRgbdMaterial_->shader_program_);
	{
		GLuint depthViewToWorldMatLoc = glGetUniformLocation(mergeRgbdMaterial_->shader_program_, "depthViewToWorldMat");
		GLuint depthViewProjInvMatLoc = glGetUniformLocation(mergeRgbdMaterial_->shader_program_, "depthViewProjInvMat");
		GLuint colorWorldToViewMatLoc = glGetUniformLocation(mergeRgbdMaterial_->shader_program_, "colorWorldToViewMat");
		GLuint colorViewProjMatLoc = glGetUniformLocation(mergeRgbdMaterial_->shader_program_, "colorViewProjMat");
		glUniformMatrix4fv(depthViewToWorldMatLoc, 1, GL_FALSE, glm::value_ptr(depthViewToWorldMat));
		glUniformMatrix4fv(depthViewProjInvMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(depthViewProjMat)));
		glUniformMatrix4fv(colorWorldToViewMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(colorViewToWorldMat)));
		glUniformMatrix4fv(colorViewProjMatLoc, 1, GL_FALSE, glm::value_ptr(colorViewProjMat));
	}

	quad_->render(glm::mat4(1.0), glm::mat4(1.0), *mergeRgbdMaterial_);

	glUseProgram(0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glEnable(GL_DEPTH_TEST);
}

void GlPointcloudRenderer::mergeColor(GLuint destTextureId, int destWidth, int destHeight,
	GLuint currentColorTextureId,
	const glm::mat4& depthViewToWorldMat, const glm::mat4& depthViewProjMat, GLuint depthTextureId,
	const glm::mat4& colorViewToWorldMat, const glm::mat4& colorViewProjMat, GLuint colorTextureId
	)
{
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTextureId, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	GlUtil::checkFramebuffer();

	glClearColor(0, 1, 1, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glViewport(0, 0, destWidth, destHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthTextureId);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, colorTextureId);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, currentColorTextureId);
	

	glUseProgram(mergeColorMaterial_.shader_program_);
	{
		GLuint depthViewToWorldMatLoc = glGetUniformLocation(mergeColorMaterial_.shader_program_, "depthViewToWorldMat");
		GLuint depthViewProjInvMatLoc = glGetUniformLocation(mergeColorMaterial_.shader_program_, "depthViewProjInvMat");
		GLuint colorWorldToViewMatLoc = glGetUniformLocation(mergeColorMaterial_.shader_program_, "colorWorldToViewMat");
		GLuint colorViewProjMatLoc = glGetUniformLocation(mergeColorMaterial_.shader_program_, "colorViewProjMat");
		glUniformMatrix4fv(depthViewToWorldMatLoc, 1, GL_FALSE, glm::value_ptr(depthViewToWorldMat));
		glUniformMatrix4fv(depthViewProjInvMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(depthViewProjMat)));
		glUniformMatrix4fv(colorWorldToViewMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(colorViewToWorldMat)));
		glUniformMatrix4fv(colorViewProjMatLoc, 1, GL_FALSE, glm::value_ptr(colorViewProjMat));
	}

	quad_->render(glm::mat4(1.0), glm::mat4(1.0), mergeColorMaterial_);

	glUseProgram(0);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glEnable(GL_DEPTH_TEST);
}

void GlPointcloudRenderer::warpImage(GlPlaneMesh* mesh, const glm::mat4& dstWorldToViewMat, const glm::mat4& dstViewProjMat, 
	GLuint destColorTextureId, GLuint destDepthTextureId, int destWidth, int destHeight, int pointSize, 
	const glm::mat4& srcViewToWorldMat, const glm::mat4& srcViewProjMat, GLuint srcColorTextureId, GLuint srcDepthTextureId,
	float srcNearClip, float srcFarClip)
{
	glViewport(0, 0, destWidth, destHeight);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	//glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// first we forward-warp the depth data...
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destColorTextureId, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, destDepthTextureId, 0);
	GlUtil::checkFramebuffer();

	// set the bg to RGBD: (testcolor, 1)
	glClearColor(1, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(warpMeshMaterial->shader_program_);
	{
		GLuint srcViewToWorldMatLoc = glGetUniformLocation(warpMeshMaterial->shader_program_, "srcViewToWorldMat");
		GLuint srcViewProjInvMatLoc = glGetUniformLocation(warpMeshMaterial->shader_program_, "srcViewProjInvMat");
		GLuint nearClipLoc = glGetUniformLocation(warpMeshMaterial->shader_program_, "srcNearClip");
		GLuint farClipLoc = glGetUniformLocation(warpMeshMaterial->shader_program_, "srcFarClip");
		glUniformMatrix4fv(srcViewToWorldMatLoc, 1, GL_FALSE, glm::value_ptr(srcViewToWorldMat));
		glUniformMatrix4fv(srcViewProjInvMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(srcViewProjMat)));
		glUniform1f(nearClipLoc, srcNearClip);
		glUniform1f(farClipLoc, srcFarClip);

		GLuint srcImageModeLoc = glGetUniformLocation(warpMeshMaterial->shader_program_, "srcImageMode");
		if (srcDepthTextureId == 0)
			glUniform1i(srcImageModeLoc, 1); // srcColorTextureId is RGBD
		else	
			glUniform1i(srcImageModeLoc, 0); // srcColorTextureId & srcDepthTextureId are valid.

		GLuint pointSizeLoc = glGetUniformLocation(warpMeshMaterial->shader_program_, "pointSize");
		if (pointSizeLoc != -1)
			glUniform1f(pointSizeLoc, pointSize);
	}


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, (srcDepthTextureId)?srcDepthTextureId:srcColorTextureId);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, srcColorTextureId);

	mesh->render(dstViewProjMat, dstWorldToViewMat, *warpMeshMaterial);

	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);

#if 0
	// now we inverse-warp the color data.
	// we only draw the color data, and store the final result as RGBD.

	glDisable(GL_DEPTH_TEST);

	//glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	//glDepthMask(GL_FALSE);
	//glDisable(GL_DEPTH_TEST);

	///
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_.id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destTextureId, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	GlUtil::checkFramebuffer();
	glViewport(0, 0, destWidth, destHeight);

	glClearColor(0, 1, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	///

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, warpedTextureId_);
	//glActiveTexture(GL_TEXTURE1);
	//glBindTexture(GL_TEXTURE_EXTERNAL_OES, colorTextureId);

	glUseProgram(warpColorMaterial_->shader_program_);
	{
		GLuint depthViewToWorldMatLoc = glGetUniformLocation(warpColorMaterial_->shader_program_, "depthViewToWorldMat");
		GLuint depthViewProjInvMatLoc = glGetUniformLocation(warpColorMaterial_->shader_program_, "depthViewProjInvMat");
		GLuint colorWorldToViewMatLoc = glGetUniformLocation(warpColorMaterial_->shader_program_, "colorWorldToViewMat");
		GLuint colorViewProjMatLoc = glGetUniformLocation(warpColorMaterial_->shader_program_, "colorViewProjMat");
		glUniformMatrix4fv(depthViewToWorldMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(worldToViewMat)));
		glUniformMatrix4fv(depthViewProjInvMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(viewProjMat)));
		glUniformMatrix4fv(colorWorldToViewMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(colorViewToWorldMat)));
		glUniformMatrix4fv(colorViewProjMatLoc, 1, GL_FALSE, glm::value_ptr(colorViewProjMat));
	}

	quad_->Render(glm::mat4(1.0), glm::mat4(1.0), *warpColorMaterial_);

	glUseProgram(0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
#endif
}

void GlPointcloudRenderer::storeDepthInAlpha(const GlTexturePtr& dstRGBDTexture, const GlTexturePtr& srcColorTexture, const GlTexturePtr& srcDepthTexture)
{
	// now put the depthTexture into the RGBD image...
	glViewport(0, 0, dstRGBDTexture->width, dstRGBDTexture->height);

	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstRGBDTexture->id, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	GlUtil::checkFramebuffer();

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcDepthTexture->id);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, srcColorTexture->id);

	quad_->render(glm::mat4(1.0), glm::mat4(1.0), setRgbdMaterial_);

	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
	glEnable(GL_DEPTH_TEST);
}

void GlPointcloudRenderer::fillHoles(const GlTexturePtr& destRgbdTexture, const GlTexturePtr& srcRgbdTexture)
{
	glViewport(0, 0, destRgbdTexture->width, destRgbdTexture->height);

	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destRgbdTexture->id, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	GlUtil::checkFramebuffer();

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcRgbdTexture->id);

	quad_->render(glm::mat4(1.0), glm::mat4(1.0), holeFillMaterial_);

	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
	glEnable(GL_DEPTH_TEST);
}