#include "stdafx.h"

#include "AVIFWrapper.h"
#include "avif/avif.h"
#include "MaxImageDef.h"

struct AvifReader::avif_cache {
	avifDecoder* decoder;
	avifRGBImage rgb;
	uint8_t* data;
	size_t data_size;
};

AvifReader::avif_cache AvifReader::cache = { 0 };

void* AvifReader::ReadImage(int& width,
	int& height,
	int& nchannels,
	bool& has_animation,
	int frame_index,
	int& frame_count,
	int& frame_time,
	bool& outOfMemory,
	const void* buffer,
	int sizebytes)
{
	outOfMemory = false;
	width = height = 0;
	nchannels = 4;
	has_animation = false;
	unsigned char* pPixelData = NULL;

	if (cache.decoder == NULL) {
		cache.data = (uint8_t*)malloc(sizebytes);
		if (cache.data == NULL)
			return NULL;
		memcpy(cache.data, buffer, sizebytes);
		cache.decoder = avifDecoderCreate();
		avifResult result = avifDecoderSetIOMemory(cache.decoder, cache.data, sizebytes);
		if (result != AVIF_RESULT_OK) {
			DeleteCache();
			return NULL;
		}
		result = avifDecoderParse(cache.decoder);
		if (result != AVIF_RESULT_OK) {
			DeleteCache();
			return NULL;
		}
		cache.rgb = { 0 };
		
		cache.data_size = sizebytes;
	}
	
	if (avifDecoderNthImage(cache.decoder, frame_index) == AVIF_RESULT_OK) {
		// Now available (for this frame):
		// * All decoder->image YUV pixel data (yuvFormat, yuvPlanes, yuvRange, yuvChromaSamplePosition, yuvRowBytes)
		// * decoder->image alpha data (alphaPlane, alphaRowBytes)
		// * this frame's sequence timing

		avifRGBImageSetDefaults(&cache.rgb, cache.decoder->image);
		// Override YUV(A)->RGB(A) defaults here:
		//   depth, format, chromaUpsampling, avoidLibYUV, ignoreAlpha, alphaPremultiplied, etc.
		cache.rgb.depth = 8;
		cache.rgb.format = AVIF_RGB_FORMAT_BGRA;

		// Alternative: set rgb.pixels and rgb.rowBytes yourself, which should match your chosen rgb.format
		// Be sure to use uint16_t* instead of uint8_t* for rgb.pixels/rgb.rowBytes if (rgb.depth > 8)
		avifRGBImageAllocatePixels(&cache.rgb);

		if (avifImageYUVToRGB(cache.decoder->image, &cache.rgb) != AVIF_RESULT_OK) {
			DeleteCache();
		}

		// Now available:
		// * RGB(A) pixel data (rgb.pixels, rgb.rowBytes)

		if (cache.rgb.depth > 8) {
			// uint16_t* firstPixel = (uint16_t*)rgb.pixels;
			//printf(" * First pixel: RGBA(%u,%u,%u,%u)\n", firstPixel[0], firstPixel[1], firstPixel[2], firstPixel[3]);
		}
		else {
			uint8_t* firstPixel = cache.rgb.pixels;
			//printf(" * First pixel: RGBA(%u,%u,%u,%u)\n", firstPixel[0], firstPixel[1], firstPixel[2], firstPixel[3]);
		}
	} else {
		DeleteCache();
		return NULL;
	}
	width = cache.rgb.width;
	height = cache.rgb.height;
	has_animation = cache.decoder->imageCount > 1;
	frame_count = cache.decoder->imageCount;
	frame_time = cache.decoder->imageTiming.duration * 1000;

	uint8_t* pixels = cache.rgb.pixels;
	cache.rgb.pixels = NULL;
	cache.rgb.rowBytes = 0;
	return pixels;
}

void AvifReader::DeleteCache() {
	avifRGBImageFreePixels(&cache.rgb); // Only use in conjunction with avifRGBImageAllocatePixels()
	if (cache.decoder)
		avifDecoderDestroy(cache.decoder);
	free(cache.data);
	cache = { 0 };
}
