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

#include "Tango.h"
#include "TangoUpsampleUtil.h"
#include <vector>

//------------------------------------------------------------------------------
// Get status string based on the pose status code.
static const char* getStatusStringFromStatusCode(TangoPoseStatusType status) 
{
	const char* status_string = 0;
	switch (status) 
	{
		case TANGO_POSE_INITIALIZING:
			status_string = "initializing";
			break;
		case TANGO_POSE_VALID:
			status_string = "valid";
			break;
		case TANGO_POSE_INVALID:
			status_string = "invalid";
			break;
		case TANGO_POSE_UNKNOWN:
			status_string = "unknown";
			break;
		default:
			break;
	}
	return status_string;
}

//------------------------------------------------------------------------------
/// Callback function when new XYZij data available, caller is responsible
/// for allocating the memory, and the memory will be released after the
/// callback function is over.
/// XYZij data updates in 5Hz.
static void onXYZijAvailable(void*, const TangoXYZij* XYZ_ij) 
{
	TangoData& tango = TangoData::instance();
	ScopedMutex sm(tango.pointcloud.mutex);

	// Copying out the depth buffer.
	// Note: the XYZ_ij object will be out of scope after this callback is
	// excuted.
	if (XYZ_ij->xyz_count <= tango.max_vertex_count) 
	{
		if (tango.depth_buffer != 0 && XYZ_ij->xyz != 0) 
		{
			memcpy(tango.depth_buffer, XYZ_ij->xyz, XYZ_ij->xyz_count * 3 * sizeof(float));
		}
	}

	tango.depth_buffer_size = XYZ_ij->xyz_count;

	// Calculate the depth delta frame time, and store current and
	// previous frame timestamp. prev_depth_timestamp used for querying
	// closest pose data. (See in UpdateXYZijData())
	tango.pointcloud.deltaTime = (XYZ_ij->timestamp - tango.pointcloud.timestamp) * kSecondToMillisecond;
	tango.pointcloud.timestamp = XYZ_ij->timestamp;

	// Set xyz_ij dirty flag.
	tango.pointcloud.isDirty = true;
}

//------------------------------------------------------------------------------
// Tango event callback.
static void onTangoEvent(void*, const TangoEvent* e) 
{
	TangoData& tango = TangoData::instance();
	ScopedMutex sm(tango.pose_mutex);

	// Update the status string for debug display.
	std::stringstream string_stream;
	string_stream << "System event: " << e->event_key << ": " << e->event_value;
	tango.event_string = string_stream.str();
}

//------------------------------------------------------------------------------
// This callback function is called when new pose update is available.
static void onPoseAvailable(void*, const TangoPoseData* pose) 
{
	TangoData& tango = TangoData::instance();
	ScopedMutex sm(tango.view.mutex);

	if (pose != 0) 
	{
		tango.cur_pose_data = *pose;

		tango.view.deltaTime = (pose->timestamp - tango.view.timestamp) * kSecondToMillisecond;
		tango.view.timestamp = pose->timestamp;
		tango.view.isDirty = true;
	}
}

//------------------------------------------------------------------------------
TangoData::TangoData() : 
	config_(0)
{
	d_2_imu_mat = glm::mat4(1.0f);
}

//------------------------------------------------------------------------------
// Initialize Tango Service.
TangoErrorType TangoData::initialize(JNIEnv* env, jobject activity) 
{
	// The initialize function perform API and Tango Service version check,
	// if there is a mis-match between API and Tango Service version, the
	// function will return TANGO_INVALID.
	return TangoService_initialize(env, activity);
}

