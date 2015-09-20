/* Class for camera calibration
 * Requires OpenCV 2.x (tested with 2.4.9)
 * author: Peng Liu <peng.liu916@gmail.com>
 * version: July 2014
 */

#include <ctime>
#include "Calibration.h"

// Utility functions
bool FindOrCreateDirectory( const wchar_t* pszPath )
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = ::FindFirstFile( pszPath, &fd );
	while( hFind != INVALID_HANDLE_VALUE ){
		if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			return true;
	}

	if( !::CreateDirectory( pszPath, NULL ) ){
		char szDir[MAX_PATH];
		sprintf_s( szDir, sizeof( szDir ), "Failed to create folder [%s]", pszPath );
		cout << szDir << endl;
		return false;
	}
	return true;
}

// Calibration class implementation
vector<vector<Mat>> Calibration::ImagesAcquisition(int iCamCount, function<bool(vector<Mat>&)> captureFunc)
{
#if CIRCLES_GRID
	Pattern caliBoard = ASYMMETRIC_CIRCLES_GRID;
#else
	Pattern caliBoard = CHESSBOARD;
#endif
	Size boardSize = Size(CHESSBOARD_CORNER_WIDTH,CHESSBOARD_CORNER_HEIGHT);

	// Subvector contains img list for one cam
	vector<vector<Mat>> camImgList(iCamCount);
	
	// For return failed vector
	vector<vector<Mat>> emptyVector;

	int frameCount = 0;
	// Receive new data from device?
	bool bSucceed = true;
	// Pattern is found in all img from one frame?
	bool bPatternFoundForAll = true;
	// For user input
	char userInput;
	
	// Vector for holding all images from one frame
	vector<Mat> syncImgs;

	// For introduce delay between each acquisition
	clock_t delayTimer_startTime = clock() / (float)CLOCKS_PER_SEC;
	clock_t curAcceptanceTime;
	
	while( 'q' != (userInput = waitKey(1))){
		// Clear syncImgs to receive new data 
		syncImgs.clear();
		// Reset bPatternFoundForAll to false
		bPatternFoundForAll = true;
		// Store sync shot imgs
		bSucceed = captureFunc( syncImgs );
		
		if( !bSucceed ) continue;

		if( iCamCount != syncImgs.size() ){
			cout << "[Error] Camera count didn't match receive img count" << endl;
			return emptyVector;
		}

		// Img copy for showing img with pattern
		vector<Mat> DisplayCopy(iCamCount);

		char windowName[200];

		// Iterate the imgs to find pattern
		for( int i = 0; i < syncImgs.size(); i++){
			// Receive the idx of found corners
			vector<Point2f> corners;

			if( syncImgs[i].channels() == 1 ) cvtColor( syncImgs[i], DisplayCopy[i], CV_GRAY2BGR );
			else syncImgs[i].copyTo(DisplayCopy[i]);

			bool found = FindPatternCorners( syncImgs[i], corners);
			drawChessboardCorners( DisplayCopy[i], boardSize, corners, found );

			// Resize the img for display
			float scale = RESIZED_IMG_HEIGHT / syncImgs[i].size().height;

			Mat cornerMat( corners );
			cornerMat *= scale;

			resize( DisplayCopy[i], DisplayCopy[i], Size(), scale, scale, scale < 1.0f ? INTER_AREA : INTER_LINEAR );

			// Show imgs
			sprintf_s(windowName,200,"Sensor%i",i);
			imshow(windowName,DisplayCopy[i]);

			bPatternFoundForAll = bPatternFoundForAll && found;
		}
		frameCount++;

		if(!bPatternFoundForAll){
			delayTimer_startTime = clock() / CLOCKS_PER_SEC;
			continue;
		}

		// if it's within 1 second after the previous acceptance, deny it
		curAcceptanceTime = clock() / CLOCKS_PER_SEC;
		if( curAcceptanceTime - delayTimer_startTime < 1.0) continue;

		delayTimer_startTime = clock() / CLOCKS_PER_SEC;


		for( int i = 0; i < syncImgs.size(); i++){
			cvtColor(DisplayCopy[i],DisplayCopy[i],CV_BGR2GRAY);
			DisplayCopy[i] = 255 - DisplayCopy[i];
			sprintf_s( windowName, 200, "Sensor%i", i );
			imshow( windowName, DisplayCopy[i] );
			
			camImgList[i].push_back(syncImgs[i]);
		}

		cout<<"Images at frame "<< frameCount <<" added"<<endl;
		waitKey(10);
	}

	return camImgList;
}

