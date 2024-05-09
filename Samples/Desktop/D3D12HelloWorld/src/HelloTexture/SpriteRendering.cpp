#include "stdafx.h"
#include "SpriteRendering.h"
#include "stdio.h"
#include "fstream"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define _CRT_SECURE_NO_WARNINGS
#include "d3dx12.h"
#include <array>

/*
* todo:
*	add mip min -- kind of a hack because array ...
*   check uv range for precision issues -- use better end points to avoid artifacts.
*	texture allocator align to texture height & width
* 
// Notes
// Add mips & improve allocator
// verify padding and artifacts

// Make sprite renderer separate, just manages sprite/materials
// Text renderer can use sprite renderer
// separate out resource memory allocators to allow for clean up
// separate out loading/testing code
// separate out texture creation code
// allow atlas to allocate more array slices
// change "4"s to something more sensible texel_unit_size_bytes 
//		make sense for block compressed textures?
// For demo 
// add user input 
//	switch between rendering modes.. single/array/atlas -- add color to textures or something to differentiate
//	render atlas to show packing
//	render text/allow text input
//	some animation sprites
//	render lots of sprites
*/
using namespace std;

const int cbufferOffset = 0;
const int arrayOffset = 1;
const int atlasOffset = 2;
const int singleTextureOffset = 3;

//const char testFileName[] = "spriteTest01.txt";
//const char testFileName[] = "spriteTest02.txt";
const char testFileName[] = "spriteTest03.txt";





void SpriteRenderer::LoadAssets(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, std::wstring assetPath)
{
	//LoadFont(mData.textures);
	LoadTest(mData);
	pCommandList->Reset(mCommandAllocator, mPipelineState);
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 0 },
			{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 0 },
			{ "ROTATION", 0, DXGI_FORMAT_R32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 0 },
			{ "TEXTURE_ID", 0, DXGI_FORMAT_R32_SINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 0 },
		};

		const int MAX_SPRITES = 2048;
		const UINT vertexBufferSize = sizeof(Sprite) * MAX_SPRITES;// mData.sprites.size();
				
		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mSpriteBuffer));
		mSpriteBuffer->SetName(L"sprite instance buffer");
		// Copy the triangle data to the vertex buffer.
		CD3DX12_RANGE readRange(0, 0);        
		mSpriteBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mpVertexDataBegin));
		memcpy(mpVertexDataBegin, mData.sprites.data(), sizeof(Sprite) * mData.sprites.size());

		// Initialize the vertex buffer view.
		mSpriteBufferView.BufferLocation = mSpriteBuffer->GetGPUVirtualAddress();
		mSpriteBufferView.StrideInBytes = sizeof(Sprite);
		mSpriteBufferView.SizeInBytes = vertexBufferSize;
	}

	vector<TextureAtlas> singleTextures;
	int texOffset = 0;
	for (Texture& t : mData.textures) {
		vector<Texture> vt; vt.push_back(t);
		singleTextures.push_back(CreateTextureArray(vt, pDevice, pCommandList, mSRVHeap, singleTextureOffset + texOffset++, mFreeList));
	}

	SampleParameters* params = new SampleParameters[mData.textures.size() * 3];
	for (int i = 0; i < mData.textures.size(); i++) {
			
	}
	TextureAtlas array = CreateTextureArray(mData.textures, pDevice, pCommandList, mSRVHeap, arrayOffset, mFreeList);

	int baseTextureSize = 1024;
	TextureAtlas atlas = CreateTextureAtlas(mData.textures, pDevice, pCommandList, mSRVHeap, atlasOffset, baseTextureSize, mFreeList);
	ID3D12Resource* textureInfoBuffer = CreateStaticBuffer((BYTE*)(atlas.sampleParameters.data()), sizeof(SampleParameters) * mData.textures.size(), pDevice, pCommandList, mSRVHeap, cbufferOffset, mFreeList);
	pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { pCommandList };
	pCommandQueue->ExecuteCommandLists(1, ppCommandLists);

}