//------------------------------------------------------------------------------
// Set up Tango Configuration handle, and connecting all callbacks.
bool TangoData::setConfig(bool is_auto_recovery, bool useDepth) 
{
	// Get the default TangoConfig.
	config_ = TangoService_getConfig(TANGO_CONFIG_DEFAULT);
	if (config_ == NULL) 
	{
		LOGE("TangoService_getConfig(): Failed");
		return false;
	}


	color.isActive = true;
	fisheye.isActive = true;
	view.isActive = true;

	
	if (TangoConfig_setBool(config_, "config_enable_low_latency_imu_integration", true) != TANGO_SUCCESS) {
		LOGE("config_enable_low_latency_imu_integration(): Failed");
		return false;
	}

	// Turn on auto recovery for motion tracking.
	// Note that the auto-recovery is on by default.
	if (TangoConfig_setBool(config_, "config_enable_auto_recovery", is_auto_recovery) != TANGO_SUCCESS) {
		LOGE("config_enable_auto_recovery(): Failed");
		return false;
	}

	if (TangoConfig_setBool(config_, "config_enable_depth", useDepth) != TANGO_SUCCESS) 
	{
		LOGE("config_enable_depth Failed");
		return false;
	}

	if (useDepth)
	{
		// Get max point cloud elements. The value is used for allocating the depth buffer.
		int temp = 0;
		if (TangoConfig_getInt32(config_, "max_point_cloud_elements", &temp) != TANGO_SUCCESS) 
		{
			LOGE("Get max_point_cloud_elements Failed");
			return false;
		}
		max_vertex_count = static_cast<uint32_t>(temp);

		// Forward allocate the maximum size of depth buffer.
		// max_vertex_count is the vertices count, max_vertex_count*3 is
		// the actual float buffer size.
		depth_buffer = new float[3 * max_vertex_count];

		pointcloud.isActive = true;
	}

	// Get library version string from service.
	if (TangoConfig_getString(
		config_, "tango_service_library_version",
		const_cast<char*>(TangoData::instance().lib_version_string.c_str()),
		kVersionStringLength) != TANGO_SUCCESS) 
	{
		LOGE("Get tango_service_library_version Failed");
		return false;
	}

	



  // Load the most recent ADF.
  char* uuid_list;

  // uuid_list will contain a comma separated list of UUIDs.
  if (TangoService_getAreaDescriptionUUIDList(&uuid_list) != TANGO_SUCCESS) {
    LOGI("TangoService_getAreaDescriptionUUIDList");
  }

  // Parse the uuid_list to get the individual uuids.
  if (uuid_list != NULL && uuid_list[0] != '\0') {
    std::vector<std::string> adf_list;

    char* parsing_char;
    char* saved_ptr;
    parsing_char = strtok_r(uuid_list, ",", &saved_ptr);
    while (parsing_char != NULL) {
      std::string s = std::string(parsing_char);
      adf_list.push_back(s);
      parsing_char = strtok_r(NULL, ",", &saved_ptr);
    }

    int list_size = adf_list.size();
    if (list_size == 0) {
      LOGE("List size is 0");
      return false;
    }
    cur_uuid = adf_list[list_size - 1];
    if (TangoConfig_setString(config_, "config_load_area_description_UUID",
                              adf_list[list_size - 1].c_str()) !=
        TANGO_SUCCESS) {
      LOGE("config_load_area_description_uuid Failed");
      return false;
    } else {
      LOGI("Load ADF: %s", adf_list[list_size - 1].c_str());
    }
  } else {
    LOGE("No area description file available, no file loaded.");
  }
  is_localized = false;

  return true;
}

//------------------------------------------------------------------------------
void onFrameAvailable(void *context, TangoCameraId id)
{
	TangoData& tango = *static_cast<TangoData*>(context);

	// do something with the raw camera data...
	//CT2(buffer->timestamp, buffer->format);
}

//------------------------------------------------------------------------------
bool TangoData::ConnectCallbacks() 
{
	// Attach the onXYZijAvailable callback.
	if (TangoService_connectOnXYZijAvailable(onXYZijAvailable) != TANGO_SUCCESS) 
	{
		LOGI("TangoService_connectOnXYZijAvailable(): Failed");
		return false;
	}

	// Set the reference frame pair after connect to service.
	// Currently the API will set this set below as default.
	TangoCoordinateFramePair pairs;
	pairs.base = TANGO_COORDINATE_FRAME_START_OF_SERVICE;
	pairs.target = TANGO_COORDINATE_FRAME_DEVICE;

	// Attach onPoseAvailable callback.
	if (TangoService_connectOnPoseAvailable(1, &pairs, onPoseAvailable) != TANGO_SUCCESS) 
	{
		LOGI("TangoService_connectOnPoseAvailable(): Failed");
		return false;
	}

	// Set the event callback listener.
	if (TangoService_connectOnTangoEvent(onTangoEvent) != TANGO_SUCCESS) 
	{
		LOGI("TangoService_connectOnTangoEvent(): Failed");
		return false;
	}
	/*
	if (TangoService_connectOnFrameAvailable(TANGO_CAMERA_COLOR, this, onFrameAvailable) != TANGO_SUCCESS) 
	{
		LOGI("TangoService_connectOnPoseAvailable(): Failed");
		return false;
	}
	*/
	return true;
}

