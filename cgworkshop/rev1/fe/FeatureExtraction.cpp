#include "FeatureExtraction.h"
#include "cvgabor.h"

static void displayImage(char * title, IplImage * pImg)
{
	cvNamedWindow(title, 1);
	cvShowImage(title,pImg);
	cvWaitKey(0);
	cvDestroyWindow(title);		
}

// TODO: Not really working
void CFeatureExtraction::CalcHistogram(IplImage * pImg, CvMat * pHistogram)
{
	int nBins = 10;
	
	int step = pImg->widthStep;
	int channels = m_nChannels;
	int w = m_nWidth;
	int h = m_nHeight;
	uchar * data  = (uchar *)pImg->imageData;
	
	for (int y=0; y<h; y++)
	{
		for (int x=0;x<w; x++)
		{
			//printf("%d,%d\n", x,y);
	        for (int k=0;k<channels;k++)
	                for (int l=0;l<nBins;l++)
	                        pHistogram->data.fl[x*step+y*channels+k*nBins+l] = 0;
	
	        for (int j=y-2;j<=y+2;j++)
	        {
	
				if (j<0 || j>=h)
                	continue;

                for (int i=x-2;i<=x+2;i++)
                {
                    if (i<0 || i>=w)
                            continue;

                    for (int k=0;k<channels;k++)
                    {
                            uchar val = data[i*step+j*channels+k];;
                            uchar bin = val*nBins/255;
                            pHistogram->data.fl[x*step+y*channels+k*nBins+bin]+=1;
                    }
                }
			}
		}
	}
}

void CFeatureExtraction::GetHistogram(CvMat * pHistVectors[])
{
	// Calc the full histogram vectors
	CvMat * pMat = cvCreateMat( m_nWidth*m_nHeight, 30 , CV_32F );
	CalcHistogram(m_pSrcImg, pMat);
	
		// Create our result matrices
	CvMat* avg = cvCreateMat( 1, 30, CV_32F );
	CvMat* eigenVectors = cvCreateMat( 30, 30, CV_32F );
	CvMat* eigenValues = cvCreateMat( 30, 1, CV_32F );	
	
	// Actual PCA calculation
	cvCalcPCA(pMat, avg, eigenValues, eigenVectors, CV_PCA_DATA_AS_ROW ); 
	
	// Useful debugging
	printf("Mean Vector: %f,%f,%f\n", cvmGet(avg, 0,0), cvmGet(avg, 0,1), cvmGet(avg, 0,2));
	printf("Eigen Values: %f,%f,%f\n", cvmGet(eigenValues, 0,0), cvmGet(eigenValues, 1,0), cvmGet(eigenValues, 2,0));
	printf("Eigen Vector 1: %f, %f, %f\n", cvmGet(eigenVectors, 0,0), cvmGet(eigenVectors, 0,1), cvmGet(eigenVectors, 0,2));
	printf("Eigen Vector 2: %f, %f, %f\n", cvmGet(eigenVectors, 1,0), cvmGet(eigenVectors, 1,1), cvmGet(eigenVectors, 1,2));
	printf("Eigen Vector 3: %f, %f, %f\n", cvmGet(eigenVectors, 2,0), cvmGet(eigenVectors, 2,1), cvmGet(eigenVectors, 2,2));
	
	// Transform to the new basis
	CvMat * pTransMat = cvCreateMat( m_nWidth*m_nHeight, 30 , CV_32F );
	cvMatMul(pMat, eigenVectors, pTransMat);

	// TODO: Normalize each channel by itself?		
	// Normalize the matrix (0..255)
	cvNormalize(pTransMat, pTransMat, 0, 255, CV_MINMAX);
	
	// Store each of the 3 p-channels in a matrix
	float val;
	for (int k=0;k<m_nChannels;k++)
	{
		for (int i=0;i<m_nHeight;i++)
		{
			for (int j=0;j<m_nWidth;j++)
			{
					val = pTransMat->data.fl[i*m_nWidth*3 + j*10 + k];
					pHistVectors[k]->data.ptr[i*m_nWidth+j] = (unsigned char) val;
			}
		}
	}
	
	// Save each channel to a bitmap, just for fun.
	char filename[255];
	IplImage * pImg = cvCreateImage(cvSize(m_nWidth,m_nHeight),IPL_DEPTH_8U,1);
	char * tempData = pImg->imageData;
	for (int i=0;i<m_nChannels;i++)
	{
		sprintf(filename, "output/hist%d.bmp", i+1);
		pImg->imageData = (char *) pHistVectors[i]->data.ptr;
		
		// TODO: Remove this, only a test
		displayImage(filename, pImg);
		
		printf("Saving pchannel %d to: %s\n",i+1, filename);
		if (!cvSaveImage(filename,pImg)) 
			printf("Could not save: %s\n",filename);
	}
	pImg->imageData = tempData;
	cvReleaseImage(&pImg);
	
	// Useful releasing
	cvReleaseMat(&pMat);
	cvReleaseMat(&avg);
	cvReleaseMat(&eigenVectors);
	cvReleaseMat(&eigenValues);
	cvReleaseMat(&pTransMat);
}

