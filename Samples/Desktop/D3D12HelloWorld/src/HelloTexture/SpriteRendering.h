#pragma once
#include <string>
#include <vector>



struct SampleParameters {
	float offsetX = 0;
	float offsetY = 0;
	int index = 0;
	int nothing = 0;
	float u[2] = { 1.0, 0.0 };
	float v[2] = { 0.0, 1.0 };
};

struct TextureAtlas {
	ID3D12Resource* pResource;
	D3D12_CPU_DESCRIPTOR_HANDLE pDescriptor;
	int width = 0;
	int height = 0;
	int slices = 0;
	std::vector<SampleParameters> sampleParameters;
};

struct Texture {
	int size[2];
	int channels;
	int format; // assuming 4 channel uncompressed
	unsigned int id;
	char fileName[256];
	unsigned char* pData;
};

struct Sprite {
	float position[2];
	float size[2];
	float rotation;
	int textureID;
};

struct SpriteTest {
	std::vector<Sprite> sprites;
	std::vector<Texture> textures;
	std::vector<int> offsets;
};


class SpriteRenderer {

public:
	void LoadAssets(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, std::wstring assetPath);
	void CreatePipelineState(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, std::wstring assetPath);
	void PopulateCommandList(ID3D12GraphicsCommandList* pCommandList);

	SpriteTest mData;
	ID3D12Resource* mSpriteBuffer;
	UINT8* mpVertexDataBegin;
	D3D12_VERTEX_BUFFER_VIEW mSpriteBufferView;
	std::vector<ID3D12Resource*> mSpriteTextures;
	ID3D12DescriptorHeap* mSRVHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE mSRVStart;
	int mHeapIncr;

	ID3D12RootSignature* mRootSignature;
	ID3D12PipelineState* mPipelineState;
	ID3D12CommandAllocator* mCommandAllocator;
	ID3D12GraphicsCommandList* mCommandList;

};
void LoadTest(SpriteTest& test);
void PrintTestParams(SpriteTest& test);
