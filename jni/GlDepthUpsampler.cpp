
#include "GlDepthUpsampler.h"

GlDepthUpsampler::GlDepthUpsampler()
	:
	setColorMaterial_(vs_simpleTexture, fs_simpleTexture2d),
	reduceColorMaterial_(vs_simpleTexture, fs_reduceColor),
	setRgbdMaterial_(vs_simpleTexture, fs_setRgbd),
	reduceRgbdMaterial_(vs_simpleTexture, fs_reduceRgbd)
{
	numLevels_ = 0;
	width_ = 0;
	height_ = 0;
	quad_ = 0;
	colorTexturePyramid_ = 0;
	depthTexturePyramid_ = 0;
	depthUpsampleTexture_ = 0;
}

bool GlDepthUpsampler::setup(int width, int height, int numLevels)
{
	if (numLevels_ == numLevels && width_ == width && height_ == height)
		return true;

	delete[] colorTexturePyramid_;
	delete[] depthTexturePyramid_;
	delete[] depthUpsampleTexture_;

	numLevels_ = numLevels;
	width_ = width;
	height_ = height;

	pointcloudDepthTexture_ = GlTexturePtr::create(GL_TEXTURE_2D, width_, height_, GL_DEPTH_COMPONENT32F);
	pointcloudColorTexture_ = GlTexturePtr::create(GL_TEXTURE_2D, width_, height_, GL_RGBA32F);
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
	//glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTexturePyramid_[0]->id, 0);
	/*
		glGenRenderbuffers(1, &depthRenderBufferId_);
		glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBufferId_);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width_, height_);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBufferId_);
		GlUtil::checkFramebuffer();
		*/
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	quad_ = new GlQuad();

	bilateralGrids_.clear();
	bilateralGrids_.resize(numLevels_);

	for (int i = 0; i < numLevels_ - 1; ++i)
	{
		bilateralGrids_[i] = new GlBilateralGrid();
		bilateralGrids_[i]->setup(glm::vec4(width_ >> i, height_ >> i, 16, 1), glm::vec4(1, 1, 1, 1), glm::vec4(0, 0, 0, 0));
	}

	return true;
}

void GlDepthUpsampler::renderPointcloudToTexture(GlPointcloud* pointcloud,
	glm::mat4 viewProjectionMat, glm::mat4 worldToViewMat,
	const GlMaterial& mat)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_->id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, pointcloudDepthTexture_->id, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pointcloudColorTexture_->id, 0);
	GlUtil::checkFramebuffer();

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glClearDepthf(1);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, pointcloudColorTexture_->width, pointcloudColorTexture_->height);
	
	//glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_2D, colorTexturePyramid_[0]->id);

	pointcloud->render(viewProjectionMat, worldToViewMat, mat, 1.0, pointcloudColorTexture_->width, pointcloudColorTexture_->height);

	//glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
}


void GlDepthUpsampler::updateRgbdPyramid(const GlTexturePtr& srcColorTexture, const GlTexturePtr& srcDepthTexture, int numLevels)
{
	if (numLevels < 1 || numLevels > numLevels_)
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

	// reduce using the MIN operator (to always take the RGBD of the nearer depth value from the valid pixels)...
	for (int l = 1; l < numLevels; ++l)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTexturePyramid_[l]->id, 0);
		glViewport(0, 0, depthTexturePyramid_[0]->width >> l, depthTexturePyramid_[0]->height >> l);

		glBindTexture(GL_TEXTURE_2D, depthTexturePyramid_[l - 1]->id);
		quad_->render(glm::mat4(1.0), glm::mat4(1.0), reduceRgbdMaterial_);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void GlDepthUpsampler::updateColorPyramid(const GlTexturePtr& srcColorTexture, int numLevels)
{
	if (numLevels < 1 || numLevels > numLevels_)
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

	// reduce using a box filter...
	for (int l = 1; l < numLevels; ++l)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexturePyramid_[l]->id, 0);
		glViewport(0, 0, colorTexturePyramid_[0]->width >> l, colorTexturePyramid_[0]->height >> l);

		glBindTexture(GL_TEXTURE_2D, colorTexturePyramid_[l - 1]->id);
		quad_->render(glm::mat4(1.0), glm::mat4(1.0), reduceColorMaterial_);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void GlDepthUpsampler::upsampleRgbd()
{
#if 1
	// non-hierarchichal bilateral upsampling...
	bilateralGrids_[0]->clear();
	bilateralGrids_[0]->splatRgbd(depthTexturePyramid_[0]);
	bilateralGrids_[0]->slice(colorTexturePyramid_[0]->id, depthUpsampleTexture_[0]);
#else
	// hierarchichal bilateral upsampling (needs fix)...
	int i = numLevels_ - 1;
	bilateralGrids_[i - 1]->clear();
	bilateralGrids_[i - 1]->splatRgbd(depthTexturePyramid_[i]);
	bilateralGrids_[i - 1]->sliceMerge(colorTexturePyramid_[i - 1], depthTexturePyramid_[i], depthTexturePyramid_[i - 1], depthUpsampleTexture_[i - 1]);
	i--;
	for (; i >= 1; --i)
	{
		bilateralGrids_[i - 1]->clear();
		bilateralGrids_[i - 1]->splatRgbd(depthUpsampleTexture_[i]);
		bilateralGrids_[i - 1]->sliceMerge(colorTexturePyramid_[i - 1], depthUpsampleTexture_[i], depthTexturePyramid_[i - 1], depthUpsampleTexture_[i - 1]);
	}
#endif
}