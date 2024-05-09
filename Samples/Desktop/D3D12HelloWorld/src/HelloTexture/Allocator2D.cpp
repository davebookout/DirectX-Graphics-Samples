#include "stdafx.h"
#include "Allocator2D.h"

// tree of 2^n elements has n levels
// 4k texture is 2^12 -- 4 bits to store 12 levels
// 
// PPPP LLLL XXXX XXXX XXXX YYYY YYYY YYYY
// page, level, x offset, y offset.


struct Allocator2DImpl {
	std::vector<Allocation2D> free;
	std::vector<Allocation2D> used;
};

Allocator2D::Allocator2D() {

}

// Creates a new allocator dimension x dimension
// and adds a single dimension x dimension storage block.

Allocator2D::Allocator2D(unsigned int dimension) : mDimension(dimension), mAllocatedPages(0), mFree(1+ceil(log2(dimension))) {
	mFree[0].push_back({ 0, 0, 0, 0 });
	mAllocatedPages++;
}

AllocationID Allocator2D::Allocate(int blocksX, int blocksY) {//, bool alignToSize) {
	int maxDim = max(blocksX, blocksY);
	if (maxDim > mDimension) {
		return AllocationID();
	}
	int allocLevel = mFree.size() - 1 - static_cast<unsigned int>(ceil(log2(maxDim)));
	int smallestAvailable = allocLevel;
	while (mFree[smallestAvailable].empty() && smallestAvailable > 0) {
		smallestAvailable--;
	}
	if (smallestAvailable == 0 && mFree[smallestAvailable].empty()) {
		mFree[smallestAvailable].push_back({ mAllocatedPages, 0, 0, 0 });
		mAllocatedPages++;
	}
	while (smallestAvailable < allocLevel) {
		SplitAllocation(smallestAvailable);
		smallestAvailable++;
	}
	AllocationID newId = mFree[smallestAvailable].back();
	mFree[smallestAvailable].pop_back();
	return newId;
}

//	SplitAllocation splits an allocation at level into
//		4 allocations that it adds to the next level
void Allocator2D::SplitAllocation(unsigned int level) {
	AllocationID id = mFree[level].back();
	AllocationID newId = AllocationID(id);
	unsigned int newLevel = id.level+1;
	if (!(newLevel < mFree.size())) {
		return;
	}
	newId.level = newLevel;
	int offsetMod = 1 << (mFree.size() -1 - (newLevel));
	mFree[newLevel].push_back(newId);
	newId.offsetY += offsetMod;
	mFree[newLevel].push_back(newId);
	newId.offsetX += offsetMod;
	mFree[newLevel].push_back(newId);
	newId.offsetY -= offsetMod;
	mFree[newLevel].push_back(newId);

	mFree[level].pop_back();
}

