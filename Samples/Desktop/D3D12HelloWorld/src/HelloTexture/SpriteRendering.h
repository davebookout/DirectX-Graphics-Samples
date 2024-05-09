#pragma once
#include <string>
#include <vector>
#include "Texture.h"

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

void LoadTest(SpriteTest& test);
bool LoadFont(std::vector<Texture>& textures);

void PrintTestParams(SpriteTest& test);

class SpriteRenderer {

public:
	void LoadAssets(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, std::wstring assetPath);
	void CreatePipelineState(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, std::wstring assetPath);
	void PopulateCommandList(ID3D12GraphicsCommandList* pCommandList);
	void LoadAssetsComplete();
	void OnDestroy();

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

	std::vector<ID3D12Resource*> mFreeList;
};
