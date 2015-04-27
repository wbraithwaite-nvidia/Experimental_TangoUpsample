
#define GLM_FORCE_RADIANS
#define USE_UPRES_IN_DEPTHCOLORSPACE 1

#include <jni.h>
#include <string>
#include <iostream>

#include "TangoUpsampleUtil.h"

#include "tango-gl-renderer/axis.h"
#include "tango-gl-renderer/camera.h"
#include "tango-gl-renderer/cube.h"
#include "tango-gl-renderer/frustum.h"
#include "tango-gl-renderer/GlUtil.h"
#include "tango-gl-renderer/grid.h"
#include "tango-gl-renderer/trace.h"
#include "tango-gl-renderer/GlMaterial.h"
#include "GlBilateralGrid.h"
#include "GlPointcloudRenderer.h"
#include "GlDepthUpsampler.h"

#include "Tango.h"
#include "TangoRenderer.h"

#define POINTCLOUD_RESX 256
#define POINTCLOUD_RESY 256
#define NUM_LEVELS 6

const float kZero = 0.0f;
const glm::vec3 kZeroVec3 = glm::vec3(0.0f, 0.0f, 0.0f);
const glm::quat kZeroQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
// Scale frustum size for closer near clipping plane.
const float kFovScaler = 1.0f;//0.1f;

TangoData* tango = 0;

TangoData* setupTango()
{
	return &TangoData::instance();
}


// Render camera's parent transformation.
// This object is a pivot transformtion for render camera to rotate around.
Transform* cam_parent_transform = 0;
// Render camera.
Camera *cam = 0;
ColorViewData* colorData = 0;
DepthViewData* depthData = 0;
ImuViewData* imuData = 0;
FisheyeViewData* fisheyeData = 0;
PointCloudViewData* pointCloudData = 0;
WarpedViewData* warpedData = 0;
Grid* ground = 0;
Grid* ar_grid = 0;
Cube *cube = 0;

GlDepthUpsampler* depthUpsampler = 0;
GlPointcloudRenderer* pointCloudRenderer = 0;

// Single finger touch positional values.
// First element in the array is x-axis touching position.
// Second element in the array is y-axis touching position.
float cam_start_angle[2];
float cam_cur_angle[2];

// Double finger touch distance value.
float cam_start_dist;
float cam_cur_dist;

enum CameraType {
	THIRD_PERSON = 0,
	FIRST_PERSON = 1,
	FIRST_PERSON_COLOR = 2,
	FIRST_PERSON_FISHEYE = 3,
	FIRST_PERSON_POINTCLOUD = 4,
	NUM_CAMERA_MODES = 5,
};

CameraType camera_type = FIRST_PERSON;
static int cameraSubType[NUM_CAMERA_MODES] = { 0 };

// Min/max clamp value of camera observation distance.
const float kCamViewMinDist = 0.5f;
const float kCamViewMaxDist = 20.f;

// Render camera observation distance in third person camera mode.
const float kThirdPersonCameraDist = 7.0f;

// Render camera observation distance in top down camera mode.
const float kTopDownCameraDist = 5.0f;

// Zoom in speed.
const float kZoomSpeed = 10.0f;

// FOV set up values.
// Third and top down camera's FOV is 65 degrees.
// First person is color camera's FOV.
const float kFov = 65.0f;

// Increment value each time move AR elements.
const float kArElementIncrement = 0.05f;

// AR grid position, can be modified based on the real world scene.
const glm::vec3 kGridPosition = glm::vec3(0.0f, 1.0f, -1.0f);
// AR grid rotation, 90 degrees around x axis.
const glm::quat kArGridRotation = glm::quat(0.70711f, -0.70711f, 0.0f, 0.0f);

// AR cube dimension, based on real world scene.
const glm::vec3 kCubeScale = glm::vec3(0.5, 5, 0.5);
// AR cube position in world coordinate.
const glm::vec3 kCubePosition = glm::vec3(0, kCubeScale[1] / 2, -3);

