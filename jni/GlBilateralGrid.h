
#ifndef GLBILATERALGRID_H
#define GLBILATERALGRID_H

#include "tango-gl-renderer/GlUtil.h"
#include "GlQuad.h"
#include "GlPlaneMesh.h"
#include "GlPointcloud.h"

class GlBilateralGrid
{
public:
	GlBilateralGrid();
	~GlBilateralGrid();

	void setup(const glm::vec4& inputSize, const glm::vec4& sigma, const glm::vec4& padding = glm::vec4(0));

	void clear();
	void splatRgbd(const GlTexture& srcRgbdTexture, float inputTime=0.0, float weight=1.0);
	void splatRgbAndDepth(GLuint srcColorTextureId, GLuint srcDepthTextureId, float inputTime=0.0, float weight=1.0);
	void slice(GLuint srcTextureId, const GlTexture& dstTexture);
	void sliceMerge(const GlTexture& refRgbTexture, const GlTexture& prevUpsampleTexture, const GlTexture& rgbdTexture, const GlTexture& resultRgbdTexture);

private:

	void blurAndNormalize_();

public:

	float gridSigma[4];
	int gridInputSize[4];
	int gridPadding[4];
	int gridSize[4];
	int gridRasterWidth, gridRasterHeight;

	GlTexture gridTextures_[2];
	GlPlaneMesh* gridMesh_;
	GlQuad* quad_;
	GlFramebuffer fbo_;

	GlMaterial bilateralSplatRgbd_;
	GlMaterial bilateralSplat_;
	GlMaterial gaussianBlur_;
	GlMaterial bilateralSlice_;
	GlMaterial bilateralSliceMerge_;
	GlMaterial normalizeMaterial_;
	GlMaterial bilateralSplatPointcloud_;
};

#endif  // GLBILATERALGRID_H
