#include <RixSampleFilter.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <sstream>
#include "bucket.h"
#include "sample.h"

using namespace std;

int maxStoredBuckets = 2; // max # buckets stored in memory 
int thread_local previousBucket[2] = { -1 , -1 };

class presenzSampleFilter : public RixSampleFilter {
public:
	vector <bucket> bucketVector;

	presenzSampleFilter() {};
	virtual ~presenzSampleFilter() {};

	virtual int Init(RixContext& ctx, char const* pluginPath);
	virtual RixSCParamInfo const* GetParamTable();
	virtual void Finalize(RixContext& ctx);

	virtual void Filter(
		RixSampleFilterContext& fCtx,
		RtConstPointer instance);

	void getpixCoordinates(int sample, const RtPoint2 * screen, const RtFloat * screenwindow, double pixelWidth, double pixelHeight, int frameWidth, int frameHeight, RtFloat bucketsize[], int currentBucket[], int pixCoordinates[]);
	void print(string toPrint);
	void printAlways(string toPrint);

	virtual int CreateInstanceData(RixContext& ctx, char const* handle, RixParameterList const* params, InstanceData* instance);

private:
	RixMutex	*threadCountMutex;
	RixMutex	*generalMutex;

};

int presenzSampleFilter::Init(RixContext &ctx, char const *pluginpath)
{
	RixContext* context = RixGetContext();
	RixRenderState* renderState = (RixRenderState*)context->GetRixInterface(k_RixRenderState);
	RixThreadUtils *threadUtils = (RixThreadUtils*)ctx.GetRixInterface(k_RixThreadUtils);
	RixRenderState::FrameInfo frameInf;
	renderState->GetFrameInfo(&frameInf);
	int frameWidth = frameInf.displayState.resolution[0];	//get full render image width
	int frameHeight = frameInf.displayState.resolution[1];	//get full render image height

	if (RixThreadUtils *threadUtils = (RixThreadUtils*)ctx.GetRixInterface(k_RixThreadUtils))
	{
		generalMutex = threadUtils->NewMutex();
	}

	vector <bucket> newbucketvec(maxStoredBuckets); 
	bucketVector = newbucketvec;

	return 0;
}

RixSCParamInfo const *presenzSampleFilter::GetParamTable()
{
	static RixSCParamInfo table[] =
	{
		//End of table
		RixSCParamInfo()
	};

	return &table[0];
}