// Height offset is used for offset height of motion tracking
// pose data. Motion tracking start position is (0,0,0). Adding
// a height offset will give a more reasonable pose while a common
// human is holding the device. The units is in meters.
const glm::vec3 kFloorPosition = glm::vec3(0.0f, -1.4, 0.0f);
const glm::vec3 groundPlanePosition = glm::vec3(0.0f, 0.0, 0.0f);
glm::vec3 world_position = kFloorPosition;


void ensureViewData()
{
	TangoData& tango = *setupTango();

	if (depthData)
		depthData->setup(POINTCLOUD_RESX, POINTCLOUD_RESY / tango.pointcloud.intrinsics.aspect, tango.pointcloud.intrinsics.cc_fovx, kCamViewMinDist, kCamViewMaxDist);
	if (pointCloudData)
		pointCloudData->setup(tango.pointcloud.intrinsics.cc_width, tango.pointcloud.intrinsics.cc_height, tango.pointcloud.intrinsics.cc_fovx, kCamViewMinDist, kCamViewMaxDist);
	if (imuData)
		imuData->setup(0, 0, tango.color.intrinsics.cc_fovx, kCamViewMinDist, kCamViewMaxDist);
	if (colorData)
		colorData->setup(tango.color.intrinsics.cc_width, tango.color.intrinsics.cc_height, tango.color.intrinsics.cc_fovx, kCamViewMinDist, kCamViewMaxDist);
	if (fisheyeData)
		fisheyeData->setup(tango.fisheye.intrinsics.cc_width, tango.fisheye.intrinsics.cc_height, tango.fisheye.intrinsics.cc_fovx, kCamViewMinDist, kCamViewMaxDist);
}

void updateViewData()
{
	if (imuData)
		imuData->update();
	if (pointCloudData)
		pointCloudData->update();
	if (colorData)
		colorData->update();
	if (fisheyeData)
		fisheyeData->update();
	if (depthData)
		depthData->update();
	if (warpedData)
		warpedData->update();
}

bool setupGlResources()
{
	TangoData& tango = *setupTango();

	cam_parent_transform = new Transform();
	cam = new Camera();

	ground = new Grid(1.0f, 100, 100);
	cube = new Cube();

	camera_type = THIRD_PERSON;
	cam->SetParent(cam_parent_transform);
	cam->SetFieldOfView(kFov);

	ground->SetPosition(world_position);
	cube->SetPosition(kCubePosition + world_position);
	cube->SetScale(kCubeScale);

	// create the tango source objects...

	imuData = new ImuViewData();
	colorData = new ColorViewData();
	pointCloudData = new PointCloudViewData();
	fisheyeData = new FisheyeViewData();
	//warpedData = new WarpedViewData();
	depthData = new DepthViewData();

	depthUpsampler = new GlDepthUpsampler();

	pointCloudRenderer = new GlPointcloudRenderer(POINTCLOUD_RESX, POINTCLOUD_RESY, NUM_LEVELS);

	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	return true;
}

// Update viewport according to surface dimensions, always use image plane's
// ratio and make full use of the screen.
void setupViewport(int portX, int portY, int portWidth, int portHeight, float imageRatio)
{
	float portRatio = static_cast<float>(portWidth) / portHeight;

	if (imageRatio < portRatio)
		glViewport(portX, portY, portWidth, portWidth / imageRatio);
	else
		glViewport(portX + (portWidth - portHeight * imageRatio) / 2, portY, portHeight * imageRatio, portHeight);

	cam->SetAspectRatio(imageRatio);
}

void drawScene(const glm::mat4& projection_mat, const glm::mat4& view_mat)
{
	ground->render(projection_mat, view_mat);
}

