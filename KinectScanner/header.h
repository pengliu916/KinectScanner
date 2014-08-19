#define USING_KINECT 0 // Set to 0 will instead use prerecorded RGBD video as input
#define KINECT2 1
#define Z_OFFSET -1.0

#define SMALL_OFFSET 0.03 // 3 centimeter
#define SMALL_ROTATE 0.05 // <1 Degrees

//#define DEPTH_WIDTH 640
//#define DEPTH_HEIGHT 480
#define MAX_WEIGHT 20
//#define XSCALE 0.00175008408898

#define EPSILON 1e-8

#define SUB_TEXTUREWIDTH 600
#define SUB_TEXTUREHEIGHT 450

#define VOXEL_SIZE  0.0075
#define VOXEL_NUM_X 384
#define VOXEL_NUM_Y 384
#define VOXEL_NUM_Z 384

#define COMPILE_FLAG D3DCOMPILE_ENABLE_STRICTNESS

// New interface
#if KINECT2
// Depth Reso
#define D_W 512
#define D_H 424
// Depth Range
#define R_N 0.2
#define R_F 10.0
// Depth cam intrinsic
#define C_X 2.5681568188916287e+002
#define C_Y 2.0554866916495337e+002
#define F_X -3.6172484623525816e+002
#define F_Y -3.6121411187495357e+002
#else
// Depth Reso
#define D_W 640
#define D_H 480
// Depth Range
#define R_N 0.2
#define R_F 10.0
// Depth cam intrinsic
#define C_X 320
#define C_Y 240
#define F_X 571.428571
#define F_Y 571.428571
#endif

// Threas hold
#define ANGLE_THRES sin (20.f * 3.14159254f / 180.f)
#define DIST_THRES 0.1f