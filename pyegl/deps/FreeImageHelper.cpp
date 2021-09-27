#include "FreeImageHelper.h"

#include <iostream>
#include <cstring>
#include <fstream>

//#pragma comment(lib, "FreeImage.lib")

FreeImage::FreeImage() : w(0), h(0), nChannels(0), data(nullptr)
{
}

FreeImage::FreeImage(unsigned int width, unsigned int height, unsigned int nChannels) :
	w(width), h(height), nChannels(nChannels), data(new float[nChannels * width*height])
{
}

FreeImage::FreeImage(const FreeImage& img) :
	w(img.w), h(img.h), nChannels(img.nChannels), data(new float[nChannels * img.w*img.h])
{
	memcpy(data, img.data, sizeof(float) * nChannels * w*h);
}

FreeImage::FreeImage(const std::string& filename) : w(0), h(0), nChannels(0), data(nullptr)
{
	LoadImageFromFile(filename);
}

FreeImage::~FreeImage()
{
	if (data != nullptr) delete[] data;
}

void FreeImage::operator=(const FreeImage& other)
{
	if (other.data != this->data)
	{
		SetDimensions(other.w, other.h, other.nChannels);
		memcpy(data, other.data, sizeof(float) * nChannels * w * h);
	}
}

void FreeImage::SetDimensions(unsigned int width, unsigned int height, unsigned int nChannels)
{
	if (data != nullptr) delete[] data;
	w = width;
	h = height;
	this->nChannels = nChannels;
	data = new float[nChannels * width * height];
}

FreeImage FreeImage::ConvertToIntensity() const
{
	FreeImage result(w, h, 1);

	for (unsigned int j = 0; j < h; ++j)
	{
		for (unsigned int i = 0; i < w; ++i)
		{
			float sum = 0.0f;
			for (unsigned int c = 0; c < nChannels; ++c)
			{
				if (data[nChannels * (i + w*j) + c] == MINF)
				{
					sum = MINF;
					break;
				}
				else
				{
					sum += data[nChannels * (i + w*j) + c];
				}
			}
			if (sum == MINF) result.data[i + w*j] = MINF;
			else result.data[i + w*j] = sum / nChannels;
		}
	}

	return result;
}

bool FreeImage::LoadImageFromFile(const std::string& filename, unsigned int width, unsigned int height, bool flipY)
{
	FreeImage_Initialise();
	if (data != nullptr) delete[] data;

	//image format
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
	//pointer to the image, once loaded
	FIBITMAP *dib(0);

	//check the file signature and deduce its format
	fif = FreeImage_GetFileType(filename.c_str(), 0);
	if (fif == FIF_UNKNOWN) fif = FreeImage_GetFIFFromFilename(filename.c_str());
	if (fif == FIF_UNKNOWN) return false;

	//check that the plugin has reading capabilities and load the file
	if (FreeImage_FIFSupportsReading(fif)) dib = FreeImage_Load(fif, filename.c_str());
	if (!dib) return false;

	// Convert to RGBA float images
	FIBITMAP* hOldImage = dib;
	dib = FreeImage_ConvertToRGBAF(hOldImage); // ==> 4 channels
	FreeImage_Unload(hOldImage);

	//get the image width and height
	w = FreeImage_GetWidth(dib);
	h = FreeImage_GetHeight(dib);

	// rescale to fit width and height
	if (width != 0 && height != 0)
	{
		FIBITMAP* hOldImage = dib;
		dib = FreeImage_Rescale(hOldImage, width, height, FILTER_CATMULLROM);
		FreeImage_Unload(hOldImage);
		w = width;
		h = height;
	}

	//retrieve the image data
	BYTE* bits = FreeImage_GetBits(dib);

	//if this somehow one of these failed (they shouldn't), return failure
	if ((bits == 0) || (w == 0) || (h == 0))
		return false;

	nChannels = 4;

	// copy image data
	data = new float[nChannels * w * h];

	// flip
	if (!flipY) // per default it is flipped, because of dx rendering
	{
		for (unsigned int y = 0; y < h; ++y)
		{
			memcpy(&(data[y*nChannels * w]), &bits[sizeof(float) * (h - 1 - y) * nChannels * w], sizeof(float) * nChannels * w);
		}
	}
	else
	{
		memcpy(data, bits, sizeof(float) * nChannels * w * h);
	}

	//Free FreeImage's copy of the data
	FreeImage_Unload(dib);

	return true;
}

