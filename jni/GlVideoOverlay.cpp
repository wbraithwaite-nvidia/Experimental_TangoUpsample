#include "GlVideoOverlay.h"

GlVideoOverlay::GlVideoOverlay() 
{
	material = new GlMaterial(vs_simpleTexture, fs_simpleTextureExternal);

	glEnable(GL_TEXTURE_EXTERNAL_OES);
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glUniform1i(material->uniform_texture0, textureId);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}

GlVideoOverlay::~GlVideoOverlay() 
{
	glDeleteTextures(1, &textureId);
	delete material;
}

void GlVideoOverlay::render(
	const glm::mat4& projection_mat,
    const glm::mat4& view_mat
	) const 
{
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
	GlQuad::render(projection_mat, view_mat, *material);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}
