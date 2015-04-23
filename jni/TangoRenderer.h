#include "GlVideoOverlay.h"
#include "GlPointcloud.h"



class ViewData
{
public:
	ViewData()
	{
		axis = new Axis();
		frustum = new Frustum();
		transform = new Transform();
		lastUpdateId = 0;
		drawOffset = glm::vec3(0, 0, 0);

		frustum->SetParent(0);
		transform->SetParent(0);
		transform->SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
		transform->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
		transform->SetRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

		apertureWidth = 0;
		apertureHeight = 0;
		fieldOfView = 0;
		nearDistance = 0;
		farDistance = 0;
	}

	virtual ~ViewData()
	{
		delete transform;
		transform = 0;
		delete frustum;
		frustum = 0;
		delete axis;
		axis = 0;
	}

	virtual void setRenderMode(int mode)
	{
		if (mode == 0)
		{
			frustum->SetParent(0);

			transform->SetParent(0);
			transform->SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
			transform->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
			transform->SetRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
		}
		else if (mode == 1)
		{
			frustum->SetParent(axis);

			// scale the frustum to the far plane...
			float scaleX = farDistance * (0.5 * apertureWidth / focalLength);
			frustum->SetScale(glm::vec3(scaleX, scaleX/apertureRatio, farDistance));

			// make the distance where the frustum is 2 units wide...
			float frustumNearWidth = 2.0;
			float imagePlaneDist = frustumNearWidth * (focalLength / apertureWidth);

			transform->SetParent(axis);
			transform->SetScale(glm::vec3(1.0f, 1.f/apertureRatio, 1.0f));
			transform->SetRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
			transform->SetPosition(glm::vec3(0.0, 0.0, -imagePlaneDist));
		}
	}

	void setDrawOffset(const glm::vec3& offset)
	{
		//drawOffset = offset;
	}

	void setName(const std::string& n)
	{
		name = n;
	}

	virtual void setup(int w, int h, float hfov, float n, float f)
	{
		if (apertureWidth == w && apertureHeight == h && fieldOfView == hfov && n == nearDistance && f == farDistance)
			return;

		//CT5(w,h,hfov,n, f);
		//CT5(apertureWidth, apertureHeight, fieldOfView, nearDistance, farDistance);
		

		fieldOfView = hfov;
		apertureWidth = w;
		apertureHeight = h;
		nearDistance = n;
		farDistance = f;

		CT5(apertureWidth, apertureHeight, fieldOfView, nearDistance, farDistance);

		focalLength = (0.5 * apertureWidth) / tan(hfov * 0.5);
		apertureRatio = apertureWidth / apertureHeight;

		float xmax = nearDistance * (0.5 * apertureWidth / focalLength);
		float xmin = -xmax;
		float ymax = xmax / apertureRatio;	
		float ymin = -ymax;

		viewProjectionMat = glm::frustum(
			xmin, xmax,
			ymin, ymax,
			nearDistance, farDistance);
	}


	virtual GLuint getTextureId()
	{
		return 0;
	}

	virtual void update()
	{
	}

	virtual void render()
	{
	}

	virtual void drawCamera(const glm::mat4& viewMat, const glm::mat4& viewProjMat)
	{
		axis->SetPosition(lastCameraPosition + drawOffset);
		axis->SetRotation(lastCameraRotation);
		axis->render(viewProjMat, viewMat);

		frustum->SetParent(axis);
		frustum->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
		frustum->SetRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
		frustum->render(viewProjMat, viewMat);
	}

	glm::vec3 drawOffset;
	Axis *axis;
	Frustum *frustum;
	Transform* transform;

	int lastUpdateId;
	std::string name;
	float apertureWidth, apertureHeight, apertureRatio;
	float focalLength;
	float fieldOfView;
	float nearDistance, farDistance;
	glm::vec3 lastCameraPosition;
	glm::quat lastCameraRotation;
	glm::mat4 viewToWorldMat;
	glm::mat4 viewProjectionMat;
};

class TextureViewData : public ViewData
{
public:

	

	TextureViewData()
		:
		textureWithDepthMaterial_(vs_simpleTexture, fs_rgbdTextureWithDepth),
		showDepthMaterial_(vs_simpleTexture, fs_showDepth),
		lodMaterial_(vs_simpleTexture, fs_lodTexture2d)
	{
		material = 0;
		geometry = 0;

		geometry = new GlQuad();
		geometry->SetParent(transform);

		glUseProgram(textureWithDepthMaterial_.shader_program_);
		GLuint displayModeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "displayMode");
		glUniform1i(displayModeLoc_, 0);
		glUseProgram(0);