void SpriteRenderer::LoadAssetsComplete() {
	for (ID3D12Resource* pR : mFreeList) {
		pR->Release();
	}
	mFreeList.clear();
}
void SpriteRenderer::CreatePipelineState(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, std::wstring assetPath) {
	// Create the root signature.
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
				
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		//sampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ID3DBlob* signature;
		ID3DBlob* error;
		D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error);
		pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ID3DBlob* vertexShader;
		ID3DBlob* pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		HRESULT hr = S_OK;
		std::wstring fullPathShaderFile = assetPath + L"spriteshaders.hlsl";
		hr = D3DCompileFromFile(fullPathShaderFile.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr);
		hr = D3DCompileFromFile(fullPathShaderFile.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr);

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "ROTATION", 0, DXGI_FORMAT_R32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "TEXTURE_ID", 0, DXGI_FORMAT_R32_SINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = mRootSignature;
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState));
	}

	{
		// Describe and create a shader resource view (SRV) heap for the texture.
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 128;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		pDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSRVHeap));
		mSRVHeap->SetName(L"Sprite Renderer SRV heap");
		mHeapIncr = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	{
		pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocator));
	}
}

void SpriteRenderer::PopulateCommandList(ID3D12GraphicsCommandList* pCommandList)
{
	{
		// Set necessary state.
		pCommandList->SetGraphicsRootSignature(mRootSignature);
		ID3D12DescriptorHeap* ppHeaps[] = { mSRVHeap };
		//ID3D12DescriptorHeap* ppHeaps[] = { mSRVArrayHeap, mCBVHeap };
		pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		enum RenderPath {
			single,
			array,
			atlas
		};
		RenderPath renderPath = atlas;
		if (renderPath == single) {
			CD3DX12_GPU_DESCRIPTOR_HANDLE constants(mSRVHeap->GetGPUDescriptorHandleForHeapStart(), mHeapIncr * cbufferOffset);
			pCommandList->SetPipelineState(mPipelineState);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->IASetVertexBuffers(0, 1, &mSpriteBufferView);

			for (int i = 0; i < mData.offsets.size(); i++) {
				int offset = mData.offsets[i];
				int end = i + 1 < mData.offsets.size() ? mData.offsets[i + 1] : mData.sprites.size();
				int numSprites = end - offset;
				int textureID = mData.sprites[offset].textureID;
				CD3DX12_GPU_DESCRIPTOR_HANDLE textures(mSRVHeap->GetGPUDescriptorHandleForHeapStart(), mHeapIncr* (singleTextureOffset + textureID));
				pCommandList->SetGraphicsRootDescriptorTable(0, textures);
				pCommandList->SetGraphicsRootDescriptorTable(1, constants);
				pCommandList->DrawInstanced(3, numSprites, 0, offset);
			}
		}
		else {
			int textureOffset = renderPath == array ? arrayOffset : atlasOffset;
			CD3DX12_GPU_DESCRIPTOR_HANDLE textures(mSRVHeap->GetGPUDescriptorHandleForHeapStart(), mHeapIncr* textureOffset);
			CD3DX12_GPU_DESCRIPTOR_HANDLE constants(mSRVHeap->GetGPUDescriptorHandleForHeapStart(), mHeapIncr* cbufferOffset);

			pCommandList->SetPipelineState(mPipelineState);
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->IASetVertexBuffers(0, 1, &mSpriteBufferView);

			pCommandList->SetGraphicsRootDescriptorTable(0, textures);
			pCommandList->SetGraphicsRootDescriptorTable(1, constants);
			pCommandList->DrawInstanced(3, mData.sprites.size(), 0, 0);

		}
	}
}