void presenzSampleFilter::Filter(RixSampleFilterContext& fCtx, RtConstPointer instance)
{	// GET RENDERMAN INTERFACES
	RixContext* context = RixGetContext();
	RixMessages* msgs = (RixMessages*)context->GetRixInterface(k_RixMessages);
	RixRenderState* renderState = (RixRenderState*)context->GetRixInterface(k_RixRenderState);
	RixRenderState::FrameInfo frameInf;
	renderState->GetFrameInfo(&frameInf);
	int frameWidth = frameInf.displayState.resolution[0];	//get full render image width
	int frameHeight = frameInf.displayState.resolution[1];	//get full render image height

	// GET OPTIONS
	std::ostringstream ss;
	ss << std::this_thread::get_id();
	std::string threadId = ss.str(); // unique id of the current thread 

	RixRenderState::Type type;
	RtInt count;
	RtFloat bucketsize[2];
	renderState->GetOption("limits:bucketsize", &bucketsize, sizeof(RtFloat[2]), &type, &count);	//get bucketsize
	RtFloat formatres[2];
	renderState->GetOption("Ri:FormatResolution", &formatres, sizeof(RtFloat[2]), &type, &count);	//get format resolution
	RtFloat screenwindow[4];
	renderState->GetOption("Ri:ScreenWindow", &screenwindow, sizeof(RtFloat[4]), &type, &count);	//get screen window [left, right, bottom, top]
	RtFloat cropwindow[4];
	renderState->GetOption("Ri:CropWindow", &cropwindow, sizeof(RtFloat[4]), &type, &count);		//get crop window 
	RtFloat maxsamples;
	renderState->GetOption("RiHider:maxsamples", &maxsamples, sizeof(RtFloat), &type, &count);		//get max number of samples taken for a pixel
	RtFloat filtersize[2];
	renderState->GetOption("RiPixelFilter:width", &filtersize, sizeof(RtFloat[2]), &type, &count);
	filtersize[0] = 0; 
	filtersize[1] = 0;
	int bucketshift = floor(filtersize[0] / 2); // the AA filter shifts buckets a certain amount depending on it size
	double pixelWidth = 1 / formatres[0];
	double pixelHeight = 1 / formatres[1];
	RtPoint2 const* screen = fCtx.screen;
	RtRayGeometry const* rays = fCtx.rays;

	// FIND WHICH BUCKET WE ARE CURRENTLY WORKING ON, DEFINED BY THE LOWEST (x,y) PIXEL COORDINATES OF THE BUCKET
	int currentBucket[2] = { frameWidth , frameHeight }; //(x,y)
	RixShadingContext const &shading = *fCtx.shadeCtxs[0];
	int point = 0;
	int sampleNr = shading.integratorCtxIndex[point];
	int pixCoordinates[2] = { 0,0 }; //(x,y)
	getpixCoordinates(sampleNr, screen, screenwindow, pixelWidth, pixelHeight, frameWidth, frameHeight, bucketsize, currentBucket, pixCoordinates);
	currentBucket[0] = (floor((pixCoordinates[0] + bucketshift) / (int)bucketsize[0]) * (int)bucketsize[0]) - bucketshift;
	currentBucket[1] = (floor((pixCoordinates[1] + bucketshift)/ (int)bucketsize[1]) * (int)bucketsize[1]) - bucketshift;
	//printAlways("Thread : " + threadId + " working on bucket: " + std::to_string(currentBucket[0]) + " , " + std::to_string(currentBucket[1]));
	
	// CHECK IF A BUCKET WAS FINISHED
	if(previousBucket[0] == -1 && previousBucket[1] == -1){ // set for the first time
		previousBucket[0] = currentBucket[0];
		previousBucket[1] = currentBucket[1];
	}
	if (previousBucket[0] != currentBucket[0] || previousBucket[1] != currentBucket[1]) { // thread finished bucket
		//printAlways("thread " + threadId + " finished bucket " + std::to_string(previousBucket[0]) + ", " + std::to_string(previousBucket[1]));
		//printAlways("thread " + threadId + " current bucket " + std::to_string(currentBucket[0]) + ", " + std::to_string(currentBucket[1]));
		int bucketIndex = 0;
		if (generalMutex) {
			generalMutex->Lock();
			bool bucketfound = false;
			for (size_t i = 0; i < bucketVector.size(); i++)
			{
				if (bucketVector[i].bucket_id[0] == -1 && bucketVector[i].bucket_id[1] == -1) {	// skip if free bucket object
					continue;
				}
				if (previousBucket[0] == bucketVector[i].bucket_id[0] && previousBucket[1] == bucketVector[i].bucket_id[1]) // found corresponding existing bucket object
				{
					bucketfound = true;
					bucketIndex = i;
					goto endPreviousBucketSearch;
				}
			}
			endPreviousBucketSearch: ;	//label to mark end of loop going over bucketvector
			if (!bucketfound) {		//bucket not found
				std::string MESSAGE = "FATAL ERROR, THREAD " + threadId + " COULD NOT FIND BUCKET: " + std::to_string(previousBucket[0]) + ", " + std::to_string(previousBucket[1]);
				printAlways(MESSAGE);
			}
			bucketVector[bucketIndex].eval();
			bucketVector[bucketIndex].freeBucket();														// reset to empty bucket object
			generalMutex->Unlock();
		}
		previousBucket[0] = currentBucket[0];
		previousBucket[1] = currentBucket[1];
	}
	
	// GET THE INDEX IN bucketVector AT WHICH CURRENT BUCKET OBJECT IS LOCATED
	int bucketIndex = 0;
	if (generalMutex) {
		generalMutex->Lock();
		bool bucketfound = false;
		for (size_t i = 0; i < bucketVector.size(); i++)
		{
			if (bucketVector[i].bucket_id[0] == -1 && bucketVector[i].bucket_id[1] == -1) {	// skip if free bucket object
				continue;
			}
			if (currentBucket[0] == bucketVector[i].bucket_id[0] && currentBucket[1] == bucketVector[i].bucket_id[1]) // found corresponding existing bucket object
			{
				bucketfound = true;
				bucketVector[i].numComputations += 1;
				bucketIndex = i;
				goto endCurrentBucketSearch;
			}
		}
		endCurrentBucketSearch: ;	//label to mark end of loop going over bucketvector
		if (!bucketfound) {		//bucket not found
			for (size_t i = 0; i < bucketVector.size(); i++)
			{
				if (bucketVector[i].bucket_id[0] == -1 && bucketVector[i].bucket_id[1] == -1) {
					bucket newbucket(currentBucket, bucketsize);
					bucketVector[i] = newbucket;
					bucketIndex = i;
					break;
				}
				if (i == bucketVector.size() - 1) {
					std::string MESSAGE = "NO MORE MEMORY AVAILABLE FOR BUCKET" + std::to_string(currentBucket[0]) + ", " + std::to_string(currentBucket[1]);
					printAlways(MESSAGE);
					for (size_t i = 0; i < bucketVector.size() ; i++)
					{
						std::string X = "in storage: [" + to_string(bucketVector[i].bucket_id[0]) + "," + to_string(bucketVector[i].bucket_id[1]) + "] numsamples: " + to_string(bucketVector[i].numComputations);
						printAlways(X);
					}
				}
			}
		}

		generalMutex->Unlock();
	}
	
	//RtColorRGB * test = new RtColorRGB[256]; //324
	//fCtx.ReadRegion(0, currentBucket[0], currentBucket[0]+bucketsize[0]-1, currentBucket[1], currentBucket[1] + bucketsize[1]-1, test);
	//for (size_t i = 0; i < 256; i++)
	//{
	//	printAlways("bucket : " + to_string(currentBucket[0]) + " , " + to_string(currentBucket[1]) + " index: " + to_string(i) + " : " + to_string(test[i].r) + " " + to_string(test[i].g) + " " + to_string(test[i].b));
	//}
	//delete[] test;

	// LOOP OVER SAMPLES
	for (int group = 0; group < fCtx.numGrps; group++)
	{
		RixShadingContext const &shading = *fCtx.shadeCtxs[group];
		RtFloat const *vlen; //array of float depths
		shading.GetBuiltinVar(RixShadingContext::k_VLen, &vlen);
		RtFloat3 const *Nn;
		shading.GetBuiltinVar(RixShadingContext::k_Nn, &Nn);

		for (int point = 0; point < shading.numPts; point++)
		{
			//Declare variables
			float sampleDepth;
			RtColorRGB sampleColor;
			int pixCoordinates[2] = { 0,0 }; //(x,y)
			RtFloat3 normal;
			//Get values for sample
			int sampleNr = shading.integratorCtxIndex[point];	// ray[sample] hit distance is vlen[point]...
			sampleDepth = vlen[point];							// get depth of sample
			fCtx.Read(0, sampleNr, sampleColor);				// get color of sample
			getpixCoordinates(sampleNr, screen, screenwindow, pixelWidth, pixelHeight, frameWidth, frameHeight, bucketsize, currentBucket, pixCoordinates); //get x and y pixel coordinates for sample
			normal = Nn[point];									// get normal of sample 
			//Create and save sample object
			sample newsample(sampleColor, sampleDepth, normal);
			bucketVector[bucketIndex].addSample(pixCoordinates[0], pixCoordinates[1], newsample);
		}
	}

	//APPLY ANTI ALLIASSING ON NEWLY ADDED SAMPLES does not work yet
	//bucketVector[bucketIndex].applyAA();
}