		glUseProgram(showDepthMaterial_.shader_program_);
		GLuint srcClipRangeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcClipRange");
		GLuint srcImageTypeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcImageType");
		GLuint srcDepthTypeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcDepthType");
		glUniform1i(srcImageTypeLoc_, 0); // depth image
		glUniform1i(srcDepthTypeLoc_, 0); // depth is in fragment-space
		glUniform2f(srcClipRangeLoc_, 0, 1); // this is fixed.
		glUseProgram(0);
	}

	void setupTexture(int w, int h, GLenum internalType = GL_RGBA8, int numMipmaps = 1)
	{
		// validate the arguments.
		if (w == 0)
			w = 1;
		if (h == 0)
			h = 1;

		// if texture is different format, then rebuild the textures and material.

		if (texture && texture->width == w && texture->height == h)
			return;

		texture = GlTexture::create(GL_TEXTURE_2D, w, h, internalType, numMipmaps);


		delete material;
		material = 0;

		if (internalType == GL_R8 || internalType == GL_R32F)
			material = new GlMaterial(vs_simpleTexture, fs_simpleTexture2d_r);
		else
			material = new GlMaterial(vs_simpleTexture, fs_simpleTexture2d);
	}

	virtual ~TextureViewData()
	{
		delete geometry;
		geometry = 0;
		delete material;
		material = 0;
	}

	virtual GLuint getTextureId()
	{
		if (texture)
			return texture->id;
		return 0;
	}

	void renderTexture(GLuint texId, GlMaterial* mat)
	{
		if (texId == 0)
			return;

		glUseProgram(showDepthMaterial_.shader_program_);
		GLuint srcClipRangeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcClipRange");
		glUniform2f(srcClipRangeLoc_, nearDistance, farDistance); // this is fixed.
		glUseProgram(0);

		glDisable(GL_DEPTH_TEST);
		glBindTexture(GL_TEXTURE_2D, texId);
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), *mat);
		glBindTexture(GL_TEXTURE_2D, 0);
		glEnable(GL_DEPTH_TEST);
		glUseProgram(0);
	}

	void renderTextureLod(const GlTexture& texture, int lod)
	{
		glBindTexture(GL_TEXTURE_2D, texture->id);
		glUseProgram(lodMaterial_.shader_program_);
		GLuint loc = glGetUniformLocation(lodMaterial_.shader_program_, "lod");
		glUniform1f(loc, lod);
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), lodMaterial_);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	void renderTexture(GLuint texId)
	{
		if (texId == 0)
			return;

		glDisable(GL_DEPTH_TEST);
		glBindTexture(GL_TEXTURE_2D, texId);
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), *material);
		glBindTexture(GL_TEXTURE_2D, 0);
		glEnable(GL_DEPTH_TEST);
		glUseProgram(0);
	}

	virtual void render()
	{
		if (getTextureId() == 0)
			return;

		glDisable(GL_DEPTH_TEST);
		glBindTexture(GL_TEXTURE_2D, getTextureId());
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), *material);
		glBindTexture(GL_TEXTURE_2D, 0);
		glEnable(GL_DEPTH_TEST);
		glUseProgram(0);
	}

	virtual void drawCamera(const glm::mat4& viewMat, const glm::mat4& viewProjMat)
	{
		ViewData::drawCamera(viewMat, viewProjMat);

		if (getTextureId() == 0)
			return;

		glBindTexture(GL_TEXTURE_2D, getTextureId());
		geometry->render(viewProjMat, viewMat, *material);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	virtual void renderWithDepth(int depthType, float linearNear, float linearFar, int debugMode)
	{
		if (texture == 0)
			return;
		
		glUseProgram(textureWithDepthMaterial_.shader_program_);
		GLuint srcClipRangeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "srcClipRange");
		GLuint dstClipRangeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "dstClipRange");
		GLuint srcDepthTypeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "srcDepthType");
		glUniform1i(srcDepthTypeLoc_, depthType); // depth is in normalized-linear-space
		glUniform2f(srcClipRangeLoc_, linearNear, linearFar);
		glUniform2f(dstClipRangeLoc_, nearDistance, farDistance); // this is just the projection matrices clipping plane.
		
		GLuint displayModeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "debugMode");
		glUniform1i(displayModeLoc_, debugMode);
		
		glBindTexture(GL_TEXTURE_2D, getTextureId());
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), textureWithDepthMaterial_);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

