
#ifndef GLDEPTHUPSAMPLER_H
#define GLDEPTHUPSAMPLER_H

#include "GlPointcloudRenderer.h"
#include "GlBilateralGrid.h"

class GlDepthUpsampler
{
public:
	GlDepthUpsampler();

	bool setup(int width, int height, int numLevels);
	void upsampleRgbd();

	// build a RGB image pyramid.
	void updateColorPyramid(const GlTexture& srcColorTexture, int numLevels=-1);
	// build an RGBD image pyramid.
	void updateRgbdPyramid(const GlTexture& srcColorTexture, const GlTexture& srcDepthTexture, int numLevels=-1);

public:
	int width_, height_;
	int numLevels_;
	std::vector<GlBilateralGrid*> bilateralGrids_;
	GlTexture* depthTexturePyramid_;
	GlTexture* colorTexturePyramid_;
	GlTexture* depthUpsampleTexture_;
	GlFramebuffer fbo_;
	GlQuad* quad_;
	GlMaterial setRgbdMaterial_;
	GlMaterial setColorMaterial_;
	GlMaterial reduceRgbdMaterial_;
	GlMaterial reduceColorMaterial_;
};

#endif  // GLDEPTHUPSAMPLER_H