void Calibration::SaveImages( vector<vector<Mat>>& imgList, string folderName )
{
	vector<int> compression_params;
	compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
	compression_params.push_back(100);

	wstring wFolderName(folderName.begin(),folderName.end());
	wstringstream wPath;
	wPath<<"./"<<wFolderName;
	while(!FindOrCreateDirectory(wPath.str().c_str())){
		cout<<wPath.str().c_str()<<" exist, please type in new folder name"<<endl;
		cin>>folderName;
		wPath << "./" << wFolderName;
	}

	string ymlFileName = folderName + "/syncImgList.yml";
	FileStorage fs( ymlFileName, FileStorage::WRITE);
	fs<<"frames"<<"{";
	for(int i = 0; i < imgList.size(); i++){
		wstringstream wSubFolder;
		wSubFolder << "./" << wFolderName << "/sensor" << i;
		FindOrCreateDirectory(wSubFolder.str().c_str());
		stringstream subBuff;
		subBuff << "sensor" << i;
		fs << subBuff.str() << "[";
		for( int j = 0; j < imgList[i].size(); j++){
			stringstream buff;
			buff << "./" << folderName << "/" << "sensor" << i << "/" << "img_" << j << ".jpg";
			string filename = buff.str();
			imwrite( filename, imgList[i][j], compression_params );
			fs << filename;
		}
		fs << "]";
	}
	fs << "}";
	cout << "Image Saved." << endl;
}

vector<vector<Mat>> Calibration::LoadImages( string fileName )
{
	vector<vector<Mat>> imgList;
	FileStorage fs( fileName, FileStorage::READ );
	if( !fs.isOpened() ){
		cout << "[Error] Cannot open " << fileName << endl;
		return imgList;
	}
	FileNode sensors = fs.getFirstTopLevelNode();
	FileNodeIterator itSensor = sensors.begin(), itSensor_end = sensors.end();
	Mat	tempImg;
	for( int i = 0; itSensor != itSensor_end; ++itSensor, i++ ){
		vector<Mat> imgListForOneSensor;
		FileNodeIterator itImgName = ((FileNode)*itSensor).begin(), itImgName_end = ((FileNode)*itSensor).end();
		int imgCount = 0;
		for( ; itImgName != itImgName_end; ++itImgName ){
			tempImg = imread((string)*itImgName);
			cout << "Img: " << (string)*itImgName << " loaded" << endl;
			imgListForOneSensor.push_back( tempImg );
			imgCount++;
		}
		imgList.push_back( imgListForOneSensor );
		cout << imgCount << "images for sensor " << i << " loaded." <<endl;
	}

	int imgNum = imgList[0].size();
	for( int i = 1; i < imgList.size(); i++){
		if ( imgList[i].size() != imgNum ){
			cout << "[Error] The number of image for each sensor is not equal, return zero vector" << endl;
			imgList.resize(0);
			return imgList;
		}
	}
	cout << "Images for all sensors are successfully loaded." << endl;
	return imgList;
}

void Calibration::DisplayImages(vector<vector<Mat>>& imgList, bool showPattern)
{
	int sensorNum = imgList.size();
	vector<vector<Mat>::iterator> it;
	vector<vector<Mat>::iterator> it_end;
	for(int i = 0; i < imgList.size(); i++){
		it.push_back( imgList[i].begin() );
		it_end.push_back( imgList[i].end() );
	}
	bool keepShowing = true;
	imshow("sensnor0",Mat(320,240,CV_16UC1));
	while( keepShowing && 'q' != waitKey() ){
		keepShowing = false;
		for( int i = 0; i < sensorNum; i++ ){
			stringstream buff;
			buff << "sensor" << i;
			if( it[i] != it_end[i] ){
				if(!showPattern){
					imshow( buff.str(), (Mat)*it[i] );
				}else{
					Mat img = (Mat)*it[i];
					vector<Point2f> corners;
					bool found = FindPatternCorners(img, corners);
					drawChessboardCorners(img,Size(CHESSBOARD_CORNER_WIDTH,CHESSBOARD_CORNER_HEIGHT),corners,found);
					imshow(buff.str(),img);
				}
				keepShowing |= ( it[i] != it_end[i] );
				++it[i];
			}
		}
	}
	return;
}

