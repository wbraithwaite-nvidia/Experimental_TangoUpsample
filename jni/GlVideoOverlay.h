#ifndef AUGMENTED_REALITY_JNI_EXAMPLE_VIDEO_OVERLAY_H_
#define AUGMENTED_REALITY_JNI_EXAMPLE_VIDEO_OVERLAY_H_

#include "GlQuad.h"

class GlVideoOverlay : public GlQuad
{
public:
	GlVideoOverlay();
	virtual ~GlVideoOverlay();

	void render(const glm::mat4& projection_mat, const glm::mat4& view_mat) const;

	GLuint textureId;

private:

	GlMaterial* material;
};

/*
class GlVideoOverlay : public DrawableObject
{
public:
GlVideoOverlay();

void Render(const glm::mat4& projection_mat, const glm::mat4& view_mat) const;

GLuint textureId;

private:
GLuint vertex_buffers_;

GLuint attrib_vertices_;
GLuint attrib_textureCoords;
GLuint uniform_texture;
GLuint vertex_buffers[3];
GLuint shader_program_;

GLuint uniform_mvp_mat_;
};
*/
#endif  // AUGMENTED_REALITY_JNI_EXAMPLE_VIDEO_OVERLAY_H_
