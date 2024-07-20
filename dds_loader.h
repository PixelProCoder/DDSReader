#pragma once

#include <stdint.h>

#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
((uint32_t) ch3 << 24 | (uint32_t) ch2 << 16 | (uint32_t) ch1 << 8 | (uint32_t) ch0)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

constexpr uint32_t DDS_MAGIC_NUMBER = 0x20534444;

enum DdsFormatType
{
  DDS_FORMAT_TYPE_ALPHA = 0x2,
  DDS_FORMAT_TYPE_FOURCC = 0x4,
  DDS_FORMAT_TYPE_RGB = 0x40,
  DDS_FORMAT_TYPE_RGBA = 0x41,
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

enum DdsResourceMisc
{
  DDS_RESOURCE_MISC_TEXTURECUBE = 0x4
};

enum DdsHeaderFlags
{
  DDS_HEADER_FLAGS_PITCH = 0x8,
  DDS_HEADER_FLAGS_TEXTURE = 0x1007,
  DDS_HEADER_FLAGS_MIPMAP = 0x20000,
  DDS_HEADER_FLAGS_LINEARSIZE = 0x80000,
  DDS_HEADER_FLAGS_VOLUME = 0x800000,
};

enum DdsSurfaceFlags
{
  DDS_SURFACE_FLAGS_COMPLEX = 0x8,
  DDS_SURFACE_FLAGS_TEXTURE = 0x1000,
  DDS_SURFACE_FLAGS_MIPMAP = 0x400008
};

enum DdsAdditionalFlags
{
  DDS_ADDITIONAL_FLAGS_CUBEMAP_POSITIVEX = 0x600,
  DDS_ADDITIONAL_FLAGS_CUBEMAP_NEGATIVEX = 0xA00,
  DDS_ADDITIONAL_FLAGS_CUBEMAP_POSITIVEY = 0x1200,
  DDS_ADDITIONAL_FLAGS_CUBEMAP_NEGATIVEY = 0x2200,
  DDS_ADDITIONAL_FLAGS_CUBEMAP_POSITIVEZ = 0x4200,
  DDS_ADDITIONAL_FLAGS_CUBEMAP_NEGATIVEZ = 0x8200,
  DDS_ADDITIONAL_FLAGS_VOLUME = 0x200000
};

typedef struct DDS_PIXELFORMAT
{
  uint32_t size;
  uint32_t flags;
  uint32_t fourCC;
  uint32_t rgbBitCount;
  uint32_t rBitMask;
  uint32_t gBitMask;
  uint32_t bBitMask;
  uint32_t aBitMask;
} DDS_PIXELFORMAT;

typedef struct DDS_HEADER
{
  uint32_t size;
  uint32_t flags;
  uint32_t height;
  uint32_t width;
  uint32_t pitchOrLinearSize;
  uint32_t depth;
  uint32_t mipMapCount;
  uint32_t reserved1[11];  // Unused.
  DDS_PIXELFORMAT ddspf;
  uint32_t caps;
  uint32_t caps2;
  uint32_t caps3;      // Unused.
  uint32_t caps4;      // Unused.
  uint32_t reserved2;  // Unused.
} DDS_HEADER;

typedef struct DDS_HEADER_DXT10
{
  DXGI_FORMAT format;
  ResourceDimension resourceDimension;
  uint32_t miscFlag;
  uint32_t arraySize;
  uint32_t miscFlags2;
} DDS_HEADER_DXT10;

inline bool DecodeHeader(uint8_t* fileData, size_t file_size, DDS_HEADER** ddsHeader, DDS_HEADER_DXT10** dxt10Header, uint8_t** ddsData)
{
  if (!fileData || !ddsHeader || !dxt10Header || !ddsData)
  {
    return false;
  }

  uint32_t magic_number = *reinterpret_cast<uint32_t*>(fileData);

  if (magic_number != DDS_MAGIC_NUMBER || file_size < sizeof(DDS_HEADER))
  {
    // Not a valid DDS texture.
    return false;
  }

  DDS_HEADER* header = reinterpret_cast<DDS_HEADER*>(fileData + sizeof(uint32_t));

  if (!header)
  {
    return false;
  }

  *ddsHeader = header;
  *ddsData = fileData + sizeof(uint32_t) + sizeof(DDS_HEADER);

  if (header->ddspf.flags & DDS_FORMAT_TYPE_FOURCC && header->ddspf.fourCC == MAKEFOURCC('D', 'X', '1', '0'))
  {
    if (file_size < sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10))
    {
      return false;
    }

    *dxt10Header = reinterpret_cast<DDS_HEADER_DXT10*>(header + 1);
    *ddsData += sizeof(DDS_HEADER_DXT10);
  }

