#define UBUNTU_16_04_HACK

// apt-get install libfreeimage-dev
// -lfreeimage

#ifndef FREEIMAGEHELPER_H
#define FREEIMAGEHELPER_H

#undef min
#undef max

#include <string>
#include <algorithm>

#include "FreeImage.h"


#ifdef UBUNTU_16_04_HACK
// https://bugs.launchpad.net/ubuntu/+source/freeimage/+bug/1614266
#undef FIF_EXR
#define FIF_EXR ((FREE_IMAGE_FORMAT)28)
#endif

#ifndef MINF
#define MINF -std::numeric_limits<float>::infinity()
#endif

struct FreeImage {

	FreeImage();
	FreeImage(unsigned int width, unsigned int height, unsigned int nChannels = 4);
	FreeImage(const FreeImage& img);
	FreeImage(const std::string& filename);

	~FreeImage();

	void operator=(const FreeImage& other);

	void SetDimensions(unsigned int width, unsigned int height, unsigned int nChannels = 4);

	FreeImage ConvertToIntensity() const;

	bool LoadImageFromFile(const std::string& filename, unsigned int width = 0, unsigned int height = 0, bool flipY = false);

	bool SaveImageToFile(const std::string& filename, bool flipY = false);

	unsigned int w;
	unsigned int h;
	unsigned int nChannels;
	float* data;
};

#endif