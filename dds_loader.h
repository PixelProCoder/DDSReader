#ifndef DDS_READER_H
#define DDS_READER_H

#include <stdint.h>
#include <dxgiformat.h>
#include <fstream>

struct SubResourceData
{
	void* initData;
	unsigned int memPitch;
	unsigned int memSlicePitch;
};

#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
((unsigned int) ch3 << 24 | (unsigned int) ch2 << 16 | (unsigned int) ch1 << 8 | (unsigned int) ch0)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

constexpr unsigned int DDS_MAGIC_NUMBER = 0x20534444;

enum DDSFormatType
{
	DDS_FORMAT_TYPE_ALPHA = 0x2,
	DDS_FORMAT_TYPE_FOURCC = 0x4,
	DDS_FORMAT_TYPE_RGB = 0x40,
	DDS_FORMAT_TYPE_RGBA = 0x41,	// DDS_FORMAT_TYPE_RGB + ALPHAPIXELS (0x1)
	DDS_FORMAT_TYPE_YUV = 0x200,
	DDS_FORMAT_TYPE_LUMINANCE = 0x20000  // Supports only a single channel. Will add support for alpha channel in the future.
};

enum ResourceDimension
{
	RESOURCE_DIMENSION_UNKNOWN,
	RESOURCE_DIMENSION_BUFFER,
	RESOURCE_DIMENSION_TEXTURE1D,
	RESOURCE_DIMENSION_TEXTURE2D,
	RESOURCE_DIMENSION_TEXTURE3D
};

enum DDSResourceMisc
{
	DDS_RESOURCE_MISC_TEXTURECUBE = 0x4
};

enum DDSHeaderFlags
{
	DDS_HEADER_FLAGS_PITCH = 0x8,
	DDS_HEADER_FLAGS_TEXTURE = 0x1007,
	DDS_HEADER_FLAGS_MIPMAP = 0x20000,
	DDS_HEADER_FLAGS_LINEARSIZE = 0x80000,
	DDS_HEADER_FLAGS_VOLUME = 0x800000,
};

enum DDSSurfaceFlags
{
	DDS_SURFACE_FLAGS_COMPLEX = 0x8,
	DDS_SURFACE_FLAGS_TEXTURE = 0x1000,
	DDS_SURFACE_FLAGS_MIPMAP = 0x400008
};

enum DDSAdditionalFlags
{
	DDS_ADDITIONAL_FLAGS_CUBEMAP_POSITIVEX = 0x600,
	DDS_ADDITIONAL_FLAGS_CUBEMAP_NEGATIVEX = 0xA00,
	DDS_ADDITIONAL_FLAGS_CUBEMAP_POSITIVEY = 0x1200,
	DDS_ADDITIONAL_FLAGS_CUBEMAP_NEGATIVEY = 0x2200,
	DDS_ADDITIONAL_FLAGS_CUBEMAP_POSITIVEZ = 0x4200,
	DDS_ADDITIONAL_FLAGS_CUBEMAP_NEGATIVEZ = 0x8200,
	DDS_ADDITIONAL_FLAGS_VOLUME = 0x200000
};

struct DDSPixelFormat
{
	unsigned int size;
	unsigned int flags;
	unsigned int fourCC;
	unsigned int rgbBitCount;
	unsigned int rBitMask;
	unsigned int gBitMask;
	unsigned int bBitMask;
	unsigned int aBitMask;
};

// Describes a DDS file header.
struct DDSHeader
{
	unsigned int size;
	unsigned int flags;
	unsigned int height;
	unsigned int width;
	unsigned int pitchOrLinearSize;
	unsigned int depth;
	unsigned int mipMapCount;
	unsigned int reserved1[11];  // Unused.
	DDSPixelFormat ddspf;
	unsigned int caps;
	unsigned int caps2;
	unsigned int caps3;      // Unused.
	unsigned int caps4;      // Unused.
	unsigned int reserved2;  // Unused.
};

struct DDSHeaderDXT10
{
	DXGI_FORMAT format;
	ResourceDimension resourceDimension;
	DDSResourceMisc miscFlag;
	unsigned int arraySize;
	unsigned int miscFlags2;
};

inline void LoadDDSTextureFromFile(const char* fileName)
{
	std::ifstream file(fileName, std::ifstream::in);

	DDSHeader* header = nullptr;
	DDSHeaderDXT10* dx10Header = nullptr;
	uint8_t* surfaceData = nullptr;

	if (DecodeHeader(nullptr, 0, &header, &dx10Header, &surfaceData))
	{
		unsigned int numTextures = 1;
		if (dx10Header != nullptr)
		{
			numTextures = dx10Header->arraySize;
		}

		// Total array size: numTextures * numMipMaps per each texture
		SubResourceData* subresources = new SubResourceData[numTextures * header->mipMapCount];

		GetInitData(*header, numTextures, subresources, surfaceData, GetDXGIFormat(header->ddspf));
	};

}