  return true;
}

inline bool isBitMask(const DDS_PIXELFORMAT& ddspf, const uint32_t rBitMask, const uint32_t gBitMask, const uint32_t bBitMask, const uint32_t aBitMask)
{
  return ddspf.rBitMask == rBitMask && ddspf.gBitMask == gBitMask && ddspf.bBitMask == bBitMask && ddspf.aBitMask == aBitMask;
}

inline DXGI_FORMAT GetDdsDxgiFormat(DDS_PIXELFORMAT& ddspf)
{
  // Currently supports only basic dxgi formats.
  if (ddspf.flags & DDS_FORMAT_TYPE_RGBA)
  {
    switch (ddspf.rgbBitCount)
    {
      case 32:
      {
        if (isBitMask(ddspf, 0xFF, 0xFF00, 0xFF0000, 0xFF000000))
        {
          return DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        if (isBitMask(ddspf, 0xffff, 0xffff0000, 0x0, 0x0))
        {
          return DXGI_FORMAT_R16G16_UNORM;
        }

        if (isBitMask(ddspf, 0x3ff, 0xffc00, 0x3ff00000, 0x0))
        {
          return DXGI_FORMAT_R10G10B10A2_UNORM;
        }
      }
      break;
      case 16:
      {
        if (isBitMask(ddspf, 0x7c00, 0x3e0, 0x1f, 0x8000))
        {
          return DXGI_FORMAT_B5G5R5A1_UNORM;
        }
      }
      break;
      default:
        return DXGI_FORMAT_UNKNOWN;
    }
  }
  
  if (ddspf.flags & DDS_FORMAT_TYPE_RGB)
  {
    switch (ddspf.rgbBitCount)
    {
      case 32:
      {
        if (isBitMask(ddspf, 0xffff, 0xffff0000, 0x0, 0x0))
        {
          return DXGI_FORMAT_R16G16_UNORM;
        }
      }
      break;
      case 16:
      {
        if (isBitMask(ddspf, 0xf800, 0x7e0, 0x1f, 0x0))
        {
          return DXGI_FORMAT_B5G6R5_UNORM;
        }
      }
      break;
      default:
        return DXGI_FORMAT_UNKNOWN;
    }
  }

  if (ddspf.flags & DDS_FORMAT_TYPE_FOURCC)
  {
    switch (ddspf.fourCC)
    {
      case MAKEFOURCC('D', 'X', 'T', '1'):
        return DXGI_FORMAT_BC1_UNORM;
        break;
      case MAKEFOURCC('D', 'X', 'T', '3'):
        return DXGI_FORMAT_BC2_UNORM;
        break;
      case MAKEFOURCC('D', 'X', 'T', '5'):
        return DXGI_FORMAT_BC3_UNORM;
        break;
      // Legacy compression formats.
      case MAKEFOURCC('B', 'C', '4', 'U') || MAKEFOURCC('A', 'T', 'I', '1'):
        return DXGI_FORMAT_BC4_UNORM;
        break;
      case MAKEFOURCC('A', 'T', 'I', '2'):
        return DXGI_FORMAT_BC5_UNORM;
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

inline void GetInitData(DDS_HEADER& header, uint32_t arraySize, D3D11_SUBRESOURCE_DATA* initData, uint8_t* ddsData, DXGI_FORMAT format)
{
  uint32_t scanLineSize = 0;
  uint32_t slicePitch = 0;
  uint32_t numBytes = 0;
  uint32_t blockSize = 0; // Block size in bytes.

  switch (format)
  {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
      blockSize = 8;
      break;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
      blockSize = 16;
      break;
    default:
      break;
  }

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;

  uint32_t index = 0;

  for (uint32_t i = 0; i < arraySize; ++i)
  {
    width = header.width;
    height = header.height;
    depth = header.depth;

    for (uint32_t j = 0; j < header.mipMapCount; ++j)
    {
      // This will recalculate te pitch of the main image as well.
      if (header.flags & DDS_HEADER_FLAGS_LINEARSIZE)
      {
        // compressed textures
        scanLineSize = MAX(1, ((width + 3u) / 4u)) * blockSize;
        uint32_t numScanLines = MAX(1, ((height + 3u) / 4u));
        numBytes = scanLineSize * numScanLines;
      }
      else if (header.flags & DDS_HEADER_FLAGS_PITCH)
      {
        // uncompressed data
        scanLineSize = (width * header.ddspf.rgbBitCount + 7u) / 8u;
        numBytes = scanLineSize * height;
      }

      initData[index].pSysMem = ddsData;
      initData[index].SysMemPitch = scanLineSize;
      initData[index].SysMemSlicePitch = slicePitch;

      ++index;

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