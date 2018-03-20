#pragma once
#include <vector>
#include <RixSampleFilter.h>
#include "sample.h"
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

class bucket
{
public:
	int numComputations;	// initialized on 0
	int bucket_id[2];		//(x,y)
	int bucket_dims[2];
	RixMutex * bucket_mutex;
	vector< vector <sample>> edgeTop;
	vector< vector <sample>> edgeLeft;
	vector< vector <sample>> edgeRight;
	vector< vector <sample>> edgeBottom;
	vector< vector <vector<sample>>> sampleMatrix; //(x,y,z)

	bucket();
	bucket(int bucketID[2], RtFloat bucketDIMS[2]);
	~bucket();

	void freeBucket();
	void eval();
	void addSample(int x, int y, sample sampleObj);
	void applyAA();
};