public:

	GlMaterial textureWithDepthMaterial_, showDepthMaterial_;
	GlMaterial lodMaterial_;
	GlMaterial* material;
	GlQuad* geometry;
public:
	GlTexture texture;
};

class ExternalTextureViewData : public ViewData
{
public:

	ExternalTextureViewData()
		:
		material_(vs_simpleTexture, fs_simpleTextureExternal)
	{
		geometry = new GlQuad();
		geometry->SetParent(transform);
		externalTextureId_ = GlUtil::createTexture(GL_TEXTURE_EXTERNAL_OES);
	}

	virtual ~ExternalTextureViewData()
	{
		glDeleteTextures(1, &externalTextureId_);
		delete geometry;
		geometry = 0;
	}

	virtual GLuint getTextureId()
	{
		return externalTextureId_;
	}

	virtual void render()
	{
		glDisable(GL_DEPTH_TEST);
		glBindTexture(GL_TEXTURE_EXTERNAL_OES, getTextureId());
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), material_);
		glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
		glEnable(GL_DEPTH_TEST);
		glUseProgram(0);
	}

	virtual void drawCamera(const glm::mat4& viewMat, const glm::mat4& viewProjMat)
	{
		ViewData::drawCamera(viewMat, viewProjMat);

		glBindTexture(GL_TEXTURE_EXTERNAL_OES, getTextureId());
		geometry->render(viewProjMat, viewMat, material_);
		glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
		glUseProgram(0);
	}

protected:

	GlMaterial material_;
	GlQuad* geometry;
	GLuint externalTextureId_;
};

class FisheyeViewData : public TextureViewData
{
public:

	FisheyeViewData()
		:
		material_(vs_simpleTexture, fs_correctFisheyeDistortion)
	{
		externalTextureId = GlUtil::createTexture(GL_TEXTURE_EXTERNAL_OES);

		glGenFramebuffers(1, &fbo_);

		focalLengthLoc_ = glGetUniformLocation(material_.shader_program_, "focalLength");
		kCoefficientsLoc_ = glGetUniformLocation(material_.shader_program_, "kCoefficients");
		kCoefficients2Loc_ = glGetUniformLocation(material_.shader_program_, "kCoefficients2");
		principalPointLoc_ = glGetUniformLocation(material_.shader_program_, "principalPoint");
	}

	virtual void setup(int w, int h, float fov, float n, float f)
	{
		if (apertureWidth == w && apertureHeight == h && fieldOfView == fov && n == nearDistance && f == farDistance)
			return;

		ViewData::setup(w, h, fov, n, f);

		/*
		

		apertureWidth = static_cast<float>(tango.fisheye.cc_width);
		apertureHeight = static_cast<float>(tango.fisheye.cc_height);
		focalLength = static_cast<float>(tango.fisheye.cc_fx);
		apertureRatio = apertureHeight / apertureWidth;

		nearDistance = kCamViewMinDist;
		farDistance = kCamViewMaxDist;

		float xmax = nearDistance * (0.5 * apertureWidth / focalLength);
		float xmin = -xmax;
		float ymax = xmax * apertureRatio;	
		float ymin = -ymax;

		viewProjectionMat = glm::frustum(
			xmin, xmax,
			ymin, ymax,
			nearDistance, farDistance);
		*/

		TangoData& tango = TangoData::instance();

		glUseProgram(material_.shader_program_);
		if (focalLengthLoc_ != -1)
			glUniform2f(focalLengthLoc_, tango.fisheye.intrinsics.cc_fx, tango.fisheye.intrinsics.cc_fy);
		if (principalPointLoc_ != -1)
			glUniform2f(principalPointLoc_, tango.fisheye.intrinsics.cc_cx, tango.fisheye.intrinsics.cc_cy);
		if (kCoefficientsLoc_ != -1)
			glUniform4f(kCoefficientsLoc_, tango.fisheye.intrinsics.cc_distortion[0], tango.fisheye.intrinsics.cc_distortion[1], tango.fisheye.intrinsics.cc_distortion[2], tango.fisheye.intrinsics.cc_distortion[3]);
		if (kCoefficients2Loc_ != -1)
			glUniform1f(kCoefficients2Loc_, tango.fisheye.intrinsics.cc_distortion[4]);
		if (material_.uniform_textureSize0 != -1)
			glUniform2f(material_.uniform_textureSize0, tango.fisheye.intrinsics.cc_width, tango.fisheye.intrinsics.cc_height);
		glUseProgram(0);

		setupTexture(apertureWidth, apertureHeight, GL_R8);
		assert(texture);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
		GlUtil::checkFramebuffer();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	virtual void update()
	{
		TangoData& tango = TangoData::instance();
		tango.updateFisheyeData();

		if (texture)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
			//glClear(GL_COLOR_BUFFER_BIT);
			glViewport(0, 0, texture->width, texture->height);
			//glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_EXTERNAL_OES, externalTextureId);
			geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), material_);
			glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);
		}

		viewToWorldMat = tango.getOC2OWMat(tango.fisheye);
		// update the camera's current position, rotation, and get the scale...
		glm::vec3 scale;
		GlUtil::DecomposeMatrix(viewToWorldMat, lastCameraPosition, lastCameraRotation, scale);
	}