bool render(int portWidth, int portHeight)
{
	TangoData& tango = *setupTango();

	ensureViewData();
	updateViewData();

	if (pointCloudData && colorData)
	{
		if (pointCloudData->lastUpdateId != tango.pointcloud.updateId)
		{
			pointCloudData->lastUpdateId = tango.pointcloud.updateId;

			// store a snapshot of the current hi-res color values in the pointcloud vertices
			// by inverse mapping the colorData into the pointcloud.
			// this is sparse, only storing a single color value for each point.

			pointCloudData->pointclouds->updateColorsFromTexture(
				colorData->texture->id, colorData->viewProjectionMat, colorData->viewToWorldMat);
		}
	}

	if (depthUpsampler)
	{
		// ensure the depthupsampler is the right format.
		depthUpsampler->setup(POINTCLOUD_RESX, POINTCLOUD_RESY, NUM_LEVELS);

		if (colorData)
		{
			depthUpsampler->updateColorPyramid(colorData->texture);
		}

		if (pointCloudData)
		{
			// BUG: this fails with imuData (probably because of a bad projection matrix!)
			depthData->viewProjectionMat = colorData->viewProjectionMat;
			depthData->viewToWorldMat = imuData->viewToWorldMat;

			depthUpsampler->renderPointcloudToTexture(
				pointCloudData->pointclouds, 
				depthData->viewProjectionMat, glm::inverse(depthData->viewToWorldMat), pointCloudData->pointclouds->defaultMaterial);

			depthUpsampler->updateRgbdPyramid(depthUpsampler->pointcloudColorTexture_, depthUpsampler->pointcloudDepthTexture_);

			depthUpsampler->upsampleRgbd();
		}
	}

	/*
	if (depthData && pointCloudData)
	{
		bool depthUpdated = false;

		if (pointCloudData->lastUpdateId != tango.pointcloud.updateId)
		{
			pointCloudData->lastUpdateId = tango.pointcloud.updateId;
		}

		if (colorData->lastUpdateId != tango.color.updateId)
		{
			colorData->lastUpdateId = tango.color.updateId;

			// store a snapshot of the current hi-res color values in the pointcloud vertices
			// by inverse mapping the colorData into the pointcloud.
			// this is sparse, only storing a single color value for each point.
			pointCloudData->pointclouds->updateColorsFromTexture(
				colorData->prefilteredTexture->id, colorData->viewProjectionMat, colorData->viewToWorldMat);

			// render the points in pointcloud-space...
			pointCloudRenderer->renderToTextureClear(
				depthData->rgbdTexture->id, depthData->depthTexture->id, depthData->depthTexture->width, depthData->depthTexture->height);
			pointCloudRenderer->renderToTexture(
				pointCloudData->pointclouds, depthData->depthTexture->width*1.0 / POINTCLOUD_RESX,
				depthData->rgbdTexture->id, depthData->depthTexture->id, depthData->depthTexture->width, depthData->depthTexture->height,
				colorData->viewProjectionMat, glm::inverse(colorData->viewToWorldMat),
				pointCloudData->pointclouds->defaultMaterial);

			// depthData is now in colorData-view
			depthData->viewToWorldMat = colorData->viewToWorldMat;
			depthData->viewProjectionMat = colorData->viewProjectionMat;
			depthData->nearDistance = colorData->nearDistance;
			depthData->farDistance = colorData->farDistance;
			depthData->fieldOfView = colorData->fieldOfView;

			// update the depthData matrices...
			glm::vec3 scale;
			GlUtil::DecomposeMatrix(depthData->viewToWorldMat, depthData->lastCameraPosition, depthData->lastCameraRotation, scale);

			depthUpdated = true;
		}


		if (depthUpdated)
		{
			// ensure the depthupsampler is the right format.
			depthUpsampler->setup(depthData->rgbdTexture->width, depthData->rgbdTexture->height, NUM_LEVELS);
			// build the downsampling pyramids
			depthUpsampler->updateRgbdPyramid(depthData->rgbdTexture, depthData->depthTexture);
			depthUpsampler->updateColorPyramid(colorData->texture);
			// upsample the RGBD data.
			depthUpsampler->upsampleRgbd();
		}
	}
	*/

	// prepare GL...
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glClearColor(0.1f, 0.1f, 0.1f, 0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	// matrices to render from.
	glm::mat4 projection_mat;
	glm::mat4 view_mat;

	if (camera_type == FIRST_PERSON)
	{
		int camSubMode = cameraSubType[camera_type] % 2;

		std::vector<ViewData*> views;
		if (imuData)
			views.push_back(imuData);
		if (colorData)
			views.push_back(colorData);
		if (fisheyeData)
			views.push_back(fisheyeData);
		if (pointCloudData)
			views.push_back(pointCloudData);
		if (depthData)
			views.push_back(depthData);

		int numItems = views.size();
		int itemWidth = portWidth;
		int itemHeight = portHeight;
		float aspect = (float(portHeight) / portWidth) * (float(itemWidth) / itemHeight);
		int gridWidth = ceilf(sqrtf(numItems*aspect));
		int gridHeight = ceilf(sqrtf(numItems / aspect));
		int cellWidth = iDivUp(portWidth, gridWidth);
		int cellHeight = iDivUp(portHeight, gridHeight);

		int i = 0;
		for (int iy = 0; iy < gridHeight; ++iy)
		{
			for (int ix = 0; ix < gridWidth; ++ix)
			{
				if (i >= views.size())
				{
					iy = gridHeight;
					break;
				}

				int xoffset = ix*cellWidth;
				int yoffset = iy*cellHeight;

				if (views[i])
				{
					if (camSubMode == 0)
					{
						glViewport(xoffset, yoffset, cellWidth, cellHeight);
						//setupViewport(xoffset, yoffset, cellWidth, cellHeight, views[i]->apertureRatio);

						views[i]->render();

						projection_mat = views[i]->viewProjectionMat;
						view_mat = glm::inverse(views[i]->viewToWorldMat);
					}
					else
					{
						setupViewport(xoffset, yoffset, cellWidth, cellHeight, float(portHeight / 2) / (portWidth / 3));

						// get camera matrices...
						glm::quat parent_cam_rot = glm::rotate(kZeroQuat, -cam_cur_angle[0], glm::vec3(kZero, 1.0f, kZero));
						parent_cam_rot = glm::rotate(parent_cam_rot, -cam_cur_angle[1], glm::vec3(1.0f, kZero, kZero));
						cam_parent_transform->SetRotation(parent_cam_rot);
						//cam_parent_transform->SetPosition(colorCamera_position);
						cam->SetPosition(glm::vec3(kZero, kZero, cam_cur_dist));
						projection_mat = cam->GetProjectionMatrix();
						view_mat = cam->GetViewMatrix();

						views[i]->drawCamera(view_mat, projection_mat);
					}

					drawScene(projection_mat, view_mat);
				}

				i++;
			}
		}
	}
	else
	{
		//setupViewport(0, 0, portWidth, portHeight, portWidth/portHeight);
		glViewport(0, 0, portWidth, portHeight);

		if (camera_type == FIRST_PERSON_COLOR)
		{
			int camSubMode = cameraSubType[camera_type] % (1 + ((depthUpsampler)?depthUpsampler->numLevels_:0));

			if (camSubMode == 0)
			{
				if (colorData)
				{
					//setupViewport(0, 0, portWidth, portHeight, colorData->apertureRatio);
					colorData->renderTextureLod(colorData->texture, 0);

					projection_mat = colorData->viewProjectionMat;
					view_mat = glm::inverse(colorData->viewToWorldMat);
				}
			}
			else
			{
				int level = camSubMode - 1;
				if (colorData && depthUpsampler)
				{
					//setupViewport(0, 0, depthUpsampler->width_, depthUpsampler->height_, 1.0);
					colorData->renderTexture(depthUpsampler->colorTexturePyramid_[level]->id);

					projection_mat = colorData->viewProjectionMat;
					view_mat = glm::inverse(colorData->viewToWorldMat);
				}
			}
		}
		else if (camera_type == FIRST_PERSON_FISHEYE)
		{
			int camSubMode = cameraSubType[camera_type] % (1);

			if (camSubMode == 0)
			{
				if (fisheyeData)
				{
					//setupViewport(0, 0, portWidth, portHeight, fisheyeData->apertureRatio);
					fisheyeData->render();

					projection_mat = fisheyeData->viewProjectionMat;
					view_mat = glm::inverse(fisheyeData->viewToWorldMat);
				}
			}
		}
		else if (camera_type == FIRST_PERSON_POINTCLOUD)
		{
			int camSubMode = cameraSubType[camera_type] % (1 + ((depthUpsampler)?depthUpsampler->numLevels_:0));

			if (camSubMode == 0)
			{
				pointCloudData->pointclouds->render(depthData->viewProjectionMat, glm::inverse(depthData->viewToWorldMat), pointCloudData->pointclouds->defaultMaterial, 5.0);

				projection_mat = depthData->viewProjectionMat;
				view_mat = glm::inverse(depthData->viewToWorldMat);
			}
			else
			{
				int level = camSubMode - 1;

				if (depthData && depthUpsampler)
				{
					//depthData->renderTexture(depthUpsampler->depthTexturePyramid_[level]->id);
					//depthData->renderTexture(depthUpsampler->depthUpsampleTexture_[level]->id);
					//depthData->renderTexture(depthUpsampler->bilateralGrids_[0]->gridTextures_[1]->id);

					glUseProgram(depthData->showDepthMaterial_.shader_program_);
					GLuint srcClipRangeLoc_ = glGetUniformLocation(depthData->showDepthMaterial_.shader_program_, "srcClipRange");
					GLuint srcImageTypeLoc_ = glGetUniformLocation(depthData->showDepthMaterial_.shader_program_, "srcImageType");
					GLuint srcDepthTypeLoc_ = glGetUniformLocation(depthData->showDepthMaterial_.shader_program_, "srcDepthType");
					glUniform1i(srcImageTypeLoc_, 1); // rgbd image
					glUniform1i(srcDepthTypeLoc_, 0); // depth is in fragment-space
					glUniform2f(srcClipRangeLoc_, pointCloudData->nearDistance, pointCloudData->farDistance); // this is fixed.

					depthData->renderTexture(depthUpsampler->depthUpsampleTexture_[0]->id, &depthData->showDepthMaterial_);
					projection_mat = depthData->viewProjectionMat;
					view_mat = glm::inverse(depthData->viewToWorldMat);
				}
			}

			/*{
				if (depthData)
				{
					//setupViewport(0, 0, depthUpsampler->width_ * 2, depthUpsampler->height_ * 2, 1);
					depthData->renderTexture(depthUpsampler->depthUpsampleTexture_[level]->id, &depthData->showDepthMaterial_);
					projection_mat = depthData->viewProjectionMat;
					view_mat = glm::inverse(depthData->viewToWorldMat);
				}
			}*/
			/*

			int camSubMode = cameraSubType[camera_type] % 6;

			if (camSubMode == 0)
			{
			if (pointCloudData)
			{
			setupViewport(0, 0, portWidth, portHeight, pointCloudData->apertureRatio);
			pointCloudData->render();
			projection_mat = pointCloudData->viewProjectionMat;
			view_mat = glm::inverse(pointCloudData->viewToWorldMat);
			}
			}
			else if (camSubMode == 1)
			{
			if (depthData)
			{
			setupViewport(0, 0, portWidth, portHeight, depthData->apertureRatio);
			depthData->renderTexture(depthData->depthTexture->id, &depthData->showDepthMaterial_);
			projection_mat = depthData->viewProjectionMat;
			view_mat = glm::inverse(depthData->viewToWorldMat);
			}
			}
			else if (camSubMode == 2)
			{
			if (depthData)
			{
			setupViewport(0, 0, portWidth, portHeight, depthData->apertureRatio);
			depthData->renderTexture(depthData->texture->id, &depthData->showDepthMaterial_);
			projection_mat = depthData->viewProjectionMat;
			view_mat = glm::inverse(depthData->viewToWorldMat);
			}
			}
			else if (camSubMode == 3)
			{
			if (depthData)
			{
			setupViewport(0, 0, portWidth, portHeight, depthData->apertureRatio);
			depthData->renderTexture(depthData->cleanDepthTexture->id, &depthData->showDepthMaterial_);
			//depthData->renderTexture(depthData->rgbdTexture->id);
			projection_mat = depthData->viewProjectionMat;
			view_mat = glm::inverse(depthData->viewToWorldMat);
			}
			}
			else if (camSubMode == 4)
			{
			if (depthData)
			{
			setupViewport(0, 0, portWidth, portHeight, 1);
			depthData->renderTexture(bilateralGrid_->gridTextures_[1]->id);
			}
			}
			else if (camSubMode == 5)
			{
			if (depthData)
			{
			setupViewport(0, 0, portWidth, portHeight, depthData->apertureRatio);
			depthData->renderTexture(bilateralResultTexture_->id, &depthData->showDepthMaterial_);
			projection_mat = depthData->viewProjectionMat;
			view_mat = glm::inverse(depthData->viewToWorldMat);
			}
			}*/
		}
		else if (camera_type == THIRD_PERSON)
		{
			setupViewport(0, 0, portWidth, portHeight, float(portHeight) / portWidth);

			// get camera matrices...
			glm::quat parent_cam_rot = glm::rotate(kZeroQuat, -cam_cur_angle[0], glm::vec3(kZero, 1.0f, kZero));
			parent_cam_rot = glm::rotate(parent_cam_rot, -cam_cur_angle[1], glm::vec3(1.0f, kZero, kZero));
			cam_parent_transform->SetRotation(parent_cam_rot);
			//cam_parent_transform->SetPosition(colorCamera_position);
			cam->SetPosition(glm::vec3(kZero, kZero, cam_cur_dist));
			projection_mat = cam->GetProjectionMatrix();
			view_mat = cam->GetViewMatrix();

			// draw glyphs...
			if (imuData)
				imuData->drawCamera(view_mat, projection_mat);
			if (colorData)
				colorData->drawCamera(view_mat, projection_mat);
			if (fisheyeData)
				fisheyeData->drawCamera(view_mat, projection_mat);
			if (depthData)
				depthData->drawCamera(view_mat, projection_mat);
			if (pointCloudData)
				pointCloudData->drawCamera(view_mat, projection_mat);
			if (warpedData)
				warpedData->drawCamera(view_mat, projection_mat);
		}

		drawScene(projection_mat, view_mat);
	}


	return true;
}

void setCamera(CameraType cameraIndex)
{
	if (camera_type == cameraIndex)
	{
		// rotate through the different subtypes.
		cameraSubType[cameraIndex]++;
	}

	camera_type = cameraIndex;

	cam_cur_angle[0] = cam_cur_angle[1] = cam_cur_dist = kZero;

	if (cameraIndex == THIRD_PERSON)
	{
		int camSubMode = cameraSubType[camera_type] % 2;

		if (camSubMode == 0)
		{
			// third
			cam_parent_transform->SetRotation(kZeroQuat);
			cam->SetPosition(kZeroVec3);
			cam->SetRotation(kZeroQuat);
			cam_cur_dist = kThirdPersonCameraDist;
			cam_cur_angle[0] = -PI / 4.0f;
			cam_cur_angle[1] = PI / 4.0f;
		}
		else if (camSubMode == 1)
		{
			// top
			cam_parent_transform->SetRotation(kZeroQuat);
			cam->SetPosition(kZeroVec3);
			cam->SetRotation(kZeroQuat);
			cam_cur_dist = kTopDownCameraDist;
			cam_cur_angle[1] = PI / 2.0f;
		}
	}
	else
	{
		// remove the third-person camera...
		cam_parent_transform->SetPosition(kZeroVec3);
		cam_parent_transform->SetRotation(kZeroQuat);
	}

	switch (cameraIndex)
	{
	case FIRST_PERSON:
	case FIRST_PERSON_COLOR:
	case FIRST_PERSON_FISHEYE:
	case FIRST_PERSON_POINTCLOUD:

		if (colorData)
			colorData->setRenderMode(0);
		if (pointCloudData)
			pointCloudData->setRenderMode(0);
		if (depthData)
			depthData->setRenderMode(0);
		if (imuData)
			imuData->setRenderMode(0);
		if (fisheyeData)
			fisheyeData->setRenderMode(0);
		break;

	case THIRD_PERSON:

		if (colorData)
			colorData->setRenderMode(1);
		if (pointCloudData)
			pointCloudData->setRenderMode(1);
		if (depthData)
			depthData->setRenderMode(1);
		if (imuData)
			imuData->setRenderMode(1);
		if (fisheyeData)
			fisheyeData->setRenderMode(1);
		break;
	}

}

void reset()
{
	setCamera(camera_type);
	imuData->trace->ClearVertexArray();
}

#ifdef __cplusplus
extern "C" {
#endif

	JNIEXPORT jint JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_initialize(
		JNIEnv* env, jobject, jobject activity)
	{
		setupTango();

		TangoErrorType err = TangoData::instance().initialize(env, activity);
		if (err != TANGO_SUCCESS) {
			if (err == TANGO_INVALID) {
				LOGE("Tango Service version mis-match");
			}
			else {
				LOGE("Tango Service initialize internal error");
			}
		}
		if (err == TANGO_SUCCESS) {
			LOGI("Tango service initialize success");
		}
		return static_cast<int>(err);
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_setupConfig(
		JNIEnv*, jobject, bool useAutoRecovery, bool useColorCamera, bool useDepthCamera)
	{
		setupTango();
		if (!TangoData::instance().setConfig(useAutoRecovery, useColorCamera, useDepthCamera))
		{
			LOGE("Tango set config failed");
		}
		else {
			LOGI("Tango set config success");
		}
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_connectTexture(
		JNIEnv*, jobject)
	{
		setupTango();
		TangoData::instance().connectTextures((colorData) ? colorData->externalTextureId : 0, (fisheyeData) ? fisheyeData->externalTextureId : 0);
	}


	JNIEXPORT jint JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_connectService(JNIEnv*, jobject)
	{
		setupTango();
		TangoErrorType err = TangoData::instance().Connect();
		if (err == TANGO_SUCCESS)
		{
			TangoData::instance().GetExtrinsics();
			TangoData::instance().GetIntrinsics();

			reset();

			LOGI("Tango Service connect success");
		}
		else
		{
			LOGE("Tango Service connect failed");
		}
		return static_cast<int>(err);
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_connectCallbacks(
		JNIEnv*, jobject)
	{
		setupTango();
		if (!TangoData::instance().connectCallbacks())
		{
			LOGE("Tango connectCallbacks failed");
		}
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_disconnectService(
		JNIEnv*, jobject)
	{
		setupTango();
		TangoData::instance().Disconnect();
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_destroyGlResources(
		JNIEnv*, jobject)
	{
		delete cam;
		cam = 0;

		delete colorData;
		colorData = 0;

		delete imuData;
		imuData = 0;

		delete pointCloudData;
		pointCloudData = 0;

		delete depthData;
		depthData = 0;

		delete fisheyeData;
		fisheyeData = 0;

		delete warpedData;
		warpedData = 0;

		delete ground;
		ground = 0;

		delete cube;
		cube = 0;

		delete pointCloudRenderer;
		pointCloudRenderer = 0;

		delete depthUpsampler;
		depthUpsampler = 0;
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_setupGlResources(
		JNIEnv*, jobject)
	{
		setupTango();
		setupGlResources();
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_render(
		JNIEnv*, jobject, int portWidth, int portHeight)
	{
		setupTango();
		render(portWidth, portHeight);
	}

	JNIEXPORT jboolean JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_getIsLocalized(
		JNIEnv*, jobject)
	{
		setupTango();
		ScopedMutex sm(TangoData::instance().pose_mutex);
		return TangoData::instance().is_localized;
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_resetMotionTracking(
		JNIEnv*, jobject)
	{
		setupTango();
		reset();
		// TangoData::instance().ResetMotionTracking();
	}

	JNIEXPORT void JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_setCamera(
		JNIEnv*, jobject, int cameraIndex)
	{
		setupTango();
		setCamera(static_cast<CameraType>(cameraIndex));
	}

	JNIEXPORT jstring JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_getPoseString(
		JNIEnv* env, jobject)
	{
		setupTango();
		std::string pose_string_cpy = TangoData::instance().getStatusString();
		return (env)->NewStringUTF(pose_string_cpy.c_str());
	}

	JNIEXPORT jstring JNICALL
		Java_com_odd_TangoUpsample_TangoUpsampleNative_getVersionNumber(
		JNIEnv* env, jobject)
	{
		setupTango();
		return (env)->NewStringUTF(TangoData::instance().lib_version_string.c_str());
	}

#ifdef __cplusplus
}
#endif

