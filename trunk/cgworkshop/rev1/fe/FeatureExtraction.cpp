#include "FeatureExtraction.h"
#include "cvgabor.h"
#include <math.h>

// TODO:
// Add the destructor.
// Make sure everything everywhere is cleaned up nicely.

inline double round2( double d )
{
	return floor( d + 0.5 );
}


static void displayImage(char * title, IplImage * pImg)
{
	cvNamedWindow(title, 1);
	IplImage* pNewImg = cvCreateImage(cvSize(pImg->width,pImg->height),IPL_DEPTH_8U,1);
	cvConvertScale(pImg,pNewImg,1,0); 
	cvShowImage(title,pImg);
	cvWaitKey(0);
	cvDestroyWindow(title);		
	
	cvReleaseImage(&pNewImg);
}

static void saveChannel(char * title, CvMat * pMat)
{
	printf("\nsaveChannel in\n");
	CvMat * p8UMat = cvCreateMat( pMat->rows,pMat->cols, CV_8U );
	
	// Convert to unsigned char, 0..255
	cvConvertScale(pMat,p8UMat,255,0); 

	// Attach our data to an image header
	IplImage tImg;
	cvInitImageHeader(&tImg,cvSize(pMat->rows,pMat->cols),IPL_DEPTH_8U,1);
	tImg.imageData = (char*) p8UMat->data.ptr;

	// TODO: Remove this, only a test
	displayImage(title, &tImg);
		
	printf("saveChannel: Saving pchannel to: %s\n", title);
	if (!cvSaveImage(title,&tImg)) 
		printf("saveChannel: Could not save: %s\n",title);	
		
	cvReleaseMat(&p8UMat);
	
	printf("saveChannel out\n");
}

//////////////////////////////////////////////////////////////////////////////////////

// TODO: Make sure working......
// Image seems alright, but not perfect
void CFeatureExtraction::CalcHistogram(IplImage * pImg, CvMat * pHistogram)
{
	printf("\nCFeatureExtraction::CalcHistogram in\n");	
	int nBins = 10;
	
	int step = pImg->widthStep;
	int channels = m_nChannels;
	int w = m_nWidth;
	int h = m_nHeight;
	uchar * pData  = (uchar *)pImg->imageData;
	
	for (int y=0; y<h; y++)
	{
		for (int x=0;x<w; x++)
		{
			for (int k=0;k<channels;k++)
			{
				// Get appropriate bin for current pixel
				uchar val = pData[y*step+x*channels+k];;
				uchar bin = val*nBins/256;
	
				// Go over a 5x5 patch, increase appropriate bin by 1
				for (int j=y-2;j<=y+2;j++)
				{
					
					int rj = j;
					//if (j<0 || j>=h)
	                //	continue;
	                // Symmetric mirroring, like matlab
	                if (rj<0)
	                	rj = -rj;
	                if (rj >= h)
	                	rj = 2*h-rj-1;
	                

	                for (int i=x-2;i<=x+2;i++)
	                {
	                	int ri = i;
	                   // if (i<0 || i>=w)
	                    //        continue;
		                // Symmetric mirroring, like matlab
		                if (ri<0)
		                	ri = -ri;
		                if (ri >= w)
		                	ri = 2*w-ri-1;

						int row = rj*w+ri;
						pHistogram->data.fl[row*pHistogram->cols +k*nBins+bin]+=1;

						//int row = j*w+x;
						//pHistogram->data.fl[j*step*10+x*channels*10 +k*nBins+bin]+=1;
	                }
				}
			}
		}
	}
	
	printf("CFeatureExtraction::CalcHistogram out\n");	
}

//////////////////////////////////////////////////////////////////////////////////////

