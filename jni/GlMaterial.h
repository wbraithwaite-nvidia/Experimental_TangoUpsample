
#ifndef GLMATERIAL_H_
#define GLMATERIAL_H_

#include "tango-gl-renderer/gl_util.h"
#include "MaterialShaders.h"

class GlMaterial
{
public:
	GlMaterial();
	GlMaterial(const char* vertexShader, const char* fragmentShader=0);
	~GlMaterial();

	void apply(const glm::mat4& projMat, const glm::mat4& viewMat, const glm::mat4& modelMat) const;

	GLuint shader_program_;
	GLuint uniform_mvp_mat_;
	GLuint uniform_texture0;
	GLuint uniform_textureSize0;
	GLuint uniform_texture1;
	GLuint uniform_texture2;
	GLuint uniform_texture3;
	GLuint attrib_vertices_;
	GLuint attrib_colors_;
	GLuint attrib_textureCoords;
	GLuint uniform_color;

	glm::vec4 color;
};

#endif  // GLMATERIAL_H_
