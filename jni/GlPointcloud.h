
#ifndef GLPOINTCLOUD_H_
#define GLPOINTCLOUD_H_

#include "tango-gl-renderer/gl_util.h"
#include "GlMaterial.h"

// depth_buffer_size: Number of vertices in of the data. Example: 60 floats
// in the buffer, the size should be 60/3 = 20;
//
// depth_data_buffer: Pointer to float array contains float triplet of each
// vertices in the point cloud.
class GlPointcloud
{
public:
	GlPointcloud();
	~GlPointcloud();

	// render the pointcloud from a pose.
	void render(glm::mat4 projection_mat, glm::mat4 view_mat, const GlMaterial& mat, float pointSize=1.0, float width=0.0, float height=0.0);
	// update positions from a sysmem buffer.
	void updatePositions(int numPoints, float* buffer, const glm::mat4& viewToWorldMat);
	// use gl transform feedback to populate the vertex color buffer with texture values.
	void updateColorsFromTexture(GLuint colorTextureId, const glm::mat4& colorViewProjMat, const glm::mat4& colorViewToWorldMat);

	GlMaterial defaultMaterial;		// default material to render colored pointcloud.
	GlMaterial setRgbdMaterial;

private:
	GlTransformFeedbackPtr tf_;		// transformfeedback object.
	GlMaterial tfMaterial_;			// transformfeedback material.
	GLuint vbos_[2];				// vertex buffers.
	int numPoints_;					// last updated point count.
	glm::mat4 viewToWorldMat_;		// last updated pose matrix.
};

#endif  // GLPOINTCLOUD_H_
