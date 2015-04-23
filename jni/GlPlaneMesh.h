
#ifndef GL_PLANEMESH_H_
#define GL_PLANEMESH_H_

#include "tango-gl-renderer/drawable_object.h"
#include "tango-gl-renderer/GlUtil.h"
#include "tango-gl-renderer/GlMaterial.h"

class GlPlaneMesh : public DrawableObject
{
public:
	GlPlaneMesh(int width, int height);
	virtual ~GlPlaneMesh();

	void render(const glm::mat4& projection_mat, const glm::mat4& view_mat, const GlMaterial& mat) const;

private:
	GLuint vertex_buffers[1];

	int width_, height_;
};
#endif  // GL_PLANEMESH_H_