//------------------------------------------------------------------------------
// Connect to Tango Service
TangoErrorType TangoData::Connect() 
{
	return TangoService_connect(0, config_);
}

//------------------------------------------------------------------------------
// Disconnect from Tango Service.
void TangoData::Disconnect() 
{
	// Disconnect application from Tango Service.
	TangoService_disconnect();
}

//------------------------------------------------------------------------------
bool TangoData::getPoseAtTime(double timestamp, TangoPoseData& pose) 
{
	// Set the reference frame pair after connect to service.
	// Currently the API will set this set below as default.

	// Device to Start of Service frames pair.
	TangoCoordinateFramePair d_to_ss_pair;
	d_to_ss_pair.base = TANGO_COORDINATE_FRAME_START_OF_SERVICE;
	d_to_ss_pair.target = TANGO_COORDINATE_FRAME_DEVICE;

	// Device to ADF frames pair.
	TangoCoordinateFramePair d_to_adf_pair;
	d_to_adf_pair.base = TANGO_COORDINATE_FRAME_AREA_DESCRIPTION;
	d_to_adf_pair.target = TANGO_COORDINATE_FRAME_DEVICE;

	const char* frame_pair = "Target->Device, Base->Start: ";
	// Before localized, use device to start of service frame pair,
	// after localized, use device to ADF frame pair.
	if (!is_localized) 
	{
		// Should use timestamp, but currently updateTexture() only returns
		// 0 for timestamp, if set to 0.0, the most
		// recent pose estimate for the target-base pair will be returned.
		if (TangoService_getPoseAtTime(timestamp, d_to_ss_pair, &pose) != TANGO_SUCCESS)
			return false;
	} 
	else 
	{
		frame_pair = "Target->Device, Base->ADF: ";
		if (TangoService_getPoseAtTime(timestamp, d_to_adf_pair, &pose) != TANGO_SUCCESS)
			return false;
	}

	//latestColorPosition_ = glm::vec3(pose.translation[0], pose.translation[1], pose.translation[2]);
	//latestColorOrientation_ = glm::quat(pose.orientation[3], pose.orientation[0], pose.orientation[1], pose.orientation[2]);
	/*
	std::stringstream string_stream;
	string_stream.setf(std::ios_base::fixed, std::ios_base::floatfield);
	string_stream.precision(2);
	string_stream << "Tango system event: " << event_string << "\n" << frame_pair
		<< "\n"
		<< "  status: "
		<< getStatusStringFromStatusCode(pose.status_code)
		<< ", count: " << status_count[pose.status_code]
		<< ", colorTimestamp(ms): " << colorTimestamp << ", position(m): ["
		<< pose.translation[0] << ", " << pose.translation[1] << ", "
		<< pose.translation[2] << "]"
		<< ", orientation: [" << pose.orientation[0] << ", "
		<< pose.orientation[1] << ", " << pose.orientation[2] << ", "
		<< pose.orientation[3] << "]\n"
		<< "Color Camera Intrinsics(px):\n"
		<< "  width: " << cc_width << ", height: " << cc_height
		<< ", fx: " << cc_fx << ", fy: " << cc_fy;	
	pose_string = string_stream.str();
	*/
	return true;
}

