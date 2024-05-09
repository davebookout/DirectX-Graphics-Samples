#pragma once
#include <vector>
#include <d3d12.h>
#include <d3dx12.h>
struct TextureData {
	int size[2];
	int rowPitch;
	BYTE* pData;
};
struct Texture {
	int size[2];
	int channels;
	int format; // assuming 4 channel uncompressed
	int mips;
	unsigned int id;
	char fileName[256];
	std::vector<TextureData> mipData;
};

struct SampleParameters {
	float start[2] = { 1.0, 0.0 };
	float end[2] = { 0.0, 1.0 };
	int index = 0;
	int padding[3];
};

struct TextureAtlas {
	ID3D12Resource* pResource;
	D3D12_CPU_DESCRIPTOR_HANDLE pDescriptor;
	int width = 0;
	int height = 0;
	int slices = 0;
	std::vector<SampleParameters> sampleParameters;
};

// CreateTextureArray
// returns a TextureArray resource from the input textures.
// image size is the max width and max height of the provided textures
// each slice in the TextureArray maps to an input texture
// input textures are not scaled
// this is a special case of the texture atlas where only a single input image per array slice.
// for sample parameters, offsets are always 0, scale is relative to the max width, height
TextureAtlas CreateTextureArray(std::vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, std::vector<ID3D12Resource*>& tempResources);

// CreateTextureAtals
TextureAtlas CreateTextureAtlas(std::vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, int baseTextureSize, std::vector<ID3D12Resource*>& tempResources);
//TextureAtlas CreateTextureAtlas(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset);
ID3D12Resource* CreateStaticBuffer(BYTE* pData, int sizeInBytes, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, std::vector<ID3D12Resource*>& tempResources);

void CopyIntoImage(BYTE* dst, int dstPitch, int dstOffsetX, int dstOffsetY, BYTE* src, int srcPitch, int rows);
void DownScale(TextureData& src, TextureData& dst, bool zeroOneAlpha);