bool CFeatureExtraction::GetGaborResponse(IplImage *pGrayImg, IplImage *pResImg, float orientation, float freq, float sx, float sy)
{
	// Create the filter
	CvGabor *pGabor = new CvGabor(orientation, freq, sx, sy);
	
	//pGabor->show(CV_GABOR_REAL);
	
	// Convolution
	pGabor->Apply(pGrayImg, pResImg, CV_GABOR_MAG);
	
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////

bool CFeatureExtraction::GetGaborResponse(CvMat * pGaborMat)
{
	float* pMatPos;
	int idx = 0;
	printf("\nCFeatureExtraction::GetGaborResponse in\n");	
	// Convert our image to grayscale (Gabor doesn't care about colors! I hope?)	
	IplImage *pGrayImg = cvCreateImage(cvSize(m_pSrcImg->width,m_pSrcImg->height), IPL_DEPTH_8U, 1);
	cvCvtColor(m_pSrcImg,pGrayImg,CV_BGR2GRAY);

	// The output image
	IplImage *reimg = cvCreateImage(cvSize(pGrayImg->width,pGrayImg->height), IPL_DEPTH_8U, 1);

	double freq = 0.4;
	int freq_steps = 4;
	int ori_count = 6;
	double ori_space = PI/ori_count;
	
	int i,j;
	for (i=0;i<freq_steps;i++)	
	{
    	double bw = (2 * freq) / 3;
    	double sx = round2(0.5 / PI / pow(bw,2));
    	double sy = round2(0.5 * log(2.0) / pow(PI,2.0) / pow(freq,2.0) / (pow(tan(ori_space / 2),2.0)));	
    		
		for (j=0;j<ori_count;j++)
		{	
			double ori = j*ori_space;
			GetGaborResponse(pGrayImg, reimg, ori, freq, sx, sy);
			
			// This being a test and all, display the image
			// displayImage(title, reimg);
			char filename[255];
			sprintf(filename, "gabor%02d.bmp", idx+1);
			cvSaveImage(filename,reimg);
			
			// Concat the new vector to the result matrix
			int k;
			pMatPos = (float *) pGaborMat->data.fl;
			char * pResData = (char *) reimg->imageData;
			for (k=0;k<reimg->width*reimg->height;k++)
			{
				pMatPos[idx] = (float) pResData[0];
				pMatPos += 24;
				pResData++;
							
			}

			idx++;
		}
		freq /= 2;	
	}
	// Release
	cvReleaseImage(&reimg);
	cvReleaseImage(&pGrayImg);
	printf("CFeatureExtraction::GetGaborResponse out\n");	
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////

// TODO: Would fail if m_nChannels != 3
// RGB to LAB
bool CFeatureExtraction::GetColorChannels(CvMat * pColorChannels[])
{
	printf("\nCFeatureExtraction::GetColorChannels in\n");
	int nSize = 3;
	
	// Convert to LAB color space
	IplImage *pLabImg = cvCreateImage(cvSize(m_pSrcImg->width,m_pSrcImg->height), IPL_DEPTH_32F, nSize);
	cvCvtColor(m_pSrcImgFloat,pLabImg,CV_BGR2Lab);	

	// Put the 32F lab image data in a matrix header
	CvMat srcMat;
	cvInitMatHeader(&srcMat, m_nWidth*m_nHeight, nSize , CV_32F, (float*)pLabImg->imageData );

	// This matrix would hold the values represented in the new basis we've found
	CvMat * pResultMat = cvCreateMat( m_nWidth*m_nHeight, nSize , CV_32F );
	// Actual calculation
	DoPCA(&srcMat, pResultMat, nSize, 3);
	// Extracting the 3 primary channels
	GetChannels(pResultMat, pColorChannels, nSize, 3);
	
	// Useful releasing

	cvReleaseMat(&pResultMat);
	
	printf("CFeatureExtraction::GetColorChannels out\n");
	return true;	
}

//////////////////////////////////////////////////////////////////////////////////////

bool CFeatureExtraction::GetTextureChannels(CvMat * pTextureChannels[])
{
	int i;
	printf("\nCFeatureExtraction::GetTextureChannels in\n");
	int gaborSize = 24;
	int histSize = 30;
	int vectorSize = gaborSize+histSize;
	
	// Calc the full histogram vectors
	CvMat * pHistMat = cvCreateMat( m_nWidth*m_nHeight, histSize , CV_32F );
	CalcHistogram(m_pSrcImg, pHistMat);
	// Do we need to normalize?
	cvNormalize(pHistMat, pHistMat, 0, 1, CV_MINMAX);

	CvMat * pGaborMat = cvCreateMat (m_nWidth * m_nHeight, gaborSize, CV_32F);
	GetGaborResponse(pGaborMat);
	// Do we need to normalize?
	cvNormalize(pGaborMat, pGaborMat, 0, 1, CV_MINMAX);

	// Our merged matrix
	CvMat * pTextureMat = cvCreateMat( m_nWidth*m_nHeight, vectorSize , CV_32F );

	// Go over row by row, concat the gabor and histogram matrices
	float * pMatData = (float *) pTextureMat->data.fl;
	float * pHistData = (float *)  pHistMat->data.fl;
	float * pGaborData = (float *)  pGaborMat->data.fl;
	
	int gaborStep = pGaborMat->step;
	int histStep = pHistMat->step;
	
	for (i=0;i<m_nWidth * m_nHeight;i++)
	{
		
		memcpy(pMatData, pGaborData, gaborStep);
		pMatData+=gaborSize;
		pGaborData+=gaborSize;		
		
		memcpy(pMatData, pHistData, histStep);
		pMatData+=histSize;
		pHistData+=histSize;
	}
	

	// This matrix would hold the values represented in the new basis we've found
	CvMat * pResultMat = cvCreateMat( m_nWidth*m_nHeight, 3 , CV_32F );
	// Actual calculation
	DoPCA(pTextureMat, pResultMat, vectorSize, 3);
	// Extracting the 3 primary channels
	GetChannels(pResultMat, pTextureChannels, 3, 3);

	cvReleaseMat(&pHistMat);
	cvReleaseMat(&pGaborMat);
	cvReleaseMat(&pTextureMat);
	cvReleaseMat(&pResultMat);
	
	printf("CFeatureExtraction::GetTextureChannels out\n");

	return true;
}


//////////////////////////////////////////////////////////////////////////////////////

bool CFeatureExtraction::DoPCA(CvMat * pMat, CvMat * pResultMat, int nSize, int nExpectedSize)
{
	printf("\nCFeatureExtraction::DoPCA in\n");
	int i;	

	printf("DoPCA: Sort our data sets in a vector each\n");	
	// Sort our data sets in a vector each
	CvMat ** pDataSet = new CvMat*[m_nWidth*m_nHeight];
	float * pData = pMat->data.fl;
	for (i=0;i<m_nWidth*m_nHeight;i++)
	{
		pDataSet[i] = (CvMat*) malloc(sizeof(CvMat));
		cvInitMatHeader(pDataSet[i], 1, nSize, CV_32FC1, &pData[i*nSize]);
	}
	
	printf("DoPCA: Calc covariance matrix\n");
	// Calc covariance matrix
	CvMat* pCovMat = cvCreateMat( nSize, nSize, CV_32F );
	CvMat* pMeanVec = cvCreateMat( 1, nSize, CV_32F );
	
	cvCalcCovarMatrix( (const void **)pDataSet, m_nWidth*m_nHeight, pCovMat, pMeanVec, CV_COVAR_SCALE | CV_COVAR_NORMAL );
	
	cvReleaseMat(&pMeanVec);
	
	printf("DoPCA: Do the SVD decomposition\n");
	// Do the SVD decomposition
	CvMat* pMatW = cvCreateMat( nSize, 1, CV_32F );
	CvMat* pMatV = cvCreateMat( nSize, nSize, CV_32F );
	CvMat* pMatU = cvCreateMat( nSize, nSize, CV_32F );
	
	cvSVD(pCovMat, pMatW, pMatU, pMatV, CV_SVD_MODIFY_A+CV_SVD_V_T);
	
	cvReleaseMat(&pCovMat);
	cvReleaseMat(&pMatW);
	cvReleaseMat(&pMatV);

	printf("DoPCA: Extract the requested number of dominant eigen vectors\n");
	// Extract the requested number of dominant eigen vectors
	CvMat* pEigenVecs = cvCreateMat( nSize, nExpectedSize, CV_32F );
	for (i=0;i<nSize;i++)
		memcpy(&pEigenVecs->data.fl[i*nExpectedSize], &pMatU->data.fl[i*nSize], nExpectedSize*sizeof(float));

	printf("DoPCA: Transform to the new basis\n");
	// Transform to the new basis	
	cvMatMul(pMat, pEigenVecs, pResultMat);
	cvReleaseMat(&pMatU);
	cvReleaseMat(&pEigenVecs);
	
	for (i = 0; i < m_nHeight * m_nWidth; i++)
		delete [] pDataSet[i];
	delete [] pDataSet;

	printf("CFeatureExtraction::DoPCA out\n");
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////

// Only nExtractChans=3 is supported!!!
bool CFeatureExtraction::GetChannels(CvMat * pMergedMat, CvMat * pChannels[], int nTotalChans, int nExtractChans)
{
	static int s_nChannel = 1;

	printf("\nCFeatureExtraction::GetChannels in\n");
	
	printf("GetChannels: Store each of the 3 p-channels in a matrix\n");
	// Store each of the 3 p-channels in a matrix
	float val;

	for (int i=0;i<m_nHeight;i++)
	{
		for (int j=0;j<m_nWidth;j++)
		{
			for (int k=0;k<nExtractChans;k++)
			{
				val = pMergedMat->data.fl[(i*m_nWidth+j)*nTotalChans + k];
				pChannels[k]->data.fl[i*m_nWidth+j] = val;
			}
		}
	}
	
	printf("GetChannels: Normalize to 0..1\n");
	// Normalize to 0..1	
	for (int k=0;k<nExtractChans;k++)
		cvNormalize(pChannels[k],pChannels[k], 0, 1, CV_MINMAX);

	printf("GetChannels: Save each channel to a bitmap, just for fun.\n");
	// Save each channel to a bitmap, just for fun.
	char filename[255];
	for (int i=0;i<m_nChannels;i++)
	{
		sprintf(filename, "chan%d.bmp", s_nChannel);
		saveChannel(filename, pChannels[i]);
		s_nChannel++;	
	}

	printf("CFeatureExtraction::GetChannels out\n");
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////

CFeatureExtraction::CFeatureExtraction(char * file)
{	
	printf("\nCFeatureExtraction::CFeatureExtraction in\n");
	// Load the input image
	printf("CFeatureExtraction: Loading image %s\n", file);
	m_pSrcImg = cvLoadImage(file,1);
	if (m_pSrcImg == NULL)
		return;

	m_pFile = file;
	
	// Extract parameters
	m_nChannels = m_pSrcImg->nChannels;
	m_nWidth = m_pSrcImg->width;
	m_nHeight = m_pSrcImg->height; 	
	
	printf("CFeatureExtraction: m_nChannels=%d, m_nWidth=%d, m_nHeight=%d\n", m_nChannels, m_nWidth, m_nHeight);
	// Scale to a 32bit float image (needed for later stages)
	m_pSrcImgFloat = cvCreateImage(cvSize(m_nWidth,m_nHeight),IPL_DEPTH_32F,3);
	cvConvertScale(m_pSrcImg,m_pSrcImgFloat,1.0,0);
	
	printf("CFeatureExtraction::CFeatureExtraction out\n");
}

//////////////////////////////////////////////////////////////////////////////////////

bool CFeatureExtraction::Run()
{
	printf("\nCFeatureExtraction::Run in\n");
	int i;

	CvMat * pColorChannels[3];
	for (i=0;i<3;i++)
		pColorChannels[i] = cvCreateMat( m_nWidth , m_nHeight , CV_32F );

	GetColorChannels(pColorChannels);

	CvMat * pTextureChannels[3];
	for (i = 0; i < 3; i++)
		pTextureChannels[i] = cvCreateMat(m_nWidth, m_nHeight, CV_32F);

	GetTextureChannels(pTextureChannels);

	for (i=0;i<3;i++)
		cvReleaseMat(&pColorChannels[i]);
	for (i=0;i<3;i++)
		cvReleaseMat(&pTextureChannels[i]);

	printf("CFeatureExtraction::out in\n");
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////