double Calibration::MonoCalib( vector<Mat>& imgList, Mat& cameraMatrix, Mat& distCoeffs, vector<vector<Point2f>>& imgPoints, vector<Mat>& rvecs, vector<Mat>& tvecs )
{
#if CIRCLES_GRID
	Pattern patternType = ASYMMETRIC_CIRCLES_GRID;
#else
	Pattern patternType = CHESSBOARD;
#endif
	Size boardSize = Size(CHESSBOARD_CORNER_WIDTH, CHESSBOARD_CORNER_HEIGHT);
	// Get object point
	vector<vector<Point3f>> objectPoints(1);
	Size imgSize = imgList[0].size();
	CalcChessboardCorners(objectPoints[0]);
	objectPoints.resize(imgList.size(),objectPoints[0]);
	bool foundInAllImg = true;
	for( int i = 0; i < imgList.size(); i++ ){
		vector<Point2f> cornerBuff;
 		foundInAllImg = foundInAllImg && FindPatternCorners(imgList[i],cornerBuff);
		imgPoints.push_back(cornerBuff);
	}
	if ( !foundInAllImg ){
		cout << "[Error] Pattern are not found in one of the img list" << endl;
		return false;
	}
	rvecs.clear(); tvecs.clear();
	double rms = calibrateCamera( objectPoints, imgPoints, imgSize, cameraMatrix, distCoeffs, rvecs, tvecs, MONO_CALIB_FLAG|CV_CALIB_FIX_K4|CV_CALIB_FIX_K5 );

	bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs);
	//cout << "rms:" << rms << endl;
	return rms;
}

void Calibration::CalcChessboardCorners(vector<Point3f>& corners)
{
#if CIRCLES_GRID
	Pattern patternType = ASYMMETRIC_CIRCLES_GRID;
#else
	Pattern patternType = CHESSBOARD;
#endif
	Size boardSize = Size(CHESSBOARD_CORNER_WIDTH, CHESSBOARD_CORNER_HEIGHT);

	corners.resize( 0 );

	switch( patternType )
	{
	case CHESSBOARD:
		for( int i = 0; i < boardSize.height; i++ )
			for( int j = 0; j < boardSize.width; j++ )
				corners.push_back( Point3f( float( j*SQUARE_SIZE ),
				float( i*SQUARE_SIZE ), 0 ) );
		break;

	case ASYMMETRIC_CIRCLES_GRID:
		for( int i = 0; i < boardSize.height; i++ )
			for( int j = 0; j < boardSize.width; j++ )
				corners.push_back( Point3f( float( ( 2 * j + i % 2 )*SQUARE_SIZE ),
				float( i*SQUARE_SIZE ), 0 ) );
		break;

	default:
		cout << "[Error] Unknown pattern type" << endl;
	}
}