//------------------------------------------------------------------------------
bool TangoData::GetIntrinsics() 
{
	// Retrieve the Intrinsic
	TangoCameraIntrinsics ccIntrinsics;
	
	if (TangoService_getCameraIntrinsics(TANGO_CAMERA_FISHEYE, &ccIntrinsics) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_getCameraIntrinsics(TANGO_CAMERA_FISHEYE): Failed");
		return false;
	}
	// Color camera's image plane width.
	fisheye.intrinsics.cc_width = ccIntrinsics.width;
	// Color camera's image plane height.
	fisheye.intrinsics.cc_height = ccIntrinsics.height;
	// Color camera's x axis focal length.
	fisheye.intrinsics.cc_fx = ccIntrinsics.fx;
	// Color camera's y axis focal length.
	fisheye.intrinsics.cc_fy = ccIntrinsics.fy;
	// Principal point x coordinate on the image.
	fisheye.intrinsics.cc_cx = ccIntrinsics.cx;
	// Principal point y coordinate on the image.
	fisheye.intrinsics.cc_cy = ccIntrinsics.cy;
	for (int i = 0; i < 5; i++) 
		fisheye.intrinsics.cc_distortion[i] = ccIntrinsics.distortion[i];

	fisheye.intrinsics.cc_fovx = 2.0 * atan(0.5 * ccIntrinsics.width / ccIntrinsics.fx);
	fisheye.intrinsics.cc_fovy = 2.0 * atan(0.5 * ccIntrinsics.height / ccIntrinsics.fy);
	fisheye.intrinsics.aspect = float(ccIntrinsics.width) / ccIntrinsics.height;

	CT2(fisheye.intrinsics.cc_fx, fisheye.intrinsics.cc_fy);
	CT2(fisheye.intrinsics.cc_cx, fisheye.intrinsics.cc_cy);
	CT3(fisheye.intrinsics.cc_width, fisheye.intrinsics.cc_height, fisheye.intrinsics.aspect);
	CT(fisheye.intrinsics.cc_distortion[0]);


	if (TangoService_getCameraIntrinsics(TANGO_CAMERA_COLOR, &ccIntrinsics) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_getCameraIntrinsics(TANGO_CAMERA_COLOR): Failed");
		return false;
	}

	// Color camera's image plane width.
	color.intrinsics.cc_width = ccIntrinsics.width;
	// Color camera's image plane height.
	color.intrinsics.cc_height = ccIntrinsics.height;
	// Color camera's x axis focal length.
	color.intrinsics.cc_fx = ccIntrinsics.fx;
	// Color camera's y axis focal length.
	color.intrinsics.cc_fy = ccIntrinsics.fy;
	// Principal point x coordinate on the image.
	color.intrinsics.cc_cx = ccIntrinsics.cx;
	// Principal point y coordinate on the image.
	color.intrinsics.cc_cy = ccIntrinsics.cy;
	for (int i = 0; i < 5; i++) 
		color.intrinsics.cc_distortion[i] = ccIntrinsics.distortion[i];

	color.intrinsics.cc_fovx = 2.0 * atan(0.5 * ccIntrinsics.width / ccIntrinsics.fx);
	color.intrinsics.cc_fovy = 2.0 * atan(0.5 * ccIntrinsics.height / ccIntrinsics.fy);
	color.intrinsics.aspect = float(ccIntrinsics.width) / ccIntrinsics.height;

	CT2(color.intrinsics.cc_fx, color.intrinsics.cc_fy);
	CT2(color.intrinsics.cc_cx, color.intrinsics.cc_cy);
	CT3(color.intrinsics.cc_width, color.intrinsics.cc_height, color.intrinsics.aspect);
	CT5(color.intrinsics.cc_distortion[0], color.intrinsics.cc_distortion[1], color.intrinsics.cc_distortion[2], color.intrinsics.cc_distortion[3], color.intrinsics.cc_distortion[4]);

	pointcloud.intrinsics = color.intrinsics;
	view.intrinsics = color.intrinsics;

	return true;
}

