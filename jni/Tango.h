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

#ifndef POINT_CLOUD_JNI_EXAMPLE_TANGO_DATA_H_
#define POINT_CLOUD_JNI_EXAMPLE_TANGO_DATA_H_
#define GLM_FORCE_RADIANS

#include "TangoUpsampleUtil.h"
#include <sstream>
#include <stdlib.h>
#include <string>
#include <tango_client_api.h>
#include "tango-gl-renderer/GlUtil.h"

const int kMeterToMillimeter = 1000;
const int kVersionStringLength = 27;
const float kSecondToMillisecond = 1000.0f;

// Opengl to color camera matrix.
const glm::mat4 oc_2_c_mat =
glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

// Start service to opengl world matrix.
const glm::mat4 ss_2_ow_mat =
glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

class TangoData
{
	// structure for a threadsafe snapshot of a pose and any associated data.
	struct ViewPoseData
	{
		ViewPoseData()
		{
			isActive = false;
			isDirty = false;
			timestamp = 0.0f;
			deltaTime = 0.0f;
			deviceToRestMat = glm::mat4(1.0f);
			c_2_imu_mat = glm::mat4(1.0f);
			updateId = 0;
		}

		double timestamp;
		Mutex mutex;
		glm::mat4 deviceToRestMat; // device to start-of-service matrix.
		float deltaTime;	// time since last asynchronous update
		std::string statusString;
		bool isDirty;
		bool isActive;
		int updateId;

		// Tango Intrinsic for camera.
		struct Intrinsics
		{
			int cc_width;
			int cc_height;
			double cc_fx;
			double cc_fy;
			double cc_cx;
			double cc_cy;
			double cc_distortion[5];
			double cc_fovx;
			double cc_fovy;
			double aspect;
		} intrinsics;

		// device to IMU matrix.
		glm::mat4 c_2_imu_mat;

	};

public:
	static TangoData& instance() {
		static TangoData instance;
		return instance;
	}
	TangoData();
	~TangoData();

	TangoErrorType initialize(JNIEnv* env, jobject activity);
	bool setConfig(bool isAutoReset, bool useDepth);
	TangoErrorType Connect();
	bool ConnectCallbacks();
	void Disconnect();

	void ResetMotionTracking();

	// connect GL textures to the camera data.
	void connectTextures(GLuint colorTextureId, GLuint fisheyeTextureId);

	// setup the matrices from the current pose.
	bool GetExtrinsics();
	bool GetIntrinsics();

	glm::mat4 getOC2OWMat(ViewPoseData& viewData);

	// get the pose at timestamp.
	// returns false on error.
	bool getPoseAtTime(double timestamp, TangoPoseData& pose);



	// update all the data.
	void updateColorData();
	void updateFisheyeData();
	void updateViewData();
	bool updatePointcloudData();

	std::string getStatusString();

	Mutex event_mutex;

	float* depth_buffer;
	uint32_t depth_buffer_size;

	TangoPoseData cur_pose_data;
	TangoPoseData prev_pose_data;

	int pose_status_count;

	float depth_average_length;

	uint32_t max_vertex_count;

	Mutex pose_mutex;

	// Device to IMU matrix.
	glm::mat4 d_2_imu_mat;


	std::string event_string;
	std::string lib_version_string;

	//--------------
	int status_count[3];

	ViewPoseData color, pointcloud, view, fisheye;

	// Localization status.
	bool is_localized;
	std::string cur_uuid;

private:
	TangoConfig config_;
};

#endif  // POINT_CLOUD_JNI_EXAMPLE_TANGO_DATA_H_