bool Calibration::FindPatternCorners(Mat& img, vector<Point2f>& corners)
{
#if CIRCLES_GRID
	Pattern patternType = ASYMMETRIC_CIRCLES_GRID;
#else
	Pattern patternType = CHESSBOARD;
#endif
	Size boardSize = Size(CHESSBOARD_CORNER_WIDTH, CHESSBOARD_CORNER_HEIGHT);
	// Resize the img for display
	float scale = RESIZED_IMG_HEIGHT / img.size().height;

	// Get gray img for pattern finder
	Mat ViewGray;

	// Scale and convert the img for pattern finder and imshow
	if( img.channels() == 3 ){
		cvtColor( img, ViewGray, CV_BGR2GRAY );
		resize( ViewGray, ViewGray, Size(), scale, scale, scale < 1.0f ? INTER_AREA : INTER_LINEAR );
	} else if( img.channels() == 1 ){
		resize( img, ViewGray, Size(), scale, scale, scale < 1.0f ? INTER_AREA : INTER_LINEAR );
	} else {
		cout << "[Error] Unsupported multiChannel img" << endl;
	}

	bool found;
	switch( patternType )
	{
	case Calibration::CHESSBOARD:
		found = findChessboardCorners( ViewGray, boardSize, corners, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE );
		break;
	case Calibration::ASYMMETRIC_CIRCLES_GRID:
 		found = findCirclesGrid( ViewGray, boardSize, corners, CALIB_CB_ASYMMETRIC_GRID );
		break;
	default:
		cout << "[Error] Unknown pattern type" << endl;
	}
	// Improve the found corners' coordinate accuracy
	if( patternType == Calibration::CHESSBOARD && found )
		cornerSubPix( ViewGray, corners, Size( 11, 11 ), Size( -1, -1 ), TermCriteria( CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1 ) );

	Mat cornerMat( corners );
	cornerMat *= 1.f / scale;

	return found;
}

void Calibration::SaveCameraParams( const string& filename,
                       Size imageSize,
                       float aspectRatio, int flags,
                       const Mat& cameraMatrix, const Mat& distCoeffs,
                       double totalAvgErr )
{
	Size boardSize = Size(CHESSBOARD_CORNER_WIDTH, CHESSBOARD_CORNER_HEIGHT);
    FileStorage fs( filename, FileStorage::WRITE );

    time_t tt;
    time( &tt );
    struct tm *t2 = localtime( &tt );
    char buf[1024];
    strftime( buf, sizeof(buf)-1, "%c", t2 );

    fs << "calibration_time" << buf;

    fs << "image_width" << imageSize.width;
    fs << "image_height" << imageSize.height;
    fs << "board_width" << boardSize.width;
    fs << "board_height" << boardSize.height;
    fs << "square_size" << SQUARE_SIZE;

    if( flags & CV_CALIB_FIX_ASPECT_RATIO )
        fs << "aspectRatio" << aspectRatio;

    if( flags != 0 )
    {
        sprintf( buf, "flags: %s%s%s%s",
            flags & CV_CALIB_USE_INTRINSIC_GUESS ? "+use_intrinsic_guess" : "",
            flags & CV_CALIB_FIX_ASPECT_RATIO ? "+fix_aspectRatio" : "",
            flags & CV_CALIB_FIX_PRINCIPAL_POINT ? "+fix_principal_point" : "",
            flags & CV_CALIB_ZERO_TANGENT_DIST ? "+zero_tangent_dist" : "" );
        cvWriteComment( *fs, buf, 0 );
    }

    fs << "flags" << flags;

    fs << "camera_matrix" << cameraMatrix;
    fs << "distortion_coefficients" << distCoeffs;

    fs << "avg_reprojection_error" << totalAvgErr;
}


