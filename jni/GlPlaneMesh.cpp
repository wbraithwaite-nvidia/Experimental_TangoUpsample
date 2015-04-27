
#include "GlPlaneMesh.h"
#include <vector>

GlPlaneMesh::GlPlaneMesh(int width, int height)
{
	width_ = width;
	height_ = height;
	vertex_buffers[0] = 0;

	glGenBuffers(1, vertex_buffers);

	std::vector<float> data;
	data.reserve(width_*height_ * 4);
	for (int y = 0, i = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x, ++i)
		{
			data[i * 4 + 0] = (float(x)+0.5) / width;
			data[i * 4 + 1] = (float(y)+0.5) / height;
			data[i * 4 + 2] = 0.5;
		}
	}

	// Allocate vertices buffer.
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * width*height, &data[0], GL_STATIC_DRAW);

	/*
	// Allocate triangle indices buffer.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_buffers[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * 6, kIndices, GL_STATIC_DRAW);

	// Allocate texture coordinates buufer.
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, kTextureCoords, GL_STATIC_DRAW);
	
	*/
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GlPlaneMesh::~GlPlaneMesh()
{
	if (vertex_buffers[0])
		glDeleteBuffers(1, vertex_buffers);
}

void GlPlaneMesh::render(
	const glm::mat4& projection_mat,
	const glm::mat4& view_mat,
	const GlMaterial& mat
	) const
{

	glm::mat4 model_mat = GetTransformationMatrix();
	mat.apply(projection_mat, view_mat, model_mat);

	// Bind vertices buffer.
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[0]);
	glEnableVertexAttribArray(mat.attrib_vertices_);
	glVertexAttribPointer(mat.attrib_vertices_, 4, GL_FLOAT, GL_FALSE, 0, 0);

	/*
	// Bind texture coordinates buffer.
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffers[2]);
	glEnableVertexAttribArray(mat.attrib_textureCoords);
	glVertexAttribPointer(mat.attrib_textureCoords, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// Bind element array buffer.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_buffers[1]);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	*/

	glDrawArrays(GL_POINTS, 0, width_*height_);
	GL_CHECKERRORS("glDrawArrays");

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);
	GL_CHECKERRORS("glUseProgram()");
}