inline bool DecodeHeader(uint8_t* fileData, size_t file_size, DDSHeader** ddsHeader, DDSHeaderDXT10** dxt10Header, uint8_t** ddsData)
{
	if (fileData == nullptr || ddsHeader == nullptr || dxt10Header == nullptr || ddsData == nullptr || file_size < sizeof(unsigned int) + sizeof(DDSHeader))
	{
		return false;
	}

	// First 4 bytes contain the magic number 'DDS' (0x20534444).
	const unsigned int magic_number = *reinterpret_cast<unsigned int*>(fileData);

	if (magic_number != DDS_MAGIC_NUMBER)
	{
		// Not a valid DDS texture.
		return false;
	}

	DDSHeader* header = reinterpret_cast<DDSHeader*>(fileData + sizeof(unsigned int));

	if (header == nullptr)
	{
		return false;
	}

	*ddsHeader = header;
	*ddsData = fileData + header->size;

	// Check if the DDS file contains the additional DXT10 structure.
	if (header->ddspf.flags & DDS_FORMAT_TYPE_FOURCC && header->ddspf.fourCC == MAKEFOURCC('D', 'X', '1', '0'))
	{
		// A valid texture should have a total file size of at least 148 bytes, including the DX10 header structure.
		if (file_size < sizeof(unsigned int) + sizeof(DDSHeader) + sizeof(DDSHeaderDXT10))
		{
			// Invalid texture
			return false;
		}

		*dxt10Header = (DDSHeaderDXT10*)*ddsData;
		*ddsData += sizeof(DDSHeaderDXT10);
	}

	return true;
}

inline bool IsBitMask(const DDSPixelFormat& ddspf, const unsigned int rBitMask, const unsigned int gBitMask, const unsigned int bBitMask, const unsigned int aBitMask)
{
	return ddspf.rBitMask == rBitMask && ddspf.gBitMask == gBitMask && ddspf.bBitMask == bBitMask && ddspf.aBitMask == aBitMask;
}