//------------------------------------------------------------------------------
void TangoData::connectTextures(GLuint colorTextureId, GLuint fisheyeTextureId) 
{
	if (color.isActive)
	{
		if (colorTextureId)
		{
			if (TangoService_connectTextureId(TANGO_CAMERA_COLOR, colorTextureId, this, onFrameAvailable) != TANGO_SUCCESS) 
			{
				LOGE("TangoService_connectTextureId(TANGO_CAMERA_COLOR): Failed!");
				color.isActive = false;
				exit(1);
			}
		}
		else
		{
			color.isActive = false;
		}
	}

	if (fisheye.isActive)
	{
		if (fisheyeTextureId)
		{
			if (TangoService_connectTextureId(TANGO_CAMERA_FISHEYE, fisheyeTextureId, this, onFrameAvailable) != TANGO_SUCCESS) 
			{
				LOGE("TangoService_connectTextureId(TANGO_CAMERA_FISHEYE): Failed!");
				fisheye.isActive = false;
				exit(1);
			}
		}
		else
		{
			fisheye.isActive = false;
		}
	}
}

//------------------------------------------------------------------------------
void TangoData::updateColorData() 
{
	if (color.isActive == false)
		return;

	ScopedMutex sm(color.mutex);

	double timestamp = 0;
	if (TangoService_updateTexture(TANGO_CAMERA_COLOR, &timestamp) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_updateTexture(TANGO_CAMERA_COLOR): Failed");
		return;
	}

	// HACK:
	// The timestamp returned will often create invalid poses in getPoseAtTime.
	// The workaround is to ensure the config option for low latency,
	// or approximate the pose by using the latest.

	glm::vec3 translation;
	glm::quat rotation;

	TangoPoseData pose;
	getPoseAtTime(timestamp, pose);

	if (pose.status_code == TANGO_POSE_VALID)
	{
		color.deltaTime = (timestamp - color.timestamp) * kSecondToMillisecond;
		color.timestamp = timestamp;

		translation = glm::vec3(pose.translation[0], pose.translation[1], pose.translation[2]);
		rotation = glm::quat(pose.orientation[3], pose.orientation[0], pose.orientation[1], pose.orientation[2]);

		// Update device with respect to  start of service frame transformation.
		// Note: this is the pose transformation for depth frame.
		color.deviceToRestMat = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);

		color.updateId++;
	}
	else
	{
		// extract the existing transformation from the matrix for status display.
		glm::vec3 scale;
		GlUtil::DecomposeMatrix(color.deviceToRestMat, translation, rotation, scale);
	}

	ScopedMutex sm2(pose_mutex);

	// Build pose logging string for debug display...
	std::stringstream string_stream;
	string_stream.setf(std::ios_base::fixed, std::ios_base::floatfield);
	string_stream.precision(3);
	string_stream.width(10);
	string_stream << "* color: " << "status: " << getStatusStringFromStatusCode(pose.status_code) << "\n"
				//<< ", count: " << pose_status_count
				<< "timestamp(ms): " << color.timestamp << ", delta time(ms): " << color.deltaTime
				<< ", position(m): [" << translation[0] << ", " << translation[1] << ", " << translation[2] << "]"
				<< ", quat: [" << rotation[0] << ", " << rotation[1] << ", " << rotation[2] << ", " << rotation[3] << "]";
	color.statusString = string_stream.str();
}

//------------------------------------------------------------------------------
void TangoData::updateFisheyeData() 
{
	if (fisheye.isActive == false)
		return;

	ScopedMutex sm(fisheye.mutex);

	double timestamp = 0;
	if (TangoService_updateTexture(TANGO_CAMERA_FISHEYE, &timestamp) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_updateTexture(TANGO_CAMERA_FISHEYE): Failed");
		return;
	}

	// BUG WORKAROUND:
	// for whatever reason, the timestamp returned will often create invalid poses in getPoseAtTime.
	// so we just use the latest by setting timestamp to zero.
	//timestamp = 0;

	glm::vec3 translation;
	glm::quat rotation;

	TangoPoseData pose;
	getPoseAtTime(timestamp, pose);

	if (pose.status_code == TANGO_POSE_VALID)
	{
		fisheye.deltaTime = (timestamp - fisheye.timestamp) * kSecondToMillisecond;
		fisheye.timestamp = timestamp;

		translation = glm::vec3(pose.translation[0], pose.translation[1], pose.translation[2]);
		rotation = glm::quat(pose.orientation[3], pose.orientation[0], pose.orientation[1], pose.orientation[2]);

		// Update device with respect to  start of service frame transformation.
		// Note: this is the pose transformation for frame.
		fisheye.deviceToRestMat = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);

		fisheye.updateId++;
	}
	else
	{
		// extract the existing transformation from the matrix for status display.
		glm::vec3 scale;
		GlUtil::DecomposeMatrix(fisheye.deviceToRestMat, translation, rotation, scale);
	}

	ScopedMutex sm2(pose_mutex);

	// Build pose logging string for debug display...
	std::stringstream string_stream;
	string_stream.setf(std::ios_base::fixed, std::ios_base::floatfield);
	string_stream.precision(3);
	string_stream.width(10);
	string_stream << "* fisheye: " << "status: " << getStatusStringFromStatusCode(pose.status_code) << "\n"
				//<< ", count: " << pose_status_count
				<< "timestamp(ms): " << fisheye.timestamp << ", delta time(ms): " << fisheye.deltaTime
				<< ", position(m): [" << translation[0] << ", " << translation[1] << ", " << translation[2] << "]"
				<< ", quat: [" << rotation[0] << ", " << rotation[1] << ", " << rotation[2] << ", " << rotation[3] << "]";
	fisheye.statusString = string_stream.str();
}

