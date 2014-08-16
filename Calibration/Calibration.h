/* Class for camera calibration
 * Requires OpenCV 2.x (tested with 2.4.9)
 * author: Peng Liu <peng.liu916@gmail.com>
 * version: July 2014
 */

#pragma once
#include <functional>
#include <vector>
#include <iostream>
#include <windows.h>
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#define CIRCLES_GRID 1

const int MONO_CALIB_FLAG = CV_CALIB_FIX_K4 | CV_CALIB_FIX_K5 | CV_CALIB_FIX_K6;
const int STEREO_CALIB_FLAG = CV_CALIB_FIX_INTRINSIC | CV_CALIB_FIX_K4 | CV_CALIB_FIX_K5 | CV_CALIB_FIX_K6;

#if CIRCLES_GRID
const float SQUARE_SIZE = 0.0245f;
const int CHESSBOARD_CORNER_WIDTH = 4;
const int CHESSBOARD_CORNER_HEIGHT = 11;
#else
const float SQUARE_SIZE = 0.0265f;
const int CHESSBOARD_CORNER_WIDTH = 6;
const int CHESSBOARD_CORNER_HEIGHT = 9;
#endif
const int SATISFACTORY_IMG_COUNT = 50;
const float RESIZED_IMG_HEIGHT = 600.f;

using namespace cv;
using namespace std;

class Calibration
{
public:
	enum Pattern { CHESSBOARD, ASYMMETRIC_CIRCLES_GRID };

	// For stereo capturing, this function requires a func 'captureFunc' to ask
	// multiple cam devices to return images at the same time, and return a vector of
	// vector of imgs
	static vector<vector<Mat>> ImagesAcquisition(int iCamCount, function<bool(vector<Mat>&)> captureFunc);

	// For saving configurations and imgs, imgs for each sensor is saved in seperate subfolder. 
	// Paired imgs have their name ended with same index but different sensor prefix
	static void SaveImages( vector<vector<Mat>>& imgList, string folderName);

	// For loading configurations and img pairs
	static vector<vector<Mat>> LoadImages( string fileName );

	// For display imgs
	static void DisplayImages( vector<vector<Mat>>& imgList, bool findPattern = true);

	// Calibrate one sensor, and return the reprojection error
	static double MonoCalib( vector<Mat>& imgList, Mat& cameraMatrix, Mat& distCoeffs, vector<vector<Point2f>>& imgPoints = vector<vector<Point2f>>(), vector<Mat>& rvecs = vector<Mat>(), vector<Mat>& tvecs = vector<Mat>()); 

	// Compute the calibration board corners
	static void CalcChessboardCorners(vector<Point3f>& corners );

	// Convert img to an appropriated size and format and find pattern
	static bool FindPatternCorners( Mat& img, vector<Point2f>& corners);

	// Save calibration result to file
	static void SaveCameraParams( const string& filename,Size imageSize,float aspectRatio, int flags,const Mat& cameraMatrix, const Mat& distCoeffs,double totalAvgErr );

	// Calibrating multi-camera system
	// StereoCalibration will be running on each sensor along with the master sensor(sensor0)
	static void BundleCalibration(  string outputFileName, vector<vector<Mat>>& imgList);

	// Use epipolar constrain to check the quality of stereo calibration
	static double EpipolarConstrainError( vector<vector<Point2f>> imagePoints[], vector<Mat>& cameraMatrix, vector<Mat>& distCoeffs, Mat& F);
};