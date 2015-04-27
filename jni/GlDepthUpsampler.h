
#ifndef GLDEPTHUPSAMPLER_H
#define GLDEPTHUPSAMPLER_H

#include "GlBilateralGrid.h"

class GlDepthUpsampler
{
public:
	GlDepthUpsampler();

	bool setup(int width, int height, int numLevels);
	void upsampleRgbd();

	void renderPointcloudToTexture(GlPointcloud* pointcloud, 
		glm::mat4 viewProjectionMat, glm::mat4 worldToViewMat,
		const GlMaterial& mat);

	// build a RGB image pyramid.
	void updateColorPyramid(const GlTexturePtr& srcColorTexture, int numLevels=-1);
	// build an RGBD image pyramid.
	void updateRgbdPyramid(const GlTexturePtr& srcColorTexture, const GlTexturePtr& srcDepthTexture, int numLevels=-1);

public:
	int width_, height_;
	int numLevels_;
	std::vector<GlBilateralGrid*> bilateralGrids_;
	GlTexturePtr* depthTexturePyramid_;
	GlTexturePtr* colorTexturePyramid_;
	GlTexturePtr* depthUpsampleTexture_;
	GlTexturePtr pointcloudColorTexture_;
	GlTexturePtr pointcloudDepthTexture_;
	GlFramebufferPtr fbo_;
	GlQuad* quad_;
	GlMaterial setRgbdMaterial_;
	GlMaterial setColorMaterial_;
	GlMaterial reduceRgbdMaterial_;
	GlMaterial reduceColorMaterial_;
};

#endif  // GLDEPTHUPSAMPLER_H
