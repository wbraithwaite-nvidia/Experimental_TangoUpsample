
#include "GlPointcloud.h"

static const char kVertexShader[] =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec4 vPosition;
in vec3 vColor;
uniform mat4 worldToViewProjMat;
out vec3 fColor;
uniform float pointSize;

void main() 
{
	gl_PointSize = pointSize;
	gl_Position = worldToViewProjMat * vec4(vPosition.xyz, 1.0);
	/*float depth = gl_Position.z / gl_Position.w;
	float nearClip = 0.5 * 1.0;
	float farClip = 6.0 * 1.0;
	float linearDepth = ((gl_Position.w) - nearClip) / (farClip - nearClip);*/
	fColor = vColor;
});

static const char kFragmentShader[] =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec3 fColor;
void main() {
	gl_FragColor = vec4(fColor.rgb, 1.0);//gl_FragCoord.z);
});


static const char* vs_colorFromTexture =
"#version 300 es \n"
"precision highp float;\n"
"precision highp int;\n"
STRINGIFY(
in vec3 vPosition;
out vec3 fColor;
uniform mat4 worldToViewProjMat;

uniform sampler2D texture0;
uniform mat4 depthViewToWorldMat;
uniform mat4 colorWorldToViewMat;
uniform mat4 colorViewProjMat;

vec3 pseudoTemperature(float t)
{
	float b = t < 0.25f ? smoothstep( -0.25f, 0.25f, t ) : 1.0f-smoothstep( 0.25f, 0.5f, t );
	float g = t < 0.5f  ? smoothstep( 0.0f, 0.5f, t ) : (t < 0.75f ? 1.0f : 1.0f-smoothstep( 0.75f, 1.0f, t ));
	float r = smoothstep( 0.5f, 0.75f, t );
	return vec3( r, g, b );
}

void main() 
{
	vec4 dstNdcPos = colorViewProjMat * colorWorldToViewMat * depthViewToWorldMat * vec4(vPosition.xyz, 1.0);
	dstNdcPos.xyz = dstNdcPos.xyz / dstNdcPos.w;
	vec2 dstUvPos = dstNdcPos.xy * vec2(0.5) + vec2(0.5);
	vec3 dstTexel = texture2D(texture0, dstUvPos.xy).rgb;
	//vec3 dstTexel = textureLod(texture0, dstUvPos.xy, 4.0).rgb;

	fColor = dstTexel;
	/*
	// DEBUG: use depth temperature...
	float nearClip = 0.5 * 1.0;
	float farClip = 6.0 * 1.0;
	float linearDepth = ((dstNdcPos.w) - nearClip) / (farClip - nearClip);
	fColor = pseudoTemperature(linearDepth).rgb;*/
});

static const glm::mat4 inverse_z_mat = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, -1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f);

GlPointcloud::GlPointcloud()
	:
	defaultMaterial(kVertexShader, kFragmentShader),
	tfMaterial_(vs_colorFromTexture, kFragmentShader)
{

	numPoints_ = 0;
	glGenBuffers(2, vbos_);

	tf_ = GlTransformFeedback::create();
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf_->id);
    const char* varyingNames[] = {"fColor"};
    glTransformFeedbackVaryings(tfMaterial_.shader_program_, 1, varyingNames, GL_SEPARATE_ATTRIBS);
	GlUtil::LinkProgram(tfMaterial_.shader_program_);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
}

GlPointcloud::~GlPointcloud()
{
	glDeleteBuffers(2, vbos_);
}

void GlPointcloud::updatePositions(int numPoints, float* buffer, const glm::mat4& viewToWorldMat)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbos_[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * numPoints * 3, buffer, GL_DYNAMIC_DRAW);

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf_->id);
	glBindBuffer(GL_ARRAY_BUFFER, vbos_[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * numPoints * 3, 0, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

	numPoints_ = numPoints;
	viewToWorldMat_ = viewToWorldMat;
}

void GlPointcloud::updateColorsFromTexture(GLuint colorTextureId, const glm::mat4& colorViewProjMat, const glm::mat4& colorViewToWorldMat)
{
	if (numPoints_ == 0)
		return;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

	glUseProgram(tfMaterial_.shader_program_);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorTextureId);

	GLuint depthViewToWorldMatLoc = glGetUniformLocation(tfMaterial_.shader_program_, "depthViewToWorldMat");
	GLuint colorWorldToViewMatLoc = glGetUniformLocation(tfMaterial_.shader_program_, "colorWorldToViewMat");
	GLuint colorViewProjMatLoc = glGetUniformLocation(tfMaterial_.shader_program_, "colorViewProjMat");
	glUniformMatrix4fv(depthViewToWorldMatLoc, 1, GL_FALSE, glm::value_ptr(viewToWorldMat_ * inverse_z_mat));
	glUniformMatrix4fv(colorWorldToViewMatLoc, 1, GL_FALSE, glm::value_ptr(glm::inverse(colorViewToWorldMat)));
	glUniformMatrix4fv(colorViewProjMatLoc, 1, GL_FALSE, glm::value_ptr(colorViewProjMat));

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf_->id);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbos_[1]);

	glEnable(GL_RASTERIZER_DISCARD);
	glBeginTransformFeedback(GL_POINTS);

	glBindBuffer(GL_ARRAY_BUFFER, vbos_[0]);
	glEnableVertexAttribArray(tfMaterial_.attrib_vertices_);
	glVertexAttribPointer(tfMaterial_.attrib_vertices_, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_POINTS, 0, numPoints_);
	glDisableVertexAttribArray(tfMaterial_.attrib_vertices_);

	glEndTransformFeedback();
	glDisable(GL_RASTERIZER_DISCARD);

	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
	glUseProgram(0);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void GlPointcloud::render(glm::mat4 viewProjectionMat, glm::mat4 worldToViewMat, const GlMaterial& mat, float pointSize)
{
	if (numPoints_ == 0)
		return;

	//glm::mat4 modelToWorldMat = GetTransformationMatrix();
	mat.apply(viewProjectionMat, worldToViewMat, viewToWorldMat_ * inverse_z_mat);

	GLuint pointSizeLoc = glGetUniformLocation(mat.shader_program_, "pointSize");
	if (pointSizeLoc != -1)
		glUniform1f(pointSizeLoc, pointSize);

	if (mat.attrib_vertices_ != -1)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbos_[0]);
		glEnableVertexAttribArray(mat.attrib_vertices_);
		glVertexAttribPointer(mat.attrib_vertices_, 3, GL_FLOAT, GL_FALSE, 0, 0);
	}
	if (mat.attrib_colors_ != -1)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbos_[1]);
		glEnableVertexAttribArray(mat.attrib_colors_);
		glVertexAttribPointer(mat.attrib_colors_, 3, GL_FLOAT, GL_FALSE, 0, 0);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

	glDrawArrays(GL_POINTS, 0, numPoints_);

	if (mat.attrib_vertices_ != -1)
		glDisableVertexAttribArray(mat.attrib_vertices_);
	if (mat.attrib_colors_ != -1)
		glDisableVertexAttribArray(mat.attrib_colors_);
	glGetError();
	glUseProgram(0);
}