public:

	GLuint externalTextureId;

private:

	GLuint fbo_;
	GlMaterial material_;
	GLuint focalLengthLoc_;
	GLuint principalPointLoc_;
	GLuint kCoefficientsLoc_;
	GLuint kCoefficients2Loc_;
};

class ColorViewData : public TextureViewData
{
public:
	GlTexture prefilteredTexture;

	ColorViewData()
		:
		material_(vs_simpleTexture, fs_correctColorDistortion)
	{
		externalTextureId = GlUtil::createTexture(GL_TEXTURE_EXTERNAL_OES);

		glGenFramebuffers(1, &fbo_);
		
		focalLengthLoc_ = glGetUniformLocation(material_.shader_program_, "focalLength");
		kCoefficientsLoc_ = glGetUniformLocation(material_.shader_program_, "kCoefficients");
		kCoefficients2Loc_ = glGetUniformLocation(material_.shader_program_, "kCoefficients2");
		principalPointLoc_ = glGetUniformLocation(material_.shader_program_, "principalPoint");

	}

	virtual void setup(int w, int h, float fov, float n, float f)
	{
		if (apertureWidth == w && apertureHeight == h && fieldOfView == fov && n == nearDistance && f == farDistance)
			return;

		ViewData::setup(w, h, fov, n, f);
		/*
		

		apertureWidth = static_cast<float>(tango.color.cc_width);
		apertureHeight = static_cast<float>(tango.color.cc_height);
		focalLength = static_cast<float>(tango.color.cc_fx);
		apertureRatio = apertureHeight / apertureWidth;

		nearDistance = kCamViewMinDist;
		farDistance = kCamViewMaxDist;


		float xmax = nearDistance * (0.5 * apertureWidth / focalLength);
		float xmin = -xmax;
		float ymax = xmax * apertureRatio;	
		float ymin = -ymax;

		viewProjectionMat = glm::frustum(
			xmin, xmax,
			ymin, ymax,
			nearDistance, farDistance);
		*/
		TangoData& tango = TangoData::instance();
		glUseProgram(material_.shader_program_);
		if (focalLengthLoc_ != -1)
			glUniform2f(focalLengthLoc_, tango.color.intrinsics.cc_fx, tango.color.intrinsics.cc_fy);
		if (principalPointLoc_ != -1)
			glUniform2f(principalPointLoc_, tango.color.intrinsics.cc_cx, tango.color.intrinsics.cc_cy);
		if (kCoefficientsLoc_ != -1)
			glUniform4f(kCoefficientsLoc_, tango.color.intrinsics.cc_distortion[0], tango.color.intrinsics.cc_distortion[1], tango.color.intrinsics.cc_distortion[2], tango.color.intrinsics.cc_distortion[3]);
		if (kCoefficients2Loc_ != -1)
			glUniform1f(kCoefficients2Loc_, tango.color.intrinsics.cc_distortion[4]);
		if (material_.uniform_textureSize0 != -1)
			glUniform2f(material_.uniform_textureSize0, tango.color.intrinsics.cc_width, tango.color.intrinsics.cc_height);
		glUseProgram(0);

		setupTexture(w, h, GL_RGBA8, 8);
		glBindTexture(GL_TEXTURE_2D, texture->id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		prefilteredTexture = GlTexture::create(GL_TEXTURE_2D, w/4, h/4, GL_RGBA8);
	}

	

	virtual void update()
	{
		TangoData& tango = TangoData::instance();
		tango.updateColorData();

		if (texture)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
			GlUtil::checkFramebuffer();

			//glClear(GL_COLOR_BUFFER_BIT);
			glViewport(0, 0, texture->width, texture->height);
			//glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_EXTERNAL_OES, externalTextureId);
			geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), material_);
			glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
			glBindTexture(GL_TEXTURE_2D, texture->id);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
		