bool FreeImage::SaveImageToFile(const std::string& filename, bool flipY)
{
	std::string ext = std::string(filename.begin() + filename.find_last_of('.'), filename.end());

	if (ext == ".bin" || ext == ".BIN")
	{
		std::ofstream file(filename, std::ios::out | std::ios::binary);
		if(file.is_open())
		{
			file.write((char*)&w, sizeof(unsigned int));
			file.write((char*)&h, sizeof(unsigned int));
			file.write((char*)&nChannels, sizeof(unsigned int));
			file.write((char*)data, sizeof(float)*w*h*nChannels);
			file.close();
			return true;
		}
		else
		{
			return false;
		}
		
	}
	else if (ext == ".tif" || ext == ".TIF" || ext == ".tiff" || ext == ".TIFF")
	{
		//FREE_IMAGE_FORMAT fif = FIF_TIFF;
		FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBF, w, h, /*FIT_RGBF*/3 * 4);
		FIRGBF* sl_data = new FIRGBF[w];
		for (unsigned int j = 0; j < h; j++) {
			// write scanline
			FIRGBF* sl;
			if (!flipY)	sl = (FIRGBF *)FreeImage_GetScanLine(dib, h - 1 - j);
			else sl = (FIRGBF *)FreeImage_GetScanLine(dib, j);
			//if (j<50)
			for (unsigned int i = 0; i < w; i++) {
				sl[i].red = data[nChannels * (w*j + i) + 0];// 0xffff;
				sl[i].green = nChannels>1 ? data[nChannels * (w*j + i) + 1] : 0.0f;//0xffff;
				sl[i].blue = nChannels>2 ? data[nChannels * (w*j + i) + 2] : 0.0f;//0.0f;// 0xffff;
			}
		}
		delete[] sl_data;
		//bool r = FreeImage_Save(FIF_TIFF, dib, filename.c_str(), /*TIFF_NONE*/TIFF_DEFAULT) == 1;
		bool r = FreeImage_Save(FIF_TIFF, dib, filename.c_str(), TIFF_NONE) == 1;

		FreeImage_Unload(dib);
		return r;
	}
	else if (ext == ".exr" || ext == ".EXR")
	{
		//FREE_IMAGE_FORMAT exrFormat = FreeImage_GetFIFFromFilename("dummy.exr");
		//std::cout << "exrFormat:" << exrFormat << std::endl;
		//std::cout << "FIF_EXR:" << FIF_EXR << std::endl;


		//FREE_IMAGE_FORMAT fif = FIF_TIFF;
		FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBF, w, h, /*FIT_RGBF*/3*4);
		FIRGBF* sl_data = new FIRGBF[w];
		for (unsigned int j = 0; j < h; j++) {
			// write scanline
			FIRGBF* sl;
			if (!flipY)	sl = (FIRGBF *)FreeImage_GetScanLine(dib, h - 1 - j);
			else sl = (FIRGBF *)FreeImage_GetScanLine(dib, j);
			//if (j<50)
			for (unsigned int i = 0; i < w; i++) {
				sl[i].red = data[nChannels * (w*j + i) + 0];// 0xffff;
				sl[i].green = nChannels>1? data[nChannels * (w*j + i) + 1] : 0.0f;//0xffff;
				sl[i].blue  = nChannels>2 ? data[nChannels * (w*j + i) + 2] : 0.0f;//0.0f;// 0xffff;
			}
		}
		delete[] sl_data;
		//bool r = FreeImage_Save(FIF_TIFF, dib, filename.c_str(), /*TIFF_NONE*/TIFF_DEFAULT) == 1;
		//bool r = FreeImage_Save(FIF_TIFF, dib, filename.c_str(), TIFF_NONE) == 1;
		bool r = FreeImage_Save(FIF_EXR, dib, filename.c_str(), EXR_FLOAT) == 1;

		FreeImage_Unload(dib);
		return r;
	}
	else
	{
		FREE_IMAGE_FORMAT fif = FIF_PNG;
		FIBITMAP *dib = FreeImage_Allocate(w, h, 24);
		RGBQUAD color;
		for (unsigned int j = 0; j < h; j++) {
			for (unsigned int i = 0; i < w; i++) {
				unsigned char col[3] = { 0, 0, 0 };

				for (unsigned int c = 0; c < nChannels && c < 3; ++c)
				{
					//col[c] = std::min(std::max(0, (int)(255.0f*data[nChannels * (w*j + i) + c])), 255);
					col[c] = std::min(std::max(0, (int)(255.0f*data[nChannels * (w*j + i) + c])), 255);
				}

				color.rgbRed = col[0];
				color.rgbGreen = col[1];
				color.rgbBlue = col[2];
				if (!flipY)	FreeImage_SetPixelColor(dib, i, h - 1 - j, &color);
				else		FreeImage_SetPixelColor(dib, i, j, &color);
			}
		}
		bool r = FreeImage_Save(fif, dib, filename.c_str(), 0) == 1;
		FreeImage_Unload(dib);
		return r;
	}
}