void Calibration::BundleCalibration( string outputFileName, vector<vector<Mat>>& imgList)
{
#if CIRCLES_GRID
	Pattern patternType = ASYMMETRIC_CIRCLES_GRID;
#else
	Pattern patternType = CHESSBOARD;
#endif
	Size boardSize = Size(CHESSBOARD_CORNER_WIDTH, CHESSBOARD_CORNER_HEIGHT);
	// Write parameter into file
	FileStorage fs( outputFileName, FileStorage::WRITE );

	// Write the time of calibration
	time_t tt;
	time( &tt );
	struct tm *t2 = localtime( &tt );
	char buf[200];
	strftime( buf, sizeof( buf ) - 1, "%c", t2 );
	fs << "calibration_time" << buf;

	// Write info for calibration board
	string patternName;
	switch( patternType ){
	case CHESSBOARD:
		patternName = "CHESSBOARD"; break;
	case ASYMMETRIC_CIRCLES_GRID:
		patternName = "ASYMMETRIC_CIRCLES_GRID"; break;
	default:
		patternName = "Unknown";break;
	}

	fs << "calibration_pattern" << patternName;
	fs << "board_width" << boardSize.width;
	fs << "board_height" << boardSize.height;
	fs << "square_size" << SQUARE_SIZE;

	// Write info for calibration flag
	if( MONO_CALIB_FLAG != 0 ){
		sprintf( buf, "monoCalib_flags: %s%s%s%s",
				 MONO_CALIB_FLAG & CV_CALIB_USE_INTRINSIC_GUESS ? "+use_intrinsic_guess" : "",
				 MONO_CALIB_FLAG & CV_CALIB_FIX_ASPECT_RATIO ? "+fix_aspectRatio" : "",
				 MONO_CALIB_FLAG & CV_CALIB_FIX_PRINCIPAL_POINT ? "+fix_principal_point" : "",
				 MONO_CALIB_FLAG & CV_CALIB_ZERO_TANGENT_DIST ? "+zero_tangent_dist" : "" );
		cvWriteComment( *fs, buf, 0 );
	}
	fs << "monoCalib_flags" << MONO_CALIB_FLAG;

	if( STEREO_CALIB_FLAG != 0 ){
		sprintf( buf, "stereoCalib_flags: %s%s%s%s",
				 STEREO_CALIB_FLAG & CV_CALIB_USE_INTRINSIC_GUESS ? "+use_intrinsic_guess" : "",
				 STEREO_CALIB_FLAG & CV_CALIB_FIX_ASPECT_RATIO ? "+fix_aspectRatio" : "",
				 STEREO_CALIB_FLAG & CV_CALIB_FIX_PRINCIPAL_POINT ? "+fix_principal_point" : "",
				 STEREO_CALIB_FLAG & CV_CALIB_ZERO_TANGENT_DIST ? "+zero_tangent_dist" : "" );
		cvWriteComment( *fs, buf, 0 );
	}
	fs << "stereoCalib_flags" << STEREO_CALIB_FLAG;

	// Write calibration result for sensors
	fs << "sensor_system" << "{";

	int iSensorCount = imgList.size();
	vector<Size> imgSize( iSensorCount );
	vector<Mat> cameraMatrix( iSensorCount );
	vector<Mat> distCoeffs( iSensorCount );
	vector<double> mono_rms( iSensorCount );
	vector<double> stereo_rms( iSensorCount );
	vector<double> epipolar_err( iSensorCount );

	// Relation between slave and master sensor
	vector<Mat> R(iSensorCount);	// 3x3 rotation matrix 
	vector<Mat> T(iSensorCount);	// 3d translation vector
	vector<Mat> E(iSensorCount);	// Essential matrix
	vector<Mat> F(iSensorCount);	// Fundamental matrix
	
	// Temporary image points vector for master sensor and slave sensor 
	vector<vector<Point2f>> imgPoints[2];
	// Get object point
	vector<vector<Point3f>> objectPoints( 1 );
	CalcChessboardCorners(objectPoints[0]);
	objectPoints.resize( imgList[0].size(), objectPoints[0] );

	cout << "Individual sensor calibration..." << endl;
	// MonoCalibration for master sensor
	imgSize[0] = imgList[0][0].size();
	mono_rms[0] = MonoCalib( imgList[0],cameraMatrix[0], distCoeffs[0],imgPoints[0] );
	cout << "Calibration on sensor " << 0 << " is done with rms: " << mono_rms[0] << " @" << imgSize[0].width << "x" << imgSize[0].height << endl;

	// Write the calibration result for master sensor
	cvWriteComment( *fs, "Master sensor ", 0 );
	fs << "Sensor_0" << "{";
	fs << "img_width" << imgSize[0].width;
	fs << "img_height" << imgSize[0].height;
	fs << "camera_matrix" << cameraMatrix[0];
	fs << "distortion_coefficients" << distCoeffs[0];
	fs << "ave_reprojection_err" << mono_rms[0];
	fs << "}";

	// Mono and stereo Calibration for slave sensors
	for( int iSensorIdx = 1; iSensorIdx < iSensorCount; iSensorIdx++){
		imgSize[iSensorIdx] = imgList[iSensorIdx][0].size();
		imgPoints[1].resize(0);
		mono_rms[iSensorIdx] = MonoCalib( imgList[iSensorIdx], cameraMatrix[iSensorIdx], distCoeffs[iSensorIdx], imgPoints[1] );
		cout << "Calibration on sensor " << iSensorIdx << " is done with rms: " << mono_rms[iSensorIdx] << " @" << imgSize[iSensorIdx].width << "x" << imgSize[iSensorIdx].height << endl;

		// Begin stereo calibration between master and slave camera
		stereo_rms[iSensorIdx] = stereoCalibrate( objectPoints, imgPoints[0], imgPoints[1], 
										   cameraMatrix[0], distCoeffs[0],
										   cameraMatrix[iSensorIdx], distCoeffs[iSensorIdx],
										   imgSize[iSensorIdx], R[iSensorIdx], T[iSensorIdx],
										   E[iSensorIdx], F[iSensorIdx],
										   STEREO_CALIB_FLAG, 
										   TermCriteria( CV_TERMCRIT_ITER + CV_TERMCRIT_EPS, 100, 1e-6 ));
		cout << " StereoCalibration done with rms: " << stereo_rms[iSensorIdx];
		vector<Mat> cMat, dMat;
		cMat.push_back( cameraMatrix[0] );
		cMat.push_back( cameraMatrix[iSensorIdx] );
		dMat.push_back( distCoeffs[0] );
		dMat.push_back( distCoeffs[iSensorIdx] );
		epipolar_err[iSensorIdx] = EpipolarConstrainError(imgPoints,cMat,dMat,F[iSensorIdx]);
		cout << " with epipolar constrain error: " << epipolar_err[iSensorIdx] << endl;

		// Write calibration result for slave sensor
		cvWriteComment( *fs, "Slave sensor ", 0 );
		char buffer[200];
		sprintf( buffer,"sensor_%i", iSensorIdx );
		fs << buffer << "{";
		fs << "img_width" << imgSize[iSensorIdx].width;
		fs << "img_height" << imgSize[iSensorIdx].height;
		fs << "camera_matrix" << cameraMatrix[iSensorIdx];
		fs << "distortion_coefficients" << distCoeffs[iSensorIdx];
		fs << "ave_reprojection_err" << mono_rms[iSensorIdx];
		fs << "rotation_matrix" << R[iSensorIdx];
		fs << "translation_vector" << T[iSensorIdx];
		fs << "essential_matrix" << E[iSensorIdx];
		fs << "fundamental_matrix" << F[iSensorIdx];
		fs << "stereo_reprojection_err" << stereo_rms[iSensorIdx];
		fs << "epipolar_constrain_err" << epipolar_err[iSensorIdx];
		fs << "}";
	}
	fs << "}";
	fs.release();
}