void  presenzSampleFilter::getpixCoordinates(int sample, const RtPoint2 * screen, const RtFloat * screenwindow, double pixelWidth, double pixelHeight, int frameWidth, int frameHeight, RtFloat bucketsize[], int currentBucket[], int pixCoordinates[]) {

	float sampleScreenXcoordinate = screen[sample].x; //get the x screen coordinate of sample
	sampleScreenXcoordinate = (sampleScreenXcoordinate + abs(screenwindow[1])); // shift range to positive values, between 0 and 2*screenwindow[1]
	sampleScreenXcoordinate /= (2 * abs(screenwindow[1])); //normalize
	sampleScreenXcoordinate /= pixelWidth;
	sampleScreenXcoordinate = floor(sampleScreenXcoordinate);

	float sampleScreenYcoordinate = screen[sample].y * -1; //get the y screen coordinate of sample, multiply by -1 to invert in correct direction
	sampleScreenYcoordinate = (sampleScreenYcoordinate + abs(screenwindow[2])); // shift range to positive values, between 0 and 2*screenwindow[2]
	sampleScreenYcoordinate /= (2 * abs(screenwindow[2])); //normalize
	sampleScreenYcoordinate /= pixelHeight;
	sampleScreenYcoordinate = floor(sampleScreenYcoordinate);

	//The following if tests were added for the case when samples are taken exactly on the lower and right edge of a bucket, this hapens rarely
	if (sampleScreenYcoordinate == (currentBucket[1] + bucketsize[1])) {
		sampleScreenYcoordinate -= 1;
	}
	if (sampleScreenXcoordinate == (currentBucket[0] + bucketsize[0])) {
		sampleScreenXcoordinate -= 1;
	}
	
	pixCoordinates[0] = sampleScreenXcoordinate ;
	pixCoordinates[1] = sampleScreenYcoordinate ;
	
}

void  presenzSampleFilter::print(string toPrint) {
	RixContext* context = RixGetContext();
	RixMessages* msgs = (RixMessages*)context->GetRixInterface(k_RixMessages);
	const char * constX = toPrint.c_str();
	msgs->Info(constX);

}

void  presenzSampleFilter::printAlways(string toPrint) {
	RixContext* context = RixGetContext();
	RixMessages* msgs = (RixMessages*)context->GetRixInterface(k_RixMessages);
	const char * constX = toPrint.c_str();
	msgs->InfoAlways(constX);

}

int presenzSampleFilter::CreateInstanceData(RixContext& ctx, char const* handle, RixParameterList const* params, InstanceData* instance) {

	return 0;
}

void presenzSampleFilter::Finalize(RixContext& ctx) {
	if (generalMutex)
	{
		delete generalMutex;
		generalMutex = 0;
	}
}

RIX_SAMPLEFILTERCREATE{
	return new presenzSampleFilter();
}

RIX_SAMPLEFILTERDESTROY{
	delete((presenzSampleFilter*)filter);
}

/*
RixContext* context = RixGetContext();
RixMessages* msgs = (RixMessages*)context->GetRixInterface(k_RixMessages);
msgs->Info("x: ");
std::string X = to_string(direction.x);
const char * constX = X.c_str();
msgs->Info(constX);
*/
