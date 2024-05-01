#include "stdafx.h"
#include "SpriteRendering.h"
#include "stdio.h"
#include "fstream"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define _CRT_SECURE_NO_WARNINGS
#include "d3dx12.h"
#include <array>

using namespace std;

const int cbufferOffset = 0;
const int arrayOffset = 1;
const int atlasOffset = 2;
const int singleTextureOffset = 3;

//const char testFileName[] = "spriteTest01.txt";
const char testFileName[] = "spriteTest02.txt";


// CreateSingleTextures
// returns a vector of resources created from the input textures
// dead! only arrays!
//vector<ID3D12Resource*> CreateSingleTextures(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset);

// CreateTextureArray
// returns a TextureArray resource from the input textures.
// image size is the max width and max height of the provided textures
// each slice in the TextureArray maps to an input texture
// input textures are not scaled
// this is a special case of the texture atlas where only a single input image per array slice.
// for sample parameters, offsets are always 0, scale is relative to the max width, height
TextureAtlas CreateTextureArray(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset);

// CreateTextureAtals
TextureAtlas CreateTextureAtlas(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, int baseTextureSize = 2048);
//TextureAtlas CreateTextureAtlas(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset);
ID3D12Resource* CreateStaticBuffer(BYTE* pData, int sizeInBytes, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset);

void CopyIntoImage(BYTE* dst, int dstPitch, int dstOffsetX, int dstOffsetY, BYTE* src, int srcPitch, int rows);

void LoadTest(SpriteTest& test);
bool LoadFont(vector<Texture>& textures);

void PrintTestParams(SpriteTest& test);


void SpriteRenderer::LoadAssets(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, std::wstring assetPath)
{
	LoadFont(mData.textures);
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


		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mSpriteBuffer));

		// Copy the triangle data to the vertex buffer.
		//UINT8* mpVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		mSpriteBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mpVertexDataBegin));
		memcpy(mpVertexDataBegin, mData.sprites.data(), sizeof(Sprite) * mData.sprites.size());
		//mSpriteBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		mSpriteBufferView.BufferLocation = mSpriteBuffer->GetGPUVirtualAddress();
		mSpriteBufferView.StrideInBytes = sizeof(Sprite);
		mSpriteBufferView.SizeInBytes = vertexBufferSize;
	}

	vector<TextureAtlas> singleTextures;
	int texOffset = 0;
	for (Texture& t : mData.textures) {
		vector<Texture> vt; vt.push_back(t);
		singleTextures.push_back(CreateTextureArray(vt, pDevice, pCommandList, mSRVHeap, singleTextureOffset + texOffset++));
	}

	SampleParameters* params = new SampleParameters[mData.textures.size() * 3];
	for (int i = 0; i < mData.textures.size(); i++) {
			
	}
	TextureAtlas array = CreateTextureArray(mData.textures, pDevice, pCommandList, mSRVHeap, arrayOffset);
	//ID3D12Resource* textureInfoBuffer = CreateStaticBuffer((BYTE*)(array.sampleParameters.data()), sizeof(SampleParameters) * mData.textures.size(), pDevice, pCommandList, mSRVHeap, cbufferOffset);

	TextureAtlas atlas = CreateTextureAtlas(mData.textures, pDevice, pCommandList, mSRVHeap, atlasOffset);
	ID3D12Resource* textureInfoBuffer = CreateStaticBuffer((BYTE*)(atlas.sampleParameters.data()), sizeof(SampleParameters) * mData.textures.size(), pDevice, pCommandList, mSRVHeap, cbufferOffset);
	pCommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { pCommandList };
	pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
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
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
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
		RenderPath renderPath = single;
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


#if 0
vector<ID3D12Resource*> CreateSingleTextures(vector<Texture>& textures, ID3D12Device* pDevice,  ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset) {

	vector<ID3D12Resource*> result;
	for (Texture& t : textures)
	{
		ID3D12Resource* pTextureUploadHeap;

		ID3D12Resource* pTexture;
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = t.size[0];// TextureWidth;
		textureDesc.Height = t.size[1];// TextureHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&pTexture));
		const UINT64 uploadBufferSize = 2 * GetRequiredIntermediateSize(pTexture, 0, 1);

		// Create the GPU upload buffer.
		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pTextureUploadHeap));

		if (pTexture != nullptr && pTextureUploadHeap != nullptr) {
			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the Texture2D.
			//std::vector<UINT8> texture = GenerateTextureData();

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = t.pData;
			textureData.RowPitch = t.size[0] * t.channels;
			textureData.SlicePitch = textureData.RowPitch * t.size[1];

			UpdateSubresources(pCommandList, pTexture, pTextureUploadHeap, 0, 0, 1, &textureData);
			pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			int incrSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(pHeap->GetCPUDescriptorHandleForHeapStart(), incrSize * (heapOffset++));
			pDevice->CreateShaderResourceView(pTexture, &srvDesc, handle);

		}
		result.push_back(pTexture);
	}
	return result;

}
#endif

