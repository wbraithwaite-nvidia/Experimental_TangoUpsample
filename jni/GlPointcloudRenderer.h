
#ifndef POINTCLOUDRENDEREDR_H_
#define POINTCLOUDRENDEREDR_H_

#include "tango-gl-renderer/GlUtil.h"
#include "GlQuad.h"
#include "GlPlaneMesh.h"
#include "GlPointCloud.h"

class GlPointcloudRenderer
{
public:
	GlPointcloudRenderer(int maxWidth, int maxHeight, int maxLevels);
	~GlPointcloudRenderer();

	void renderToTextureClear(GLuint destTextureId, GLuint destTextureId2, int destWidth, int destHeight);
	void renderToTexture(GlPointcloud* pointcloud, float pointSize, GLuint destTextureId, GLuint destTextureId2, int destWidth, int destHeight,
		glm::mat4 projection_mat, glm::mat4 view_mat, const GlMaterial& mat);
	
	void fillHoles(GLuint destTextureId, int destWidth, int destHeight, GLuint srcDepthTextureId);	

	void storeDepthInAlpha(const GlTexturePtr& dstRGBDTexture, const GlTexturePtr& srcColorTexture, const GlTexturePtr& srcDepthTexture);

	// build a pyramid of minimum depths.
	// the result is as though we rendered the points at progressively lower resolutions.
	void buildRgbdPyramid(const GlTexturePtr& srcColorTexture, const GlTexturePtr& srcDepthTexture, int numLevels);
	void buildColorPyramid(const GlTexturePtr& srcColorTexture, int numLevels);

	// create an RGB texture (from one view) using a DEPTH texture from another view.
	void projectColor(GLuint destTextureId, int destWidth, int destHeight,
		const glm::mat4& depthViewToWorldMat, const glm::mat4& depthViewProjMat, GLuint depthTextureId, 
		const glm::mat4& colorViewToWorldMat, const glm::mat4& colorViewProjMat, GLuint colorTextureId
		);

	void mergeColor(GLuint destTextureId, int destWidth, int destHeight,
		GLuint currentColorTextureId,
		const glm::mat4& depthViewToWorldMat, const glm::mat4& depthViewProjMat, GLuint depthTextureId, 
		const glm::mat4& colorViewToWorldMat, const glm::mat4& colorViewProjMat, GLuint colorTextureId
		);

	// warp an image with depth texture to a new view.
	void warpImage(GlPlaneMesh* mesh, const glm::mat4& worldToViewMat, const glm::mat4& viewProjMat, GLuint destColorTextureId, GLuint destDepthTextureId, int destWidth, int destHeight, int pointSize, 
		const glm::mat4& depthViewToWorldMat, const glm::mat4& depthViewProjMat, GLuint srcColorTextureId, GLuint srcDepthTextureId,
		float nearClip, float farClip);

	void fillHoles(const GlTexturePtr& destRgbdTexture, const GlTexturePtr& srcRgbdTexture);

public:

	GlQuad* quad_;
	GlTexturePtr cleanDepthTexture_;
	GlTexturePtr rgbdTexture_;
	GlTexturePtr* depthTexturePyramid_;
	GlTexturePtr* colorTexturePyramid_;
	GlTexturePtr* depthUpsampleTexture_;
	GlFramebufferPtr fbo_;
	
	GlMaterial setRgbdMaterial_;
	GlMaterial setColorMaterial_;
	GlMaterial compositeMaterial_;
	GlMaterial reduceMaterial_;
	GlMaterial reduceRgbdMaterial_;
	GlMaterial reduceColorMaterial_;
	GlMaterial passThruMaterial_;
	GlMaterial mergeColorMaterial_;
	GlMaterial holeFillMaterial_;

	GLuint depthRenderBufferId_;

	int width_, height_;
	int numLevels_;

	GlMaterial* warpMaterial;
	GlMaterial* warpMeshMaterial;
	GlMaterial* warpColorMaterial_;
	GlMaterial* mergeRgbdMaterial_;
	
	GlPlaneMesh* mesh_;
};

#endif  // POINTCLOUDRENDEREDR_H_