			// test the mipmapping!
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, prefilteredTexture->id, 0);
			GlUtil::checkFramebuffer();
			glViewport(0, 0, prefilteredTexture->width, prefilteredTexture->height);
			renderTextureLod(texture, 2);		
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);
		}

		viewToWorldMat = tango.getOC2OWMat(tango.color);
		// update the camera's current position, rotation, and get the scale...
		glm::vec3 scale;
		GlUtil::DecomposeMatrix(viewToWorldMat, lastCameraPosition, lastCameraRotation, scale);
	}

public:

	GLuint externalTextureId;

private:

	GLuint fbo_;
	GlMaterial material_;
	GLuint focalLengthLoc_;
	GLuint principalPointLoc_;
	GLuint kCoefficientsLoc_;
	GLuint kCoefficients2Loc_;
};

class PointCloudViewData : public ViewData
{
public:
	GlPointcloud* pointclouds;

	PointCloudViewData()
	{
		pointclouds = 0;
	}

	virtual ~PointCloudViewData()
	{
		delete pointclouds;
		pointclouds = 0;
	}

	virtual void setup(int w, int h, float fov, float n, float f)
	{
		if (apertureWidth == w && apertureHeight == h && fieldOfView == fov && n == nearDistance && f == farDistance)
			return;

		ViewData::setup(w, h, fov, n, f);

		if (pointclouds == 0)
		{
			pointclouds = new GlPointcloud();
		}
	}

	virtual void update()
	{
		TangoData& tango = TangoData::instance();

		bool isUpdated = tango.updatePointcloudData();

		if (isUpdated)
		{
			// we must lock, because we don't want the depth buffers updated after we get the pose,
			// and before we have uploaded the data to GL.
			ScopedMutex sm(tango.pointcloud.mutex);

			viewToWorldMat = tango.getOC2OWMat(tango.pointcloud);
			// update the camera's current position, rotation, and get the scale...
			glm::vec3 scale;
			GlUtil::DecomposeMatrix(viewToWorldMat, lastCameraPosition, lastCameraRotation, scale);

			pointclouds->updatePositions(tango.depth_buffer_size, tango.depth_buffer, viewToWorldMat);			
		}		
	}

	virtual void render()
	{
		pointclouds->render(viewProjectionMat, glm::inverse(viewToWorldMat), pointclouds->defaultMaterial, 5.0);
	}

	virtual void drawCamera(const glm::mat4& viewMat, const glm::mat4& viewProjMat)
	{
		ViewData::drawCamera(viewMat, viewProjMat);

		pointclouds->render(viewProjMat, viewMat, pointclouds->defaultMaterial, 1.0);
	}
};

class ImuViewData : public ViewData
{
public:
	Trace* trace;

	ImuViewData()
	{
		trace = new Trace();
	}

	virtual ~ImuViewData()
	{
		delete trace;
		trace = 0;
	}

	void update()
	{
		TangoData& tango = TangoData::instance();
		tango.updateViewData();

		viewToWorldMat = tango.getOC2OWMat(tango.view);
		// update the camera's current position, rotation, and get the scale...
		glm::vec3 scale;
		GlUtil::DecomposeMatrix(viewToWorldMat, lastCameraPosition, lastCameraRotation, scale);
	}

	virtual void drawCamera(const glm::mat4& viewMat, const glm::mat4& viewProjMat)
	{
		ViewData::drawCamera(viewMat, viewProjMat);

		trace->UpdateVertexArray(lastCameraPosition);
		trace->render(viewProjMat, viewMat);
	}
};

class DepthViewData : public TextureViewData
{
public:
	GlTexture depthTexture;
	GlTexture rgbdTexture;
	GlTexture cleanDepthTexture;
	//GlTexture warpedRgbdTexture;

	DepthViewData()
	{
	}