double Calibration::EpipolarConstrainError( vector<vector<Point2f>> org_imagePoints[], vector<Mat>& cameraMatrix, vector<Mat>& distCoeffs, Mat& F){
	// Since this function will modify the input (undistort the imagePoints)
	// So provide an simple copy to avoid modification on original data
	vector<vector<Point2f>> imagePoints[2];
	imagePoints[0] = org_imagePoints[0];
	imagePoints[1] = org_imagePoints[1];
	double err = 0;
	int npoints = 0;
	vector<Vec3f> lines[2];
	for( int i = 0; i < imagePoints[0].size(); i++ )
	{
		int npt = ( int )imagePoints[0][i].size();
		Mat imgpt[2];
		for( int k = 0; k < 2; k++ )
		{
			imgpt[k] = Mat( imagePoints[k][i] );
			undistortPoints( imgpt[k], imgpt[k], cameraMatrix[k], distCoeffs[k], Mat(), cameraMatrix[k] );
			computeCorrespondEpilines( imgpt[k], k + 1, F, lines[k] );
		}
		for( int j = 0; j < npt; j++ )
		{
			double errij = fabs( imagePoints[0][i][j].x*lines[1][j][0] +
								 imagePoints[0][i][j].y*lines[1][j][1] + lines[1][j][2] ) +
								 fabs( imagePoints[1][i][j].x*lines[0][j][0] +
								 imagePoints[1][i][j].y*lines[0][j][1] + lines[0][j][2] );
			err += errij;
		}
		npoints += npt;
	}
	return err/npoints;
}