//------------------------------------------------------------------------------
// Reset the Motion Tracking.
void TangoData::ResetMotionTracking() 
{
	TangoService_resetMotionTracking();
}

//------------------------------------------------------------------------------
// Update pose data. This function will be called only when the pose
// data is changed (dirty). This function is being called through the
// GL rendering thread (See tango_pointcloud.cpp, RenderFrame()).
//
// This will off load some computation inside the onPoseAvailable()
// callback function. Heavy computation inside callback will block the whole
// Tango Service callback thread, so migrating heavy computation to other
// thread is suggested.
void TangoData::updateViewData() 
{
	if (view.isActive == false)
		return;

	if (view.isDirty == false)
		return;
	

	ScopedMutex sm(view.mutex);

	view.isDirty = false;
	view.updateId++;

	//if (cur_pose_data.status_code != TANGO_POSE_VALID)
	//	return;

	// Calculate status code count for debug display.
	if (prev_pose_data.status_code != cur_pose_data.status_code) 
		pose_status_count = 0;
	++pose_status_count;

	// Calculate frame delta time for debug display.
	// Note: this is the pose callback frame delta time.
	view.deltaTime = (cur_pose_data.timestamp - prev_pose_data.timestamp) * kSecondToMillisecond;
	view.timestamp = cur_pose_data.timestamp;

	// Store current pose data to previous.
	prev_pose_data = cur_pose_data;

	

	// Update device with respect to  start of service frame transformation.
	// Note: this is the pose transformation for pose frame.
	glm::vec3 translation = glm::vec3(prev_pose_data.translation[0], prev_pose_data.translation[1], prev_pose_data.translation[2]);
	glm::quat rotation = glm::quat(prev_pose_data.orientation[3], prev_pose_data.orientation[0], prev_pose_data.orientation[1], prev_pose_data.orientation[2]);
	view.deviceToRestMat = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);

	ScopedMutex sm2(pose_mutex);

	// Build pose logging string for debug display...
	std::stringstream string_stream;
	string_stream.setf(std::ios_base::fixed, std::ios_base::floatfield);
	string_stream.precision(3);
	string_stream.width(10);
	string_stream << "* view: " << "status: " << getStatusStringFromStatusCode(cur_pose_data.status_code) << "\n"
				<< ", count: " << pose_status_count
				<< "timestamp(ms): " << view.timestamp << ", delta time(ms): " << view.deltaTime
				<< ", position(m): [" << translation[0] << ", " << translation[1] << ", " << translation[2] << "]"
				<< ", quat: [" << rotation[0] << ", " << rotation[1] << ", " << rotation[2] << ", " << rotation[3] << "]";
	view.statusString = string_stream.str();
}