TextureAtlas CreateTextureArray(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset) {
	// array slices are same size -- find max size for slices
	// will need scale for uvs if not same size.
	TextureAtlas result;
	ID3D12Resource* pTexture = nullptr;
	int maxWidth = INT_MIN;
	int maxHeight = INT_MIN;
	for (Texture& t : textures) {
		maxWidth = max(maxWidth, t.size[0]);
		maxHeight = max(maxHeight, t.size[1]);
	}

	{
		ID3D12Resource* pTextureUploadHeap;
		vector<BYTE*> tempCopiesForResize;
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = maxWidth;
		textureDesc.Height = maxHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = textures.size();
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&pTexture));
		const UINT64 uploadBufferSize = textures.size() * GetRequiredIntermediateSize(pTexture, 0, 1);

		// Create the GPU upload buffer.
		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pTextureUploadHeap));

		if (pTexture != nullptr && pTextureUploadHeap != nullptr) {
			int rowPitch = maxWidth * 4;// t.channels;
			int slicePitch = rowPitch * maxHeight;
			vector<D3D12_SUBRESOURCE_DATA> subresourceData(textures.size());
			for (int i = 0; i < textures.size(); i++) {
				SampleParameters params;
				params.index = i;
				params.offsetX = 0; params.offsetY = 0;
				params.u[0] = 1.0 * textures[i].size[0] / maxWidth; params.u[1] = 0.0;
				params.v[0] = 0.0; params.v[1] = 1.0 * textures[i].size[1] / maxHeight;

				result.sampleParameters.push_back(params);
				subresourceData[i].pData = textures[i].pData;
				subresourceData[i].RowPitch = rowPitch;
				subresourceData[i].SlicePitch = slicePitch;

				if (textures[i].size[0] != maxWidth || textures[i].size[1] != maxHeight) {
					BYTE* newData = (BYTE*) malloc(maxWidth * maxHeight * 4);
					CopyIntoImage(newData, maxWidth * 4, 0, 0, textures[i].pData, textures[i].size[0] * 4, textures[i].size[1]);
					subresourceData[i].pData = newData;
					tempCopiesForResize.push_back(newData);
				}
			}
			UpdateSubresources(pCommandList, pTexture, pTextureUploadHeap, 0, 0, textures.size(), subresourceData.data());
			pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2DArray.ArraySize = textures.size();
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0;
			int incrSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			result.pDescriptor.ptr = pHeap->GetCPUDescriptorHandleForHeapStart().ptr + incrSize * heapOffset;
			pDevice->CreateShaderResourceView(pTexture, &srvDesc, result.pDescriptor);
		}
		for (BYTE* p : tempCopiesForResize) {
			free(p);
		}
		tempCopiesForResize.clear();
	}
	result.pResource = pTexture;
	return result;
}

void CopyIntoImage(BYTE* dst, int dstPitch, int dstOffsetX, int dstOffsetY, BYTE* src, int srcPitch, int rows) {
	int copySize = min(dstPitch - dstOffsetX, srcPitch);
	for (int copyRow = 0; copyRow < rows; copyRow++) {
		BYTE* copyDst = dst + dstPitch * (dstOffsetY + copyRow) + dstOffsetX;
		BYTE* copySrc = src + srcPitch * copyRow;
		memcpy(copyDst, copySrc, copySize);
	}
}

class Allocator2D {
	int mBlockSize;
	std::array<unsigned int, 32> mMemory;
	//keep it simple with 32x32 block allocation	
public:
	Allocator2D() : mBlockSize(32), mMemory(array<unsigned int, 32>()) {
		for (unsigned int& m : mMemory) {
			m = 0xFFFFFFFF;
		}
	};
	bool Available(int x, int y, int offset[2]) {
		if (x + offset[0] >= mBlockSize || y + offset[1] >= mBlockSize)
			return false;
		unsigned int mask = (1 << x) - 1;
		mask = mask << offset[0];
		for (int j = offset[1]; j < offset[1] + y; j++) {
			if ((mMemory[j] & mask) != mask) { 
				return false;
			}
		}
		return true;
	}

	void MarkUsed(int x, int y, int offset[2]) {
		unsigned int m = ~( ((1 << x) - 1) << offset[0]);
		for (int j = offset[1]; j < offset[1]+y; j++) {
			mMemory[j] = (mMemory[j] & m);
		}
	}

