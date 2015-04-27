
#include "GlMaterial.h"

GlMaterial::GlMaterial()
	:
	shader_program_(0)
{
}

GlMaterial::GlMaterial(const char* vertexShader, const char* fragmentShader)
{
	shader_program_ = GlUtil::CreateProgram(vertexShader, fragmentShader);
	if (!shader_program_) 
	{
		LOGE("Could not create program.");
	}
	uniform_texture0 = glGetUniformLocation(shader_program_, "texture0");
	uniform_textureSize0 = glGetUniformLocation(shader_program_, "textureSize0");
	uniform_texture1 = glGetUniformLocation(shader_program_, "texture1");
	uniform_texture2 = glGetUniformLocation(shader_program_, "texture2");
	uniform_texture3 = glGetUniformLocation(shader_program_, "texture3");
	attrib_vertices_ = glGetAttribLocation(shader_program_, "vPosition");
	attrib_colors_ = glGetAttribLocation(shader_program_, "vColor");
	attrib_textureCoords = glGetAttribLocation(shader_program_, "vTexCoords");
	uniform_mvp_mat_ = glGetUniformLocation(shader_program_, "worldToViewProjMat");
	uniform_color = glGetUniformLocation(shader_program_, "color");

	glUseProgram(shader_program_);
	// default to tex unit index...
	glUniform1i(uniform_texture0, 0);
	glUniform1i(uniform_texture1, 1);
	glUniform1i(uniform_texture2, 2);
	glUniform1i(uniform_texture3, 3);
	glUseProgram(0);

	color = glm::vec4(1,1,1,1);

	GlUtil::CheckGlError("GlMaterial()");
}

GlMaterial::~GlMaterial()
{
	glDeleteProgram(shader_program_);
	shader_program_ = 0;	
}

void GlMaterial::apply(const glm::mat4& projMat, const glm::mat4& viewMat, const glm::mat4& modelMat) const
{
	if (!shader_program_)
		return;

	glUseProgram(shader_program_);

	glm::mat4 mvp_mat = projMat * viewMat * modelMat;
	glUniformMatrix4fv(uniform_mvp_mat_, 1, GL_FALSE, glm::value_ptr(mvp_mat));
	glUniform4f(uniform_color, color[0], color[1], color[2], color[3]);

	GlUtil::CheckGlError("GlMaterial::apply()");
}
