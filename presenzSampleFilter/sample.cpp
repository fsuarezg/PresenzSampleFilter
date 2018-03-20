#include "sample.h"



sample::sample(RtColorRGB inputColor, float inputDepth, RtFloat3 inputNormal)
{
	color = inputColor;
	depth = inputDepth;
	normal = inputNormal;
}

sample::sample() {

}

sample::~sample()
{
}