	bool Allocate(int blocksX, int blocksY, int outOffset[2]) {
		//just grab first top left available...
		unsigned int allocate = (1 << blocksX) - 1;
		outOffset[0] = 0; outOffset[1] = 0;
		while (outOffset[1] < mBlockSize) {
			if (Available(blocksX, blocksY, outOffset))
			{
				MarkUsed(blocksX, blocksY, outOffset);
				return true;
			}
			outOffset[1]++;
			if (outOffset[1] == mBlockSize) {
				if (outOffset[0] < mBlockSize - 1) {
					outOffset[0]++;
					outOffset[1] = 0;
				}
			}
		}
		return false;

	}


};

TextureAtlas CreateTextureAtlas(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, int baseTextureSize) {
	
	Allocator2D allocator;
	//allocator has 32x32 blocks to allocate
	//block size in texels will be
	TextureAtlas result;
	result.height = baseTextureSize;
	result.width = baseTextureSize;
	result.slices = 1;
	int blockSizeTexels = baseTextureSize / 32;
	vector<SampleParameters> sampleParameters;
	BYTE* pData = (BYTE*)malloc(baseTextureSize * baseTextureSize * 4);
	vector<int> unallocated;
	for (int i = 0; i < textures.size(); i++) {
		int width = textures[i].size[0];
		int height = textures[i].size[1];
		int blocksWidth = ceil(1.0 * width / blockSizeTexels);
		int blocksHeight = ceil(1.0 * height / blockSizeTexels);
		int offset[2] = { 0, 0 };
		if (!allocator.Allocate(blocksWidth, blocksHeight, offset)) {
			// need to allocate a new page, but ...
			unallocated.push_back(i);
			//break;
			result.sampleParameters.push_back({});
		}
		else {
			SampleParameters p;
			p.index = 0;
			p.nothing = 0;
			p.offsetX = (float)blockSizeTexels * offset[0] / baseTextureSize;
			p.offsetY = (float)blockSizeTexels * offset[1] / baseTextureSize;
			p.u[0] = (float)width / baseTextureSize; p.u[1] = 0;
			p.v[1] = (float)height / baseTextureSize; p.v[0] = 0;
			int dstPitch = 4 * baseTextureSize;
			sampleParameters.push_back(p);
			CopyIntoImage(pData, dstPitch, offset[0] * blockSizeTexels * 4, offset[1] * blockSizeTexels, textures[i].pData, textures[i].size[0] * 4, textures[i].size[1]);
		}
	}
	Texture t;
	t.channels = 4;
	t.format = 0;
	t.id = 0;
	t.pData = pData;
	t.size[0] = baseTextureSize;
	t.size[1] = baseTextureSize;
	vector<Texture> tv;
	tv.push_back(t);
	TextureAtlas array = CreateTextureArray(tv, pDevice, pCommandList, pHeap, heapOffset);
	result.pDescriptor = array.pDescriptor;
	result.pResource = array.pResource;
	result.sampleParameters = sampleParameters;
	return result;

}

ID3D12Resource* CreateStaticBuffer(BYTE* pData, int sizeInBytes, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset) {
	
	ID3D12Resource* result;
	int resourceSize = (sizeInBytes / 256 + 1) * 256;
	pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(resourceSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&result));

	// Describe and create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = result->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = resourceSize;
	int incrSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(pHeap->GetCPUDescriptorHandleForHeapStart(), incrSize * heapOffset);
	pDevice->CreateConstantBufferView(&cbvDesc, handle);

	// Map and initialize the constant buffer. We don't unmap this until the
	// app closes. Keeping things mapped for the lifetime of the resource is okay.
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	BYTE* pMappedData = nullptr;
	result->Map(0, &readRange, reinterpret_cast<void**>(&pMappedData));
	CD3DX12_RANGE writtenRange(0, sizeInBytes);
	memcpy(pMappedData, pData, sizeInBytes);
	//result->Unmap(0, &writtenRange);
	return result;
}

/*
// notes:
// render types
//  render sprites for each texture input
//      need start offsets and texture id
//  render all with texture array
//      textures all the same size
//      need uv scale, array index
//  render all with simple sprite atlas
//      need uv scale, (array index for _a_lot_ of textures)
//  render all with sprite atlas from tool
//      available tools
//          https://www.codeandweb.com/texturepacker  -- not expensive...
//          http://free-tex-packer.com/ -- free, MIT, on github, last check in 3+ years ago
*/


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
		t.pData = texData;
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
	fclose(file);
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
		t.pData = texData;
		t.channels = 4;
		textures.push_back(t);
		fclose(file);
	}
	return true;
}
void PrintTestParams(SpriteTest& test) {
	for (const Sprite& s : test.sprites) {
		printf("Sprite: pos(%f, %f), size(%f, %f), rotation(%f), textureID(%d)\n", s.position[0], s.position[1], s.size[0], s.size[1], s.rotation, s.textureID);
	}
}