int CFeatureExtraction::GetGaborResponse(IplImage *pGrayImg, IplImage *pResImg, double orientation, int scale)
{
	// Create the filter
	// TODO: Sigma? F? What?
	double Sigma = 2*PI;
	double F = sqrt(2.0);
		
	CvGabor *pGabor = new CvGabor;
	pGabor->Init(orientation, scale, Sigma, F);
	
	// Convolution
	pGabor->conv_img(pGrayImg, pResImg, CV_GABOR_MAG);
	
	return 0;
}

// TODO: Apply all filters
// What are the correct filterS?
// 6 orientations, 4 scales
// For orientation, I assume equal spacing around PI, but scale?

// Save results (one matrix per response)
// Save to bitmaps
int CFeatureExtraction::GetGaborResponse()
{
	// Convert our image to grayscale (Gabor doesn't care about colors! I hope?)	
	IplImage *pGrayImg = cvCreateImage(cvSize(m_pSrcImg->width,m_pSrcImg->height), IPL_DEPTH_8U, 1);
	cvCvtColor(m_pSrcImg,pGrayImg,CV_BGR2GRAY);

	// The output image
	IplImage *reimg = cvCreateImage(cvSize(pGrayImg->width,pGrayImg->height), IPL_DEPTH_8U, 1);

	char title[255];
	for (double orientation=0;orientation<PI;orientation+=PI/6)
		for (int scale=1;scale<=4;scale++)
		{
			sprintf(title, "Gabor Response: Orientation=%f, Scale=%d\n", orientation*180/PI, scale);
			// TEST: Apply gabor with orientation PI/4, scale 3
			GetGaborResponse(pGrayImg, reimg, orientation, scale  );
			
			// This being a test and all, display the image
			displayImage(title, reimg);
		}
	
	// Release
	cvReleaseImage(&reimg);
	cvReleaseImage(&pGrayImg);
	
	return 0;
}

