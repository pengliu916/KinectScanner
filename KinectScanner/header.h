#define USING_KINECT 1 // Set to 0 will instead use prerecorded RGBD video as input
#define Z_OFFSET -1.0

#define SMALL_OFFSET 0.03 // 3 centimeter
#define SMALL_ROTATE 0.05 // <1 Degrees

#define DEPTH_WIDTH 640
#define DEPTH_HEIGHT 480
#define MAX_WEIGHT 20
#define XSCALE 0.00175008408898

#define EPSILON 1e-8

#define SUB_TEXTUREWIDTH 600
#define SUB_TEXTUREHEIGHT 450

#define VOXEL_SIZE  0.0075
#define VOXEL_NUM_X 384
#define VOXEL_NUM_Y 384
#define VOXEL_NUM_Z 384

#define COMPILE_FLAG D3DCOMPILE_ENABLE_STRICTNESS