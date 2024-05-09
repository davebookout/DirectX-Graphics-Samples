#include "stdafx.h"
#include "Texture.h"
#include "Allocator2D.h"
#include <d3d12.h>
#include <d3dx12.h>
#include <array>

using std::vector;
using std::array;

TextureAtlas CreateTextureArray(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, vector<ID3D12Resource*>& tempResources) {
	// array slices are same size -- find max size for slices
	// will need scale for uvs if not same size.
	TextureAtlas result;
	static int textureArrayCount = 0;
	textureArrayCount++;
	ID3D12Resource* pTexture = nullptr;
	int maxWidth = INT_MIN;
	int maxHeight = INT_MIN;
	int maxMips = 1;
	for (Texture& t : textures) {
		maxWidth = max(maxWidth, t.size[0]);
		maxHeight = max(maxHeight, t.size[1]);
		maxMips = max(maxMips, t.mips);
	}

	{
		ID3D12Resource* pTextureUploadHeap;
		vector<BYTE*> tempCopiesForResize;
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = maxMips;
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
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(pTexture, 0, textures.size() * maxMips);
		std::wstring textureatlasname = L"texture atlas" + std::to_wstring(textureArrayCount);
		pTexture->SetName(textureatlasname.c_str());
		
		// Create the GPU upload buffer.
		pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pTextureUploadHeap));
		std::wstring uploadheapname = L"Upload Heap" + std::to_wstring(textureArrayCount);
		pTextureUploadHeap->SetName(uploadheapname.c_str());
		if (pTexture != nullptr && pTextureUploadHeap != nullptr) {
			tempResources.push_back(pTextureUploadHeap);
			int rowPitch = maxWidth * 4;// t.channels;
			int slicePitch = rowPitch * maxHeight;
			vector<D3D12_SUBRESOURCE_DATA> subresourceData(textures.size()*maxMips);
			for (int i = 0; i < textures.size(); i++) {
				SampleParameters params;
				params.index = i;
				params.start[0] = 0.0f; params.start[1] = 0.0f;
				params.end[0] = (float)(textures[i].size[0] - 1) / maxWidth; params.end[1] = (float)(textures[i].size[1] - 1)/ maxHeight;//
				
				result.sampleParameters.push_back(params);
				for (int mip = 0; mip < textures[i].mipData.size(); mip++) {
					int subresource = i * maxMips + mip;
					subresourceData[subresource].pData = textures[i].mipData[mip].pData;
					subresourceData[subresource].RowPitch = textures[i].mipData[mip].rowPitch;
					subresourceData[subresource].SlicePitch = textures[i].mipData[mip].rowPitch * textures[i].mipData[mip].size[1];

					if (textures[i].mipData[mip].size[0] != (maxWidth >> mip) || textures[i].mipData[mip].size[1] != (maxHeight >> mip)) {
						BYTE* newData = (BYTE*)malloc(maxWidth * maxHeight * 4);
						CopyIntoImage(newData, maxWidth * 4, 0, 0, textures[i].mipData[0].pData, textures[i].size[0] * 4, textures[i].size[1]);
						subresourceData[i].pData = newData;
						tempCopiesForResize.push_back(newData);
					}
				}
			}
			UpdateSubresources(pCommandList, pTexture, pTextureUploadHeap, 0, 0, textures.size()*maxMips, subresourceData.data());
			pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize = textures.size();
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = maxMips;
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

TextureAtlas CreateTextureAtlas(vector<Texture>& textures, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, int baseTextureSize, vector<ID3D12Resource*>& tempResources) {

	Allocator2D allocator(baseTextureSize / 32);
	TextureAtlas result;
	result.height = baseTextureSize;
	result.width = baseTextureSize;
	result.slices = 1;
	int blockSizeTexels = baseTextureSize / 32;
	vector<SampleParameters> sampleParameters;
	
	for (Texture& t : textures) {
		int nextMipWidth = t.size[0] >> 1;
		int nextMipHeight = t.size[1] >> 1;
		int nextMip = 1;
		while (nextMipWidth > 1 && nextMipHeight > 1) {
			TextureData data;
			data.rowPitch = nextMipWidth * 4;
			data.size[0] = nextMipWidth;
			data.size[1] = nextMipHeight;
			data.pData = (BYTE*)malloc(data.rowPitch * data.size[1]);
			t.mipData.push_back(std::move(data));
			DownScale(t.mipData[nextMip - 1], t.mipData[nextMip], true);
			nextMipWidth = nextMipWidth >> 1;
			nextMipHeight = nextMipHeight >> 1;
			nextMip++;
		}
		t.mips = t.mipData.size();
	}
	// allocate mipmaps for texture atlas
	std::vector<Texture>atlasTexture(1);
	atlasTexture[0].channels = 4;
	atlasTexture[0].format = 0;
	atlasTexture[0].id = 0;
	atlasTexture[0].size[0] = baseTextureSize;
	atlasTexture[0].size[1] = baseTextureSize;
	for (int i = baseTextureSize; i > 1; i = i / 2) {
		TextureData d;
		d.rowPitch = i * atlasTexture[0].channels;
		d.size[0] = i;
		d.size[1] = i;
		d.pData = (BYTE*)malloc(d.rowPitch * d.size[1]);
		atlasTexture[0].mipData.push_back(std::move(d));
	}

	for (int i = 0; i < textures.size(); i++) {
		int width = textures[i].size[0];
		int height = textures[i].size[1];
		int blocksWidth = ceil(1.0 * width / blockSizeTexels);
		int blocksHeight = ceil(1.0 * height / blockSizeTexels);
		//int offset[2] = { 0, 0 };
		AllocationID allocId = allocator.Allocate(blocksWidth, blocksHeight);
		if (!allocId.Valid()) {			
			result.sampleParameters.push_back({});
		}
		else {
			SampleParameters p;			
			p.index = 0;
			p.start[0] = (float)blockSizeTexels * allocId.offsetX / baseTextureSize;
			p.start[1] = (float)blockSizeTexels * allocId.offsetY / baseTextureSize;
			p.end[0] = (float)(blockSizeTexels * allocId.offsetX + width - 1) / baseTextureSize;
			p.end[1] = (float)(blockSizeTexels * allocId.offsetY + height - 1) / baseTextureSize;

			int dstPitch = 4 * baseTextureSize;
			sampleParameters.push_back(p);
			for (int mip = 0; mip < textures[i].mipData.size(); mip++) {				
				int offsetX = (allocId.offsetX * blockSizeTexels * textures[0].channels) >> mip;
				int offsetY = (blockSizeTexels * allocId.offsetY) >> mip;
				CopyIntoImage(atlasTexture[0].mipData[mip].pData, 
					atlasTexture[0].mipData[mip].rowPitch, 
					offsetX, offsetY, 
					textures[i].mipData[mip].pData, 
					textures[i].mipData[mip].rowPitch, 
					textures[i].mipData[mip].size[1]);

			}
		}
	}
	atlasTexture[0].mips = atlasTexture[0].mipData.size();
	TextureAtlas array = CreateTextureArray(atlasTexture, pDevice, pCommandList, pHeap, heapOffset, tempResources);
	result.pDescriptor = array.pDescriptor;
	result.pResource = array.pResource;
	result.sampleParameters = sampleParameters;
	return result;

}

ID3D12Resource* CreateStaticBuffer(BYTE* pData, int sizeInBytes, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12DescriptorHeap* pHeap, int heapOffset, vector<ID3D12Resource*>& tempResources) {

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

	CD3DX12_RANGE readRange(0, 0);
	BYTE* pMappedData = nullptr;
	result->Map(0, &readRange, reinterpret_cast<void**>(&pMappedData));
	CD3DX12_RANGE writtenRange(0, sizeInBytes);
	memcpy(pMappedData, pData, sizeInBytes);
	return result;
}


void DownScale(TextureData& src, TextureData& dst, bool zeroOneAlpha) {
	int blockSizeX = src.size[0] / dst.size[0];
	int blockSizeY = src.size[1] / dst.size[1];

	for (int bX = 0; bX < src.size[0] / blockSizeX; bX++) {
		for (int bY = 0; bY < src.size[1] / blockSizeY; bY++) {
			UINT block[] = { 0, 0, 0, 0 };
			int xOffset = bX * blockSizeX;
			int yOffset = bY * blockSizeY;
			for (int x = 0; x < blockSizeX; x++) {
				for (int y = 0; y < blockSizeY; y++) {
					int p = (x + xOffset) * 4 + (y + yOffset) * src.rowPitch;
					UINT alpha = (src.pData[p + 3]); ;
					block[0] += alpha * (src.pData[p]); 
					block[1] += alpha * (src.pData[p + 1]);
					block[2] += alpha * (src.pData[p + 2]);
					block[3] += alpha;// (src.pData[p + 2]);

				}
			}
			UINT alpha = block[3];	
			if (alpha > 0) {
				block[0] /= alpha;//(blockSizeX*blockSizeY);			
				block[1] /= alpha;//(blockSizeX*blockSizeY);
				block[2] /= alpha;//(blockSizeX*blockSizeY);
				block[3] /= (blockSizeX * blockSizeY);
			}
			int p = dst.rowPitch * bY + bX * 4;
			dst.pData[p]		= block[0];
			dst.pData[p + 1]	= block[1];
			dst.pData[p + 2]	= block[2];
			dst.pData[p + 3]	= block[3];
		}
	}
	// todo check edges
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
