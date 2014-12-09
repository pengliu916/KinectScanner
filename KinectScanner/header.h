#define USING_KINECT 1				// Set to 0 will instead use prerecorded RGBD video as input
#define KINECT2 1					// Set to 0 will use Kinect1 params instead of Kinect2
#define PASSIVE_STREAM 1			// Set to 1 will guanrrante every frame get processed
#define SKIP_FRAME_NUM 1			// Number of first frames to skip due to bad Kinect Startup bug
#define STEP_MODE 0					// Set to 1 the iteration will be triggered by pressing 'i'
#define X_OFFSET -0.0f
#define Y_OFFSET -0.0f
#define Z_OFFSET -0.9f

#define SMALL_OFFSET 0.03f // 3 centimeter
#define SMALL_ROTATE 0.05f// <1 Degrees

//#define DEPTH_WIDTH 640
//#define DEPTH_HEIGHT 480
#define MAX_WEIGHT 50
//#define XSCALE 0.00175008408898

#define EPSILON 1e-8f

#define SUB_TEXTUREWIDTH 600
#define SUB_TEXTUREHEIGHT 450

#define THREAD_X 8
#define THREAD_Y 8
#define THREAD_Z 8
#define THREAD2D_X 8
#define THREAD2D_Y 8
#define THREAD1D 128

#define VOXEL_SIZE  0.007f
#define VOXEL_NUM_X 384
#define VOXEL_NUM_Y 384
#define VOXEL_NUM_Z 384
#define TRUNC_DIST 0.021 //should be at least VOXEL_SIZE
#define INVALID_VALUE (-1.f)

#define CELLRATIO 8 // for fast raycasting, the ratio of voxel:cell in one dimension

#define COMPILE_FLAG D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_PREFER_FLOW_CONTROL

// New interface
#if KINECT2
// Depth Reso
#define D_W 512
#define D_H 424
// Depth Range
#define R_N 0.2f
#define R_F 10.0f
// Depth cam intrinsic
#define C_X 2.5681568188916287e+002f
#define C_Y 2.0554866916495337e+002f
#define F_X -3.6172484623525816e+002f
#define F_Y -3.6121411187495357e+002f
#else
// Depth Reso
#define D_W 640
#define D_H 480
// Depth Range
#define R_N 0.2f
#define R_F 10.0f
// Depth cam intrinsic
#define C_X 320
#define C_Y 240
#define F_X 571.428571f
#define F_Y 571.428571f
#endif

// Threas hold
#define ANGLE_THRES sin (20.f * 3.14159254f / 180.f)
#define DIST_THRES 0.1f



#define DEBUG 1