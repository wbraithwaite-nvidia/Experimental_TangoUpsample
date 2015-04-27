/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TANGO_GL_RENDERER_GL_UTIL_H
#define TANGO_GL_RENDERER_GL_UTIL_H
#define GLM_FORCE_RADIANS

#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642

#include "TangoUpsampleUtil.h"
#include <stdlib.h>
#include <jni.h>
#include <android/log.h>
//#include <GLES2/gl2.h>
//#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <memory>

#ifndef GL_OES_EGL_image_external
#define GL_OES_EGL_image_external
#define GL_TEXTURE_EXTERNAL_OES             0x8d65     /* 36197 */
#define GL_SAMPLER_EXTERNAL_OES             0x8d66     /* 36198 */
#define GL_TEXTURE_BINDING_EXTERNAL_OES     0x8d67     /* 36199 */
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES 0x8d68     /* 36200 */
#endif

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/matrix_decompose.hpp"

#ifndef LOGI
#	define LOG_TAG "tango_jni_example"
#	define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#	define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#ifndef PI
#	define PI 3.1415926f
#endif
#define RADIAN_2_DEGREE 57.2957795f
#define DEGREE_2_RADIANS 0.0174532925f

#define GL_CHECKERRORS(msg) GlUtil::CheckGlError(msg, __FILE__,__LINE__)

class GlTexture
{
public:

	GLuint id;
	GLenum target;
	int width, height;
	GLenum internalType;

	~GlTexture();

	
private:
	GlTexture();
	GlTexture(const GlTexture&);

	friend class GlTexturePtr;
};

class GlTexturePtr : public SharedPtr<GlTexture>
{
	typedef SharedPtr<GlTexture> Ptr;
public:
	GlTexturePtr() {}
	explicit GlTexturePtr(Ptr d) : Ptr(d) {}
	static GlTexturePtr create(GLenum target = GL_TEXTURE_2D, int width = 256, int height = 256, GLenum internalType = GL_RGBA8, int numMipmaps=1);
};

class GlFramebuffer
{
public:
	GLuint id;

	~GlFramebuffer();

private:
	GlFramebuffer();
	GlFramebuffer(const GlFramebuffer&);

	friend class GlFramebufferPtr;
};

class GlFramebufferPtr : public SharedPtr<GlFramebuffer>
{
	typedef SharedPtr<GlFramebuffer> Ptr;
public:
	GlFramebufferPtr() {}
	explicit GlFramebufferPtr(Ptr d) : Ptr(d) {}
	static GlFramebufferPtr create();
};

class GlTransformFeedback
{
public:
	GLuint id;

	~GlTransformFeedback();

private:
	GlTransformFeedback();
	GlTransformFeedback(const GlTransformFeedback&);

	friend class GlTransformFeedbackPtr;
};

class GlTransformFeedbackPtr : public SharedPtr<GlTransformFeedback>
{
	typedef SharedPtr<GlTransformFeedback> Ptr;
public:
	GlTransformFeedbackPtr() {}
	explicit GlTransformFeedbackPtr(Ptr d) : Ptr(d) {}
	static GlTransformFeedbackPtr create();
};


class GlUtil {
public:
	static bool checkFramebuffer();
	static void CheckGlError(const char* operation,const char* file=0,int line=-1);
	static GLuint CreateProgram(const char* vertex_source, const char* fragment_source);
	static bool LinkProgram(GLuint& program);
	static GLuint createTexture(GLenum target = GL_TEXTURE_2D, int width = 256, int height = 256, GLenum internalType = GL_RGBA8, int numMipmaps=1);
	static glm::quat ConvertRotationToOpenGL(const glm::quat& rotation);
	static glm::vec3 ConvertPositionToOpenGL(const glm::vec3& position);
	static void DecomposeMatrix(const glm::mat4& transform_mat,
		glm::vec3& translation,
		glm::quat& rotation,
		glm::vec3& scale);
	static glm::vec3 GetTranslationFromMatrix(const glm::mat4& transform_mat);
	static float Clamp(float value, float min, float max);
	static void PrintMatrix(const glm::mat4& matrix);
	static void PrintVector(const glm::vec3& vector);
	static glm::vec3 LerpVector(const glm::vec3& x, const glm::vec3& y, float a);
	static const glm::mat4 ss_to_ow_mat;
	static const glm::mat4 oc_to_c_mat;
	static const glm::mat4 oc_to_d_mat;
	static float DistanceSquared(const glm::vec3& v1, const glm::vec3& v2);

private:
	static GLuint LoadShader(GLenum shader_type, const char* shader_source);
};

inline unsigned int iDivUp(unsigned int a, unsigned int b)
{
	if (b == 0)
		return a;
	return (a % b != 0) ? (a / b + 1) : (a / b);
}

#endif  // TANGO_GL_RENDERER_GL_UTIL