inline DXGI_FORMAT GetDXGIFormat(DDSPixelFormat& ddspf)
{
	// If the ddspx flags field contains 'DDS_FORMAT_TYPE_RGB' or 'DDS_FORMAT_TYPE_LUMINANCE' or 'DDS_FORMAT_TYPE_YUV', 
	// then the texture contains uncompressed data.
	if (ddspf.flags & DDS_FORMAT_TYPE_RGBA || ddspf.flags & DDS_FORMAT_TYPE_RGB)
	{
		switch (ddspf.rgbBitCount)
		{
		case 32:
		{
			if (IsBitMask(ddspf, 0xFF, 0xFF00, 0xFF0000, 0xFF000000))
			{
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}

			if (IsBitMask(ddspf, 0xFFFF, 0xFFFF0000, 0, 0))
			{
				return DXGI_FORMAT_R16G16_UNORM;
			}

			if (IsBitMask(ddspf, 0x3FF, 0xFFC00, 0x3FF00000, 0x0))
			{
				return DXGI_FORMAT_R10G10B10A2_UNORM;
			}

			if (IsBitMask(ddspf, 0xFF0000, 0xFF00, 0xFF, 0xFF000000))
			{
				return DXGI_FORMAT_B8G8R8A8_UNORM; // D3DFMT_A8R8G8B8
			}

			if (IsBitMask(ddspf, 0x3FF00000, 0xFFC00, 0x3FF, 0xC0000000))
			{
				return; // Not available
			}
		}
		break;
		case 16:
		{
			if (IsBitMask(ddspf, 0x7c00, 0x3e0, 0x1f, 0x8000))
			{
				return DXGI_FORMAT_B5G5R5A1_UNORM;
			}

			if (IsBitMask(ddspf, 0x7c00, 0x3e0, 0x1f, 0x0))
			{
				return; // Not available
			}

			if (IsBitMask(ddspf, 0xf00, 0xf0, 0xf, 0xf000))
			{
				return DXGI_FORMAT_B4G4R4A4_UNORM; // D3DFMT_A4R4G4B4
			}

			if (IsBitMask(ddspf, 0xf00, 0xf0, 0xf, 0xf000))
			{
				return DXGI_FORMAT_B4G4R4A4_UNORM; // D3DFMT_A4R4G4B4
			}
		}
		break;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}
	else if (ddspf.flags & DDS_FORMAT_TYPE_RGB)
	{
		switch (ddspf.rgbBitCount)
		{
		case 32:
		{
			if (IsBitMask(ddspf, 0xffff, 0xffff0000, 0x0, 0x0))
			{
				return DXGI_FORMAT_R16G16_UNORM;
			}
		}
		break;
		case 16:
		{
			if (IsBitMask(ddspf, 0xf800, 0x7e0, 0x1f, 0x0))
			{
				return DXGI_FORMAT_B5G6R5_UNORM;
			}
		}
		break;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	} 
	else if (ddspf.flags & DDS_FORMAT_TYPE_LUMINANCE)
	{
		switch (ddspf.rgbBitCount)
		{
		case 16:
		{
			if (IsBitMask(ddspf, 0xffff, 0x0, 0x0, 0x0))
			{
				return DXGI_FORMAT_R16_UNORM; // D3DFMT_L16
			}
		}
		break;
		case 8:
		{
			if (IsBitMask(ddspf, 0xff, 0x0, 0x0, 0x0))
			{
				return DXGI_FORMAT_R8_UNORM; // D3DFMT_L8
			}
		}
		break;
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}
	else if (ddspf.flags & DDS_FORMAT_TYPE_FOURCC)
	{
		// Texture contains compressed RGB data.
		switch (ddspf.fourCC)
		{
		case MAKEFOURCC('D', 'X', 'T', '1'):
			return DXGI_FORMAT_BC1_UNORM;
			break;
		case MAKEFOURCC('D', 'X', 'T', '2'):
		case MAKEFOURCC('D', 'X', 'T', '3'):
			return DXGI_FORMAT_BC2_UNORM;
			break;
		case MAKEFOURCC('D', 'X', 'T', '4'):
		case MAKEFOURCC('D', 'X', 'T', '5'):
			return DXGI_FORMAT_BC3_UNORM;
			break;
			// Legacy compression formats.
		case MAKEFOURCC('B', 'C', '4', 'U'):
		case MAKEFOURCC('A', 'T', 'I', '1'):
			return DXGI_FORMAT_BC4_UNORM;
			break;
		case MAKEFOURCC('B', 'C', '4', 'S'):
			return DXGI_FORMAT_BC4_SNORM;
			break;
		case MAKEFOURCC('A', 'T', 'I', '2'):
			return DXGI_FORMAT_BC5_UNORM;
			break;
		case MAKEFOURCC('B', 'C', '5', 'S'):
			return DXGI_FORMAT_BC5_SNORM;
			break;
		case MAKEFOURCC('R', 'G', 'B', 'G'):
			return DXGI_FORMAT_R8G8_B8G8_UNORM;
			break;
		case MAKEFOURCC('G', 'R', 'G', 'B'):
			return DXGI_FORMAT_G8R8_G8B8_UNORM;
			break;
		case 36:
			return DXGI_FORMAT_R16G16B16A16_UNORM;
			break;
		case 110:
			return DXGI_FORMAT_R16G16B16A16_SNORM;
			break;
		case 111:
			return DXGI_FORMAT_R16_FLOAT;
			break;
		case 112:
			return DXGI_FORMAT_R16G16_FLOAT;
			break;
		case 113:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case 114:
			return DXGI_FORMAT_R32_FLOAT;
			break;
		case 115:
			return DXGI_FORMAT_R32G32_FLOAT;
			break;
		case 116:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		default:
			return DXGI_FORMAT_UNKNOWN;
			break;
		}
	}

	return DXGI_FORMAT_UNKNOWN;
}

inline void GetInitData(DDSHeader& header, uint32_t numTextures, SubResourceData* initData, uint8_t* ddsData, DXGI_FORMAT format)
{
	unsigned int scanLineSize = 0;
	unsigned int slicePitch = 0;
	unsigned int numBytes = 0;
	unsigned int blockSize = 0; // Block size in bytes.

	switch (format)
	{
	case DXGI_FORMAT_BC1_UNORM:		 // DXT1 / BC1
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_UNORM:		 // BC4U
	case DXGI_FORMAT_BC4_SNORM:		 // BC4S
		blockSize = 8;
		break;
	case DXGI_FORMAT_BC2_UNORM:		 // DXT2 / DXT3
	case DXGI_FORMAT_BC2_UNORM_SRGB: // 
	case DXGI_FORMAT_BC3_UNORM:		 // DXT4 / DXT5
	case DXGI_FORMAT_BC3_UNORM_SRGB: // 
	case DXGI_FORMAT_BC5_UNORM:		 // ATI2
	case DXGI_FORMAT_BC5_SNORM:		 // BC5S
		blockSize = 16;
		break;
	default:
		break;
	}

	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int depth = 0;

	unsigned int index = 0;

	for (unsigned int i = 0; i < numTextures; ++i)
	{
		width = header.width;
		height = header.height;
		depth = header.depth;

		for (unsigned int j = 0; j < header.mipMapCount; ++j)
		{
			// This will recalculate the pitch of the highest mip-map level as well.
			if (header.flags & DDS_HEADER_FLAGS_LINEARSIZE)
			{
				// compressed textures
				scanLineSize = MAX(1, ((width + 3u) / 4u)) * blockSize;
				unsigned int numScanLines = MAX(1, ((height + 3u) / 4u));
				numBytes = scanLineSize * numScanLines;
			}
			else if (header.flags & DDS_HEADER_FLAGS_PITCH)
			{
				// uncompressed data
				scanLineSize = (width * header.ddspf.rgbBitCount + 7u) / 8u;
				numBytes = scanLineSize * height;
			}

			initData[index].initData = ddsData;
			initData[index].memPitch = scanLineSize;
			initData[index].memSlicePitch = slicePitch;

			++index;

			// Calculate the width and height of each subsequent mip-map level.
			width >>= 1;
			height >>= 1;
			depth >>= 1;

			if (width == 0)
			{
				width = 1;
			}

			if (height == 0)
			{
				height = 1;
			}

			ddsData += numBytes;
		}
	}
}

#endif
