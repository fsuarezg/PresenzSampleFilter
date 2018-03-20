#include "bucket.h"


bucket::bucket() {
	numComputations = 0;
	bucket_id[0] = -1;
	bucket_id[1] = -1;
	bucket_dims[0] = 0;
	bucket_dims[1] = 0;
	RixContext* context = RixGetContext();
	RixThreadUtils *threadUtils = (RixThreadUtils*)context->GetRixInterface(k_RixThreadUtils);
	if (RixThreadUtils *threadUtils =
		(RixThreadUtils*)context->GetRixInterface(k_RixThreadUtils))
	{
		bucket_mutex = threadUtils->NewMutex();
	}
	if (bucket_dims[0] <= 2) { //buckets smaller than 2 are not really allowed
		vector< vector<sample> > vec1(0, vector<sample>(0));
		edgeTop = vec1;
		edgeBottom = vec1;
		vector< vector<sample> > vec2(0, vector<sample>(0));
		edgeLeft = vec2;
		edgeRight = vec2;
		vector< vector<sample> > vec3(0, vector<sample>(0));
		vector < vector < vector<sample> >> matrix(0, vec3);
		sampleMatrix = matrix;
	}
	else {
		vector< vector<sample> > vec1(bucket_dims[0], vector<sample>(0));
		edgeTop = vec1;
		edgeBottom = vec1;
		vector< vector<sample> > vec2(bucket_dims[1] - 2, vector<sample>(0));
		edgeLeft = vec2;
		edgeRight = vec2;
		vector< vector<sample> > vec3(bucket_dims[1], vector<sample>(0));
		vector < vector < vector<sample> >> matrix(bucket_dims[0], vec3);
		sampleMatrix = matrix;
	}

	
}

bucket::bucket(int bucketID[2], RtFloat bucketDIMS[2])
{
	numComputations = 0;
	bucket_id[0] = bucketID[0];
	bucket_id[1] = bucketID[1];
	bucket_dims[0] = (int)bucketDIMS[0];
	bucket_dims[1] = (int)bucketDIMS[1];

	RixContext* context = RixGetContext();
	RixThreadUtils *threadUtils = (RixThreadUtils*)context->GetRixInterface(k_RixThreadUtils);
	if (RixThreadUtils *threadUtils =
		(RixThreadUtils*)context->GetRixInterface(k_RixThreadUtils))
	{
		bucket_mutex = threadUtils->NewMutex();
	}
	if (bucket_dims[0] <= 2) { //buckets smaller than 2 are not allowed
		vector< vector<sample> > vec1(0, vector<sample>(0));
		edgeTop = vec1;
		edgeBottom = vec1;
		vector< vector<sample> > vec2(0, vector<sample>(0));
		edgeLeft = vec2;
		edgeRight = vec2;
		vector< vector<sample> > vec3(0, vector<sample>(0));
		vector < vector < vector<sample> >> matrix(0, vec3);
		sampleMatrix = matrix;
	}
	else {
		vector< vector<sample> > vec1(bucket_dims[0], vector<sample>(0));
		edgeTop = vec1;
		edgeBottom = vec1;
		vector< vector<sample> > vec2(bucket_dims[1] - 2, vector<sample>(0));
		edgeLeft = vec2;
		edgeRight = vec2;
		vector< vector<sample> > vec3(bucket_dims[1], vector<sample>(0));
		vector < vector < vector<sample> >> matrix(bucket_dims[0], vec3);
		sampleMatrix = matrix;
	}
}


bucket::~bucket()
{
	if (bucket_mutex)
	{
		delete bucket_mutex;
		bucket_mutex = 0;
	}
}

void bucket::freeBucket() {
	bucket_id[0] = -1;
	bucket_id[1] = -1;
	bucket_dims[0] = 0;
	bucket_dims[1] = 0;
	edgeTop.clear();
	edgeBottom.clear();
	edgeLeft.clear();
	edgeRight.clear();
	sampleMatrix.clear();

	if (bucket_dims[0] <= 2) { //buckets smaller than 2 are not really allowed
		vector< vector<sample> > vec1(0, vector<sample>(0));
		edgeTop = vec1;
		edgeBottom = vec1;
		vector< vector<sample> > vec2(0, vector<sample>(0));
		edgeLeft = vec2;
		edgeRight = vec2;
	}
	else {
		vector< vector<sample> > vec1(bucket_dims[0], vector<sample>(0));
		edgeTop = vec1;
		edgeBottom = vec1;
		vector< vector<sample> > vec2(bucket_dims[1] - 2, vector<sample>(0));
		edgeLeft = vec2;
		edgeRight = vec2;
	}
	numComputations = 0;
}

