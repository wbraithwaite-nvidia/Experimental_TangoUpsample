
#include "GlQuad.h"

static const GLfloat kVertices[] =
{ -1.0, -1.0, 0.0,
  -1.0, 1.0, 0.0,
  1.0, -1.0, 0.0,
  1.0, 1.0, 0.0 };
/*
static const GLfloat kVertices[] =
{ -1.0, 1.0, 0.0,
  -1.0, -1.0, 0.0,
  1.0, 1.0, 0.0,
  1.0, -1.0, 0.0 };
  */
static const GLushort kIndices[] =
{ 0, 1, 2, 2, 1, 3 };

static const GLfloat kTextureCoords[] =
{ 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0 };


GlQuad::GlQuad() 
{
	glGenBuffers(3, vbos_);

	// Allocate vertices buffer.
	glBindBuffer(GL_ARRAY_BUFFER, vbos_[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 4, kVertices, GL_STATIC_DRAW);

	// Allocate triangle indices buffer.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos_[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * 6, kIndices, GL_STATIC_DRAW);

	// Allocate texture coordinates buufer.
	glBindBuffer(GL_ARRAY_BUFFER, vbos_[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, kTextureCoords, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GlQuad::~GlQuad()
{
	glDeleteBuffers(3, vbos_);
}

void GlQuad::render(
	const glm::mat4& projection_mat,
    const glm::mat4& view_mat,
	const GlMaterial& mat
	) const 
{

	glm::mat4 model_mat = GetTransformationMatrix();
	mat.apply(projection_mat, view_mat, model_mat);

	// Bind vertices buffer.
	glBindBuffer(GL_ARRAY_BUFFER, vbos_[0]);
	glEnableVertexAttribArray(mat.attrib_vertices_);
	glVertexAttribPointer(mat.attrib_vertices_, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// Bind texture coordinates buffer.
	glBindBuffer(GL_ARRAY_BUFFER, vbos_[2]);
	glEnableVertexAttribArray(mat.attrib_textureCoords);
	glVertexAttribPointer(mat.attrib_textureCoords, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// Bind element array buffer.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos_[1]);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	GlUtil::CheckGlError("glDrawElements");

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