	virtual void render()
	{
		if (getTextureId() == 0)
			return;
		
		glUseProgram(textureWithDepthMaterial_.shader_program_);
		GLuint srcClipRangeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "srcClipRange");
		GLuint dstClipRangeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "dstClipRange");
		GLuint srcDepthTypeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "srcDepthType");
		glUniform1i(srcDepthTypeLoc_, 0); // depth is in fragment-space
		glUniform2f(srcClipRangeLoc_, nearDistance, farDistance);
		glUniform2f(dstClipRangeLoc_, nearDistance, farDistance);
		
		glBindTexture(GL_TEXTURE_2D, getTextureId());
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), textureWithDepthMaterial_);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	virtual void setup(int w, int h, float fov, float n, float f)
	{
		if (apertureWidth == w && apertureHeight == h && fieldOfView == fov && n == nearDistance && f == farDistance)
			return;

		ViewData::setup(w, h, fov, n, f);

		setupTexture(w, h, GL_RGBA32F);

		depthTexture = GlTexture::create(GL_TEXTURE_2D, w, h, GL_DEPTH_COMPONENT32F);
		rgbdTexture = GlTexture::create(GL_TEXTURE_2D, w, h, GL_RGBA32F);
		cleanDepthTexture = GlTexture::create(GL_TEXTURE_2D, w, h, GL_RGBA32F);
		//warpedRgbdTexture = GlTexture::create(GL_TEXTURE_2D, w, h, GL_RGBA32F);

		glUseProgram(showDepthMaterial_.shader_program_);
		GLuint srcClipRangeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcClipRange");
		GLuint srcImageTypeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcImageType");
		GLuint srcDepthTypeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcDepthType");
		glUniform1i(srcImageTypeLoc_, 1); // rgbd image
		glUniform1i(srcDepthTypeLoc_, 0); // depth is in fragment-space
		glUniform2f(srcClipRangeLoc_, nearDistance, farDistance); // this is fixed.
		glUseProgram(0);
	}
};

class WarpedViewData : public TextureViewData
{
public:

	GlTexture depthTexture;
	GlTexture cleanTextureRgbd;
	GlPlaneMesh* mesh_;

	WarpedViewData()
	{
		mesh_ = 0;
	}

	virtual void setup(int w, int h, float fov, float n, float f)
	{
		if (apertureWidth == w && apertureHeight == h && fieldOfView == fov && n == nearDistance && f == farDistance)
			return;

		ViewData::setup(w, h, fov, n, f);

		setupTexture(w, h, GL_RGBA32F);

		depthTexture = GlTexture::create(GL_TEXTURE_2D, w, h, GL_DEPTH_COMPONENT32F);
		cleanTextureRgbd = GlTexture::create(GL_TEXTURE_2D, w, h, GL_RGBA32F);

		delete mesh_;
		mesh_ = new GlPlaneMesh(w, h);

		{
		glUseProgram(showDepthMaterial_.shader_program_);
		GLuint srcClipRangeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcClipRange");
		GLuint srcImageTypeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcImageType");
		GLuint srcDepthTypeLoc_ = glGetUniformLocation(showDepthMaterial_.shader_program_, "srcDepthType");
		glUniform1i(srcImageTypeLoc_, 1); // RGBD image
		glUniform1i(srcDepthTypeLoc_, 0); // depth is in normalized-linear-space
		glUniform2f(srcClipRangeLoc_, nearDistance, farDistance); // this is fixed.
		glUseProgram(0);
		}

		{
		glUseProgram(textureWithDepthMaterial_.shader_program_);
		GLuint srcClipRangeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "srcClipRange");
		GLuint dstClipRangeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "dstClipRange");
		GLuint srcDepthTypeLoc_ = glGetUniformLocation(textureWithDepthMaterial_.shader_program_, "srcDepthType");
		glUniform1i(srcDepthTypeLoc_, 0); // depth is in fragment-space
		glUniform2f(srcClipRangeLoc_, nearDistance, farDistance);
		glUniform2f(dstClipRangeLoc_, nearDistance, farDistance);
		glUseProgram(0);
		}
	}

	virtual void render()
	{
		if (getTextureId() == 0)
			return;

		glBindTexture(GL_TEXTURE_2D, getTextureId());
		geometry->render(glm::mat4(1.0f), glm::mat4(1.0f), textureWithDepthMaterial_);
		glBindTexture(GL_TEXTURE_2D, 0);		
		glUseProgram(0);
	}

};