//------------------------------------------------------------------------------
// Update XYZij data. This function will be called only when the XYZ_ij
// data is changed (dirty). This function is being called through the
// GL rendering thread (See tango_pointcloud.cpp, RenderFrame()).
//
// This will off load some computation inside the onXYZ_ijAvailable()
// callback function. Heavy computation inside callback will block the whole
// Tango Service callback thread, so migrating heavy computation to other
// thread is suggested.
bool TangoData::updatePointcloudData() 
{
	if (pointcloud.isActive == false)
		return false;

	if (pointcloud.isDirty == false)
		return false;

	ScopedMutex sm(pointcloud.mutex);
	pointcloud.isDirty = false;

	TangoPoseData pose;
	getPoseAtTime(pointcloud.timestamp, pose);

	if (pose.status_code != TANGO_POSE_VALID)
		return false;

	pointcloud.updateId++;

	glm::vec3 translation = glm::vec3(pose.translation[0], pose.translation[1], pose.translation[2]);
	glm::quat rotation = glm::quat(pose.orientation[3], pose.orientation[0], pose.orientation[1], pose.orientation[2]);

	// Update device with respect to  start of service frame transformation.
	// Note: this is the pose transformation for pointcloud frame.
	pointcloud.deviceToRestMat = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);

	// Calculating average depth for debug display.
	float total_z = 0.0f;
	for (uint32_t i = 0; i < depth_buffer_size; ++i) 
	{
		// The memory layout is x,y,z,x,y,z. We are accumulating
		// all of the z value.
		total_z += depth_buffer[i * 3 + 2];
	}

	if (depth_buffer_size != 0) 
	{
		depth_average_length = total_z / static_cast<float>(depth_buffer_size);
	}

	ScopedMutex sm2(pose_mutex);

	// Build pose logging string for debug display...
	std::stringstream string_stream;
	string_stream.setf(std::ios_base::fixed, std::ios_base::floatfield);
	string_stream.precision(3);
	string_stream.width(10);
	string_stream << "* pointcloud: " << "status: " << getStatusStringFromStatusCode(pose.status_code) << "\n"
				//<< ", count: " << pose_status_count
				<< "timestamp(ms): " << pointcloud.timestamp << ", delta time(ms): " << pointcloud.deltaTime
				<< ", position(m): [" << translation[0] << ", " << translation[1] << ", " << translation[2] << "]"
				<< ", quat: [" << rotation[0] << ", " << rotation[1] << ", " << rotation[2] << ", " << rotation[3] << "]";
	pointcloud.statusString = string_stream.str();

	return true;
}

std::string TangoData::getStatusString() 
{
	ScopedMutex sm(pose_mutex);
	return event_string  + "\n\n" + view.statusString + "\n\n" + color.statusString + "\n\n" + fisheye.statusString + "\n\n" + pointcloud.statusString;
}
//------------------------------------------------------------------------------
// Get OpenGL camera with repect to OpenGL world frame transformation.
// Note: motion tracking pose and depth pose are different. Depth updates slower
// than pose update, we always want to use the closest pose to transform
// point cloud to local space to world space.
glm::mat4 TangoData::getOC2OWMat(TangoData::ViewPoseData& viewData) 
{
	ScopedMutex sm(viewData.mutex);
	return ss_2_ow_mat * viewData.deviceToRestMat * glm::inverse(d_2_imu_mat) * viewData.c_2_imu_mat * oc_2_c_mat;
}