void bucket::eval() //placeholder for bucket evaluation function
{
	ofstream myfile;
	for (size_t x = 0; x < bucket_dims[0]; x++)
	{
		for (size_t y = 0; y < bucket_dims[1]; y++)
		{
			float r = 0;
			float g = 0;
			float b = 0;
			for (size_t z = 0; z < sampleMatrix[x][y].size(); z++)
			{
				r += sampleMatrix[x][y][z].color.r;
				g += sampleMatrix[x][y][z].color.g;
				b += sampleMatrix[x][y][z].color.b;
			}
			r /= sampleMatrix[x][y].size();
			g /= sampleMatrix[x][y].size();
			b /= sampleMatrix[x][y].size();
			RtColorRGB result; result.r = r; result.g = g; result.b = b;
			myfile.open(".\\myData\\" + to_string(y + bucket_id[1]) + "_" + to_string(x + bucket_id[0]), ofstream::out);
			myfile << y+bucket_id[1] << ", " << x+bucket_id[0] << " : " << result.r << ", " << result.g << ", " << result.b << endl;
			myfile.close();
		}
	}
	
}

void bucket::addSample(int x, int y, sample sampleObj)
{
	int xCorrected = x - bucket_id[0];
	int yCorrected = y - bucket_id[1];

	RixContext* context = RixGetContext();
	RixMessages* msgs = (RixMessages*)context->GetRixInterface(k_RixMessages);
	
	if (xCorrected == 0) {
		if (yCorrected == 0) {
			edgeTop[xCorrected].push_back(sampleObj);
		}
		else if (yCorrected == (bucket_dims[1] - 1)) {
			edgeBottom[xCorrected].push_back(sampleObj);
		}
		else {
			edgeLeft[yCorrected - 1].push_back(sampleObj);
		}
	}
	else if (xCorrected == (bucket_dims[0] - 1)) {
		if (yCorrected == 0) {
			edgeTop[xCorrected].push_back(sampleObj);
		}
		else if (yCorrected == (bucket_dims[1] - 1)) {
			edgeBottom[xCorrected].push_back(sampleObj);
		}
		else {
			edgeRight[yCorrected - 1].push_back(sampleObj);
		}
	}
	else {
		if (yCorrected == 0) {
			edgeTop[xCorrected].push_back(sampleObj);
		}
		if (yCorrected == (bucket_dims[1] - 1)) {
			edgeBottom[xCorrected].push_back(sampleObj);
		}
	}
	/*if (bucket_id[0] == 480 && bucket_id[1] == 96) {
		RixContext* context = RixGetContext();
		RixMessages* msgs = (RixMessages*)context->GetRixInterface(k_RixMessages);
		std::string X = to_string(x) + " " + to_string(y);
		const char * constX = X.c_str();
		msgs->InfoAlways(constX);
		std::string Y = to_string(xCorrected) + " " + to_string(yCorrected);
		const char * constY = Y.c_str();
		msgs->InfoAlways(constY);
	}*/
	sampleMatrix[xCorrected][yCorrected].push_back(sampleObj);
}

void bucket::applyAA() { // does not work yet, out of bounds
	vector <vector<float>> kernel = {
		{0.0625, 0.125 , 0.0625}, 
		{ 0.125, 0.250 , 0.125 }, 
		{0.0625, 0.125 , 0.0625}
	};
	vector < vector<RtColorRGB> > newColors;
	RtColorRGB empty; empty.r = 0; empty.g = 0; empty.b = 0;
	for (size_t x = 0; x < bucket_dims[0]; x++)
	{
		for (size_t y = 0; y < bucket_dims[1]; y++)
		{
			newColors[x][y] = empty;
		}
	}
	for (size_t x = 0; x < bucket_dims[0]; x++)
	{
		for (size_t y = 0; y < bucket_dims[1]; y++)
		{
			float r = 0; float g = 0; float b = 0;
			for (size_t i = -1; i < 2; i++) // x : -1, 0, 1
			{
				for (size_t j = -1; j < 2; j++) // y : -1, 0 , 1
				{
					int xindex = x + i;
					int yindex = y + j;
					if (xindex < 0 || xindex >= bucket_dims[0] || yindex < 0 || yindex >= bucket_dims[1]) {
						RtColorRGB currentColor = sampleMatrix[x][y].back().color;
						r += currentColor.r * kernel[i + 1][j + 1];
						g += currentColor.g * kernel[i + 1][j + 1];
						b += currentColor.b * kernel[i + 1][j + 1];
					}
					else {
						RtColorRGB currentColor = sampleMatrix[xindex][yindex].back().color;
						r += currentColor.r * kernel[i + 1][j + 1];
						g += currentColor.g * kernel[i + 1][j + 1];
						b += currentColor.b * kernel[i + 1][j + 1];
					}
				}
			}
			newColors[x][y].r = r;
			newColors[x][y].g = g;
			newColors[x][y].b = b;

		}
	}
	for (size_t x = 0; x < bucket_dims[0]; x++)
	{
		for (size_t y = 0; y < bucket_dims[1]; y++)
		{
			sampleMatrix[x][y].back().color = newColors[x][y];
		}
	}
}