// TODO: Would fail if m_nChannels != 3
// RGB to LAB
int CFeatureExtraction::GetColorPCA(CvMat * pColorChannels[])
{
	// Convert to LAB color space
	IplImage *pLabImg = cvCreateImage(cvSize(m_pSrcImg->width,m_pSrcImg->height), IPL_DEPTH_32F, 3);
	cvCvtColor(m_pSrcImgFloat,pLabImg,CV_BGR2Lab);	

	// Get our 32F matrix (From the 32F image created previously)	
	CvMat * pMat = cvCreateMat( m_nWidth*m_nHeight, 3 , CV_32F );
	memcpy(pMat->data.fl, (float*)pLabImg->imageData, pLabImg->imageSize);

	// Create our result matrices
	CvMat* avg = cvCreateMat( 1, 3, CV_32F );
	CvMat* eigenVectors = cvCreateMat( 3, 3, CV_32F );
	CvMat* eigenValues = cvCreateMat( 3, 1, CV_32F );	
	
	// Actual PCA calculation
	cvCalcPCA(pMat, avg, eigenValues, eigenVectors, CV_PCA_DATA_AS_ROW ); 
	
	// Useful debugging
	printf("Mean Vector: %f,%f,%f\n", cvmGet(avg, 0,0), cvmGet(avg, 0,1), cvmGet(avg, 0,2));
	printf("Eigen Values: %f,%f,%f\n", cvmGet(eigenValues, 0,0), cvmGet(eigenValues, 1,0), cvmGet(eigenValues, 2,0));
	printf("Eigen Vector 1: %f, %f, %f\n", cvmGet(eigenVectors, 0,0), cvmGet(eigenVectors, 0,1), cvmGet(eigenVectors, 0,2));
	printf("Eigen Vector 2: %f, %f, %f\n", cvmGet(eigenVectors, 1,0), cvmGet(eigenVectors, 1,1), cvmGet(eigenVectors, 1,2));
	printf("Eigen Vector 3: %f, %f, %f\n", cvmGet(eigenVectors, 2,0), cvmGet(eigenVectors, 2,1), cvmGet(eigenVectors, 2,2));
	
	// Transform to the new basis
	CvMat * pTransMat = cvCreateMat( m_nWidth*m_nHeight, 3 , CV_32F );
	cvMatMul(pMat, eigenVectors, pTransMat);

	// TODO: Normalize each channel by itself?		
	// Normalize the matrix (0..255)
	cvNormalize(pTransMat, pTransMat, 0, 255, CV_MINMAX);
	
	// Store each of the 3 p-channels in a matrix
	float val;
	for (int k=0;k<m_nChannels;k++)
	{
		for (int i=0;i<m_nHeight;i++)
		{
			for (int j=0;j<m_nWidth;j++)
			{
					val = pTransMat->data.fl[i*m_nWidth*3 + j*m_nChannels + k];
					pColorChannels[k]->data.ptr[i*m_nWidth+j] = (unsigned char) val;
			}
		}
	}
	
	// Save each channel to a bitmap, just for fun.
	char filename[255];
	IplImage * pImg = cvCreateImage(cvSize(m_nWidth,m_nHeight),IPL_DEPTH_8U,1);
	char * tempData = pImg->imageData;
	for (int i=0;i<m_nChannels;i++)
	{
		sprintf(filename, "output/chan%d.bmp", i+1);
		pImg->imageData = (char *) pColorChannels[i]->data.ptr;
		
		// TODO: Remove this, only a test
		displayImage(filename, pImg);
		
		printf("Saving pchannel %d to: %s\n",i+1, filename);
		if (!cvSaveImage(filename,pImg)) 
			printf("Could not save: %s\n",filename);
	}
	pImg->imageData = tempData;
	cvReleaseImage(&pImg);
	
	// Useful releasing
	cvReleaseMat(&pMat);
	cvReleaseMat(&avg);
	cvReleaseMat(&eigenVectors);
	cvReleaseMat(&eigenValues);
	cvReleaseMat(&pTransMat);
	return 0;	
}

CFeatureExtraction::CFeatureExtraction(char * file)
{
	m_pFile = file;
	
	// Load the input image
	m_pSrcImg = cvLoadImage(file,1);
	
	// Extract parameters
	m_nChannels = m_pSrcImg->nChannels;
	m_nWidth = m_pSrcImg->width;
	m_nHeight = m_pSrcImg->height; 	
	
	// Scale to a 32bit float image (needed for later stages)
	m_pSrcImgFloat = cvCreateImage(cvSize(m_nWidth,m_nHeight),IPL_DEPTH_32F,3);
	cvConvertScale(m_pSrcImg,m_pSrcImgFloat,1.0,0);
}

int CFeatureExtraction::Run()
{
	CvMat * pColorChannels[3];
	for (int i=0;i<3;i++)
		pColorChannels[i] = cvCreateMat( m_nWidth , m_nHeight , CV_8U );
		
	GetColorPCA(pColorChannels);

	GetGaborResponse();

	CvMat * pHistVectors[3];
	for (int i=0;i<3;i++)
		pHistVectors[i] = cvCreateMat( m_nWidth , m_nHeight , CV_8U );

	GetHistogram(pHistVectors);

	for (int i=0;i<3;i++)
		cvReleaseMat(&pColorChannels[i]);
	for (int i=0;i<3;i++)
		cvReleaseMat(&pHistVectors[i]);

	
	return 0;
}