//------------------------------------------------------------------------------
// Set up extrinsics transformations:
// 1. Device with respect to IMU transformation.
// 2. Color camera with respect to IMU transformation.
// Note: on Yellowstone devices, the color camera is the depth camera.
// so the 'c_2_imu_mat' could also be used for depth point cloud
// transformation.
bool TangoData::GetExtrinsics() 
{
	TangoPoseData pose_data;
	TangoCoordinateFramePair frame_pair;
	glm::vec3 d_to_imu_position;
	glm::quat d_to_imu_rotation;


	frame_pair.base = TANGO_COORDINATE_FRAME_IMU;
	frame_pair.target = TANGO_COORDINATE_FRAME_DEVICE;
	if (TangoService_getPoseAtTime(0.0, frame_pair, &pose_data) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_getPoseAtTime(): Failed");
		return false;
	}
	d_to_imu_position = glm::vec3(pose_data.translation[0], pose_data.translation[1], pose_data.translation[2]);
	d_to_imu_rotation = glm::quat(pose_data.orientation[3], pose_data.orientation[0], pose_data.orientation[1], pose_data.orientation[2]);
	d_2_imu_mat = glm::translate(glm::mat4(1.0f), d_to_imu_position) * glm::mat4_cast(d_to_imu_rotation);
	//view.c_2_imu_mat = glm::translate(glm::mat4(1.0f), d_to_imu_position) * glm::mat4_cast(d_to_imu_rotation) * oc_2_c_mat;
	
	frame_pair.base = TANGO_COORDINATE_FRAME_IMU;
	frame_pair.target = TANGO_COORDINATE_FRAME_CAMERA_COLOR;
	if (TangoService_getPoseAtTime(0.0, frame_pair, &pose_data) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_getPoseAtTime(TANGO_COORDINATE_FRAME_DISPLAY): Failed");
		return false;
	}
	d_to_imu_position = glm::vec3(pose_data.translation[0], pose_data.translation[1], pose_data.translation[2]);
	d_to_imu_rotation = glm::quat(pose_data.orientation[3], pose_data.orientation[0], pose_data.orientation[1], pose_data.orientation[2]);
	view.c_2_imu_mat = glm::translate(glm::mat4(1.0f), d_to_imu_position) * glm::mat4_cast(d_to_imu_rotation);
	

	frame_pair.base = TANGO_COORDINATE_FRAME_IMU;
	frame_pair.target = TANGO_COORDINATE_FRAME_CAMERA_COLOR;
	if (TangoService_getPoseAtTime(0.0, frame_pair, &pose_data) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_getPoseAtTime(TANGO_COORDINATE_FRAME_CAMERA_COLOR): Failed");
		return false;
	}
	d_to_imu_position = glm::vec3(pose_data.translation[0], pose_data.translation[1], pose_data.translation[2]);
	d_to_imu_rotation = glm::quat(pose_data.orientation[3], pose_data.orientation[0], pose_data.orientation[1], pose_data.orientation[2]);
	color.c_2_imu_mat = glm::translate(glm::mat4(1.0f), d_to_imu_position) * glm::mat4_cast(d_to_imu_rotation);


	frame_pair.base = TANGO_COORDINATE_FRAME_IMU;
	frame_pair.target = TANGO_COORDINATE_FRAME_CAMERA_FISHEYE;
	if (TangoService_getPoseAtTime(0.0, frame_pair, &pose_data) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_getPoseAtTime(TANGO_COORDINATE_FRAME_CAMERA_FISHEYE): Failed");
		return false;
	}
	d_to_imu_position = glm::vec3(pose_data.translation[0], pose_data.translation[1], pose_data.translation[2]);
	d_to_imu_rotation = glm::quat(pose_data.orientation[3], pose_data.orientation[0], pose_data.orientation[1], pose_data.orientation[2]);
	fisheye.c_2_imu_mat = glm::translate(glm::mat4(1.0f), d_to_imu_position) * glm::mat4_cast(d_to_imu_rotation);


	frame_pair.base = TANGO_COORDINATE_FRAME_IMU;
	frame_pair.target = TANGO_COORDINATE_FRAME_CAMERA_DEPTH;
	if (TangoService_getPoseAtTime(0.0, frame_pair, &pose_data) != TANGO_SUCCESS) 
	{
		LOGE("TangoService_getPoseAtTime(TANGO_COORDINATE_FRAME_CAMERA_COLOR): Failed");
		return false;
	}
	d_to_imu_position = glm::vec3(pose_data.translation[0], pose_data.translation[1], pose_data.translation[2]);
	d_to_imu_rotation = glm::quat(pose_data.orientation[3], pose_data.orientation[0], pose_data.orientation[1], pose_data.orientation[2]);
	pointcloud.c_2_imu_mat = glm::translate(glm::mat4(1.0f), d_to_imu_position) * glm::mat4_cast(d_to_imu_rotation);


	return true;
}

//------------------------------------------------------------------------------
// Clean up.
TangoData::~TangoData() 
{
	if (config_ != 0) 
		TangoConfig_free(config_);
	config_ = 0;

	delete[] depth_buffer;
}
