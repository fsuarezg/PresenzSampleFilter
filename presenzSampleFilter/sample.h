#pragma once
#include <RixSampleFilter.h>

class sample
{
public:
	RtColorRGB color;
	float depth;
	RtFloat3 normal;

	sample();
	sample(RtColorRGB inputColor, float inputDepth, RtFloat3 inputNormal);
	~sample();
};

