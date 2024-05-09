#pragma once
#include <vector>

// allocations are in "blocks"
// values are in "blocks" units
// interpreting block size is up to user of class
//  
struct Allocation2D {
	unsigned int offsetX;
	unsigned int offsetY;
	unsigned int width;
	unsigned int height;
	unsigned int sourceID;
	unsigned int ID;
	bool valid;
};

struct AllocationID {
	unsigned int page : 4;
	unsigned int level : 4;
	unsigned int offsetX : 12;
	unsigned int offsetY : 12;
	AllocationID() : page(-1), level(-1), offsetX(-1), offsetY(-1) {};
	AllocationID(unsigned int page, unsigned int level, unsigned int offsetX, unsigned int offsetY)
		: page(page), level(level), offsetX(offsetX), offsetY(offsetY) {};
	const bool Valid() {
		return page != -1 || level != -1 || offsetX != -1 || offsetY != -1;
	}
};

// Allocates 2D blocks
// constrained square and power of 2 for storage
// and allocation
// max size storage page size is 4096 "blocks"
class Allocator2D {
	int mDimension;
	unsigned int  mAllocatedPages;
	std::vector<std::vector<AllocationID>> mFree;
public:
	Allocator2D(unsigned int dimension);
	AllocationID Allocate(int blocksX, int blocksY);
	bool Free(unsigned int ID) {
		// not implemented
		return false;
	};
private:
	Allocator2D();
	void SplitAllocation(unsigned int);
};