void SpriteRenderer::OnDestroy() {
	LoadAssetsComplete();
	mCommandAllocator->Release();
	mSpriteBuffer->Release();
	for (ID3D12Resource* p : mSpriteTextures) {
		p->Release();
	}
	mSRVHeap->Release();
	mRootSignature->Release();
	mPipelineState->Release();
//mCommandList->Release();
	
}
bool LoadFont(vector<Texture>& textures) {
	char filename[] = "font/A.png";
	for (char c = 'A'; c <= 'Z'; c++) {
		FILE* file = nullptr;
		filename[5] = c;
		fopen_s(&file, filename, "r");
		if (file == nullptr) {
			fprintf(stderr, "Error loading config file: %s", testFileName);
			return false;
		}
		stbi_set_flip_vertically_on_load(true);
		Texture t;
		sscanf_s(filename, "%s\n", &t.fileName, 256);
		int requestChannels = 4;
		stbi_uc* texData = stbi_load(t.fileName, &t.size[0], &t.size[1], &t.channels, requestChannels);
		t.format = 0;
		TextureData data;
		data.pData = texData;
		data.size[0] = t.size[0];
		data.size[1] = t.size[1];
		data.rowPitch = data.size[0] * 4;
		t.mipData.push_back(std::move(data));
		t.channels = 4;
		textures.push_back(std::move(t));
		fclose(file);
	}
	return true;
}


void PrintTestParams(SpriteTest& test) {
	for (const Sprite& s : test.sprites) {
		printf("Sprite: pos(%f, %f), size(%f, %f), rotation(%f), textureID(%d)\n", s.position[0], s.position[1], s.size[0], s.size[1], s.rotation, s.textureID);
	}
}



void LoadTest(SpriteTest& test) {
	FILE* file = nullptr;
	fopen_s(&file, testFileName, "r");
	if (file == nullptr) {
		fprintf(stderr, "Error loading config file: %s", testFileName);
		return;
	}
	int textureOffset = test.textures.size();
	int numTextures = 0;
	fscanf_s(file, "Textures %d\n", &numTextures);
	stbi_set_flip_vertically_on_load(true);
	for (int i = 0; i < numTextures; i++) {
		Texture t;
		fscanf_s(file, "%s\n", &t.fileName, 256);
		int requestChannels = 4;
		stbi_uc* texData = stbi_load(t.fileName, &t.size[0], &t.size[1], &t.channels, requestChannels);
		t.format = 0;
		TextureData data;
		data.pData = texData;
		data.size[0] = t.size[0];
		data.size[1] = t.size[1];
		data.rowPitch = data.size[0] * 4;
		t.mipData.push_back(std::move(data));
		t.channels = 4;
		test.textures.push_back(t);
	}

	int numSprites = 0;
	fscanf_s(file, "Sprites %d\n", &numSprites);
	int currentTextureID = -1;
	for (int i = 0; i < numSprites; i++) {
		Sprite s;
		fscanf_s(file, "%f %f %f %f %f %d\n", &s.position[0], &s.position[1], &s.size[0], &s.size[1], &s.rotation, &s.textureID);
		s.textureID = textureOffset + s.textureID;
		test.sprites.push_back(s);
		if (s.textureID != currentTextureID) {
			test.offsets.push_back(i);
			currentTextureID = s.textureID;
		}
	}
	int testTexID = 0;
	float size = 2.0 / test.textures.size();
	float x = -1.0 + size / 2;
	float y = 1 - (testTexID * size) - size / 2;
	int spritesPerTexture = 10;
	test.sprites.resize(spritesPerTexture * test.textures.size());
	int sprite = 0;
	for (Sprite& s : test.sprites) {
		s.textureID = testTexID;
		s.position[0] = x;
		s.position[1] = y;
		s.size[0] = size;
		s.size[1] = size;
		s.rotation = 45;
		size = size / 2;
		x = x + size + size / 2 ;
		
		sprite++;
		if (sprite % spritesPerTexture == 0) {
			testTexID++;
			size = 2.0 / test.textures.size();
			x = -1.0 + size / 2;
			y = 1 - (testTexID * size) - size / 2;
		}
	}
	fclose(file);
}

