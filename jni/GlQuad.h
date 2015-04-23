
#ifndef GL_QUAD_H_
#define GL_QUAD_H_

#include "tango-gl-renderer/drawable_object.h"
#include "tango-gl-renderer/GlUtil.h"
#include "tango-gl-renderer/GlMaterial.h"

class GlQuad : public DrawableObject
{
public:
	GlQuad();
	virtual ~GlQuad();

	void render(const glm::mat4& projection_mat, const glm::mat4& view_mat, const GlMaterial& mat) const;

private:
	GLuint vbos_[3];
};
#endif  // GL_QUAD_H_
