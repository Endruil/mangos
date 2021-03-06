//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
 
#ifndef RECAST_H
#define RECAST_H

// The units of the parameters are specified in parenthesis as follows:
// (vx) voxels, (wu) world units
struct rcConfig
{
	int width, height;				// Dimensions of the rasterized heighfield (vx)
	int tileSize;					// Width and Height of a tile (vx)
	int borderSize;					// Non-navigable Border around the heightfield (vx)
	float cs, ch;					// Grid cell size and height (wu)
	float bmin[3], bmax[3];			// Grid bounds (wu)
	float walkableSlopeAngle;		// Maximum walkble slope angle in degrees.
	int walkableHeight;				// Minimum height where the agent can still walk (vx)
	int walkableClimb;				// Maximum height between grid cells the agent can climb (vx)
	int walkableRadius;				// Radius of the agent in cells (vx)
	int maxEdgeLen;					// Maximum contour edge length (vx)
	float maxSimplificationError;	// Maximum distance error from contour to cells (vx)
	int minRegionSize;				// Minimum regions size. Smaller regions will be deleted (vx)
	int mergeRegionSize;			// Minimum regions size. Smaller regions will be merged (vx)
	int maxVertsPerPoly;			// Max number of vertices per polygon
	float detailSampleDist;			// Detail mesh sample spacing.
	float detailSampleMaxError;		// Detail mesh simplification max sample error.
};

// Heightfield span.
struct rcSpan
{
	unsigned int smin : 15;			// Span min height.
	unsigned int smax : 15;			// Span max height.
	unsigned int flags : 2;			// Span flags.
    unsigned char area;             // Span area type.
	rcSpan* next;					// Next span in column.
};

static const int RC_SPANS_PER_POOL = 2048;

// Memory pool used for quick span allocation.
struct rcSpanPool
{
	rcSpanPool* next;	// Pointer to next pool.
	rcSpan items[1];	// Array of spans (size RC_SPANS_PER_POOL).
};

// Dynamic span-heightfield.
struct rcHeightfield
{
	inline rcHeightfield() : width(0), height(0), spans(0), pools(0), freelist(0) {}
	inline ~rcHeightfield()
	{
		// Delete span array.
		delete [] spans;
		// Delete span pools.
		while (pools)
		{
			rcSpanPool* next = pools->next;
			delete [] reinterpret_cast<unsigned char*>(pools);
			pools = next;
		}
	}
	int width, height;			// Dimension of the heightfield.
	float bmin[3], bmax[3];		// Bounding box of the heightfield
	float cs, ch;				// Cell size and height.
	rcSpan** spans;				// Heightfield of spans (width*height).
	rcSpanPool* pools;			// Linked list of span pools.
	rcSpan* freelist;			// Pointer to next free span.
};

struct rcCompactCell
{
	unsigned int index : 24;	// Index to first span in column.
	unsigned int count : 8;		// Number of spans in this column.
};

struct rcCompactSpan
{
	unsigned short y;			// Bottom coordinate of the span.
	unsigned short reg;
	unsigned int con : 24;		// Connections to neighbour cells.
	unsigned int h : 8;			// Height of the span.
};

// Compact static heightfield. 
struct rcCompactHeightfield
{
	inline rcCompactHeightfield() :
		maxDistance(0), maxRegions(0), cells(0),
		spans(0), dist(0), /*regs(0),*/ areas(0) {}
	inline ~rcCompactHeightfield()
	{
		delete [] cells;
		delete [] spans;
		delete [] dist;
//		delete [] regs;
		delete [] areas;
	}
	int width, height;					// Width and height of the heighfield.
	int spanCount;						// Number of spans in the heightfield.
	int walkableHeight, walkableClimb;	// Agent properties.
	unsigned short maxDistance;			// Maximum distance value stored in heightfield.
	unsigned short maxRegions;			// Maximum Region Id stored in heightfield.
	float bmin[3], bmax[3];				// Bounding box of the heightfield.
	float cs, ch;						// Cell size and height.
	rcCompactCell* cells;				// Pointer to width*height cells.
	rcCompactSpan* spans;				// Pointer to spans.
	unsigned short* dist;				// Pointer to per span distance to border.
//	unsigned short* regs;				// Pointer to per span region ID.
	unsigned char* areas;				// Pointer to per span area ID.
};

struct rcContour
{
	inline rcContour() : verts(0), nverts(0), rverts(0), nrverts(0) { }
	inline ~rcContour() { delete [] verts; delete [] rverts; }
	int* verts;			// Vertex coordinates, each vertex contains 4 components.
	int nverts;			// Number of vertices.
	int* rverts;		// Raw vertex coordinates, each vertex contains 4 components.
	int nrverts;		// Number of raw vertices.
	unsigned short reg;	// Region ID of the contour.
	unsigned char area;	// Area ID of the contour.
};

struct rcContourSet
{
	inline rcContourSet() : conts(0), nconts(0) {}
	inline ~rcContourSet() { delete [] conts; }
	rcContour* conts;		// Pointer to all contours.
	int nconts;				// Number of contours.
	float bmin[3], bmax[3];	// Bounding box of the heightfield.
	float cs, ch;			// Cell size and height.
};

// Polymesh store a connected mesh of polygons.
// The polygons are store in an array where each polygons takes
// 'nvp*2' elements. The first 'nvp' elements are indices to vertices
// and the second 'nvp' elements are indices to neighbour polygons.
// If a polygona has less than 'bvp' vertices, the remaining indices
// are set to RC_MESH_NULL_IDX. If an polygon edge does not have a neighbour
// the neighbour index is set to RC_MESH_NULL_IDX.
// Vertices can be transformed into world space as follows:
//   x = bmin[0] + verts[i*3+0]*cs;
//   y = bmin[1] + verts[i*3+1]*ch;
//   z = bmin[2] + verts[i*3+2]*cs;
struct rcPolyMesh
{
	inline rcPolyMesh() : verts(0), polys(0), regs(0), flags(0), areas(0), nverts(0), npolys(0), nvp(3) {}

	inline ~rcPolyMesh() { delete [] verts; delete [] polys; delete [] regs; delete [] flags; delete [] areas; }
	
	unsigned short* verts;	// Vertices of the mesh, 3 elements per vertex.
	unsigned short* polys;	// Polygons of the mesh, nvp*2 elements per polygon.
	unsigned short* regs;	// Region ID of the polygons.
	unsigned short* flags;	// Per polygon flags.
	unsigned char* areas;	// Area ID of polygons.
	int nverts;				// Number of vertices.
	int npolys;				// Number of polygons.
	int nvp;				// Max number of vertices per polygon.
	float bmin[3], bmax[3];	// Bounding box of the mesh.
	float cs, ch;			// Cell size and height.
};

// Detail mesh generated from a rcPolyMesh.
// Each submesh represents a polygon in the polymesh and they are stored in
// excatly same order. Each submesh is described as 4 values:
// base vertex, vertex count, base triangle, triangle count. That is,
//   const unsigned char* t = &dtl.tris[(tbase+i)*3]; and
//   const float* v = &dtl.verts[(vbase+t[j])*3];
// If the input polygon has 'n' vertices, those vertices are first in the
// submesh vertex list. This allows to compres the mesh by not storing the
// first vertices and using the polymesh vertices instead.

struct rcPolyMeshDetail
{
	inline rcPolyMeshDetail() :
		meshes(0), verts(0), tris(0),
		nmeshes(0), nverts(0), ntris(0) {}
	inline ~rcPolyMeshDetail()
	{
		delete [] meshes; delete [] verts; delete [] tris;
	}
	
	unsigned short* meshes;	// Pointer to all mesh data.
	float* verts;			// Pointer to all vertex data.
	unsigned char* tris;	// Pointer to all triangle data.
	int nmeshes;			// Number of meshes.
	int nverts;				// Number of total vertices.
	int ntris;				// Number of triangles.
};


// Simple dynamic array ints.
class rcIntArray
{
	int* m_data;
	int m_size, m_cap;
public:
	inline rcIntArray() : m_data(0), m_size(0), m_cap(0) {}
	inline rcIntArray(int n) : m_data(0), m_size(0), m_cap(n) { m_data = new int[n]; }
	inline ~rcIntArray() { delete [] m_data; }
	void resize(int n);
	inline void push(int item) { resize(m_size+1); m_data[m_size-1] = item; }
	inline int pop() { if (m_size > 0) m_size--; return m_data[m_size]; }
	inline const int& operator[](int i) const { return m_data[i]; }
	inline int& operator[](int i) { return m_data[i]; }
	inline int size() const { return m_size; }
};

// Simple helper class to delete array in scope
template<class T> class rcScopedDelete
{
	T* ptr;
public:
	inline rcScopedDelete() : ptr(0) {}
	inline rcScopedDelete(T* p) : ptr(p) {}
	inline ~rcScopedDelete() { delete [] ptr; }
	inline operator T*() { return ptr; }
	inline T* operator=(T* p) { ptr = p; return ptr; }
};

enum rcSpanFlags
{
	RC_WALKABLE = 0x01,
	RC_LEDGE = 0x02,
};

// If heightfield region ID has the following bit set, the region is on border area
// and excluded from many calculations.
static const unsigned short RC_BORDER_REG = 0x8000;

// If contour region ID has the following bit set, the vertex will be later
// removed in order to match the segments and vertices at tile boundaries.
static const int RC_BORDER_VERTEX = 0x10000;

static const int RC_AREA_BORDER = 0x20000;

// Mask used with contours to extract region id.
static const int RC_CONTOUR_REG_MASK = 0xffff;

// Null index which is used with meshes to mark unset or invalid indices.
static const unsigned short RC_MESH_NULL_IDX = 0xffff;

// Area ID that is considered empty.
static const unsigned char RC_NULL_AREA = 0;

// Area ID that is considered generally walkable.
static const unsigned char RC_WALKABLE_AREA = 255;

// Value returned by rcGetCon() if the direction is not connected.
static const int RC_NOT_CONNECTED = 0x3f;

// Compact span neighbour helpers.
inline void rcSetCon(rcCompactSpan& s, int dir, int i)
{
	const unsigned int shift = (unsigned int)dir*6;
	unsigned int con = s.con;
	s.con = (con & ~(0x3f << shift)) | (((unsigned int)i & 0x3f) << shift);
}

inline int rcGetCon(const rcCompactSpan& s, int dir)
{
	const unsigned int shift = (unsigned int)dir*6;
	return (s.con >> shift) & 0x3f;
}

inline int rcGetDirOffsetX(int dir)
{
	const int offset[4] = { -1, 0, 1, 0, };
	return offset[dir&0x03];
}

inline int rcGetDirOffsetY(int dir)
{
	const int offset[4] = { 0, 1, 0, -1 };
	return offset[dir&0x03];
}

// Common helper functions
template<class T> inline void rcSwap(T& a, T& b) { T t = a; a = b; b = t; }
template<class T> inline T rcMin(T a, T b) { return a < b ? a : b; }
template<class T> inline T rcMax(T a, T b) { return a > b ? a : b; }
template<class T> inline T rcAbs(T a) { return a < 0 ? -a : a; }
template<class T> inline T rcSqr(T a) { return a*a; }
template<class T> inline T rcClamp(T v, T mn, T mx) { return v < mn ? mn : (v > mx ? mx : v); }

// Common vector helper functions.
inline void rcVcross(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[1]*v2[2] - v1[2]*v2[1];
	dest[1] = v1[2]*v2[0] - v1[0]*v2[2];
	dest[2] = v1[0]*v2[1] - v1[1]*v2[0]; 
}

inline float rcVdot(const float* v1, const float* v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

inline void rcVmad(float* dest, const float* v1, const float* v2, const float s)
{
	dest[0] = v1[0]+v2[0]*s;
	dest[1] = v1[1]+v2[1]*s;
	dest[2] = v1[2]+v2[2]*s;
}

inline void rcVadd(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[0]+v2[0];
	dest[1] = v1[1]+v2[1];
	dest[2] = v1[2]+v2[2];
}

inline void rcVsub(float* dest, const float* v1, const float* v2)
{
	dest[0] = v1[0]-v2[0];
	dest[1] = v1[1]-v2[1];
	dest[2] = v1[2]-v2[2];
}

inline void rcVmin(float* mn, const float* v)
{
	mn[0] = rcMin(mn[0], v[0]);
	mn[1] = rcMin(mn[1], v[1]);
	mn[2] = rcMin(mn[2], v[2]);
}

inline void rcVmax(float* mx, const float* v)
{
	mx[0] = rcMax(mx[0], v[0]);
	mx[1] = rcMax(mx[1], v[1]);
	mx[2] = rcMax(mx[2], v[2]);
}

inline void rcVcopy(float* dest, const float* v)
{
	dest[0] = v[0];
	dest[1] = v[1];
	dest[2] = v[2];
}

inline float rcVdist(const float* v1, const float* v2)
{
	float dx = v2[0] - v1[0];
	float dy = v2[1] - v1[1];
	float dz = v2[2] - v1[2];
	return sqrtf(dx*dx + dy*dy + dz*dz);
}

inline float rcVdistSqr(const float* v1, const float* v2)
{
	float dx = v2[0] - v1[0];
	float dy = v2[1] - v1[1];
	float dz = v2[2] - v1[2];
	return dx*dx + dy*dy + dz*dz;
}

inline void rcVnormalize(float* v)
{
	float d = 1.0f / sqrtf(rcSqr(v[0]) + rcSqr(v[1]) + rcSqr(v[2]));
	v[0] *= d;
	v[1] *= d;
	v[2] *= d;
}

inline bool rcVequal(const float* p0, const float* p1)
{
	static const float thr = rcSqr(1.0f/16384.0f);
	const float d = rcVdistSqr(p0, p1);
	return d < thr;
}


// Calculated bounding box of array of vertices.
// Params:
//	verts - (in) array of vertices
//	nv - (in) vertex count
//	bmin, bmax - (out) bounding box
void rcCalcBounds(const float* verts, int nv, float* bmin, float* bmax);

// Calculates grid size based on bounding box and grid cell size.
// Params:
//	bmin, bmax - (in) bounding box
//	cs - (in) grid cell size
//	w - (out) grid width
//	h - (out) grid height
void rcCalcGridSize(const float* bmin, const float* bmax, float cs, int* w, int* h);

// Creates and initializes new heightfield.
// Params:
//	hf - (in/out) heightfield to initialize.
//	width - (in) width of the heightfield.
//	height - (in) height of the heightfield.
//	bmin, bmax - (in) bounding box of the heightfield
//	cs - (in) grid cell size
//	ch - (in) grid cell height
bool rcCreateHeightfield(rcHeightfield& hf, int width, int height,
						 const float* bmin, const float* bmax,
						 float cs, float ch);

// Sets the WALKABLE flag for every triangle whose slope is below
// the maximun walkable slope angle.
// Params:
//	walkableSlopeAngle - (in) maximun slope angle in degrees.
//	verts - (in) array of vertices
//	nv - (in) vertex count
//	tris - (in) array of triangle vertex indices
//	nt - (in) triangle count
//	flags - (out) array of triangle flags
void rcMarkWalkableTriangles(const float walkableSlopeAngle,
							 const float* verts, int nv,
							 const int* tris, int nt,
							 unsigned char* flags); 

// Adds span to heighfield.
// The span addition can set to favor flags. If the span is merged to
// another span and the new smax is within 'flagMergeThr' units away
// from the existing span the span flags are merged and stored.
// Params:
//	solid - (in) heighfield where the spans is added to
//  x,y - (in) location on the heighfield where the span is added
//  smin,smax - (in) spans min/max height
//  flags - (in) span flags (zero or WALKABLE)
//  flagMergeThr - (in) merge threshold.
void rcAddSpan(rcHeightfield& solid, const int x, const int y,
			   const unsigned short smin, const unsigned short smax,
			   const unsigned short flags, const unsigned char area, const int flagMergeThr);

// Rasterizes a triangle into heightfield spans.
// Params:
//	v0,v1,v2 - (in) the vertices of the triangle.
//	flags - (in) triangle flags (uses WALKABLE)
//	areaFlag - (in) area flag to apply to spans
//	solid - (in) heighfield where the triangle is rasterized
//  flagMergeThr - (in) distance in voxel where walkable flag is favored over non-walkable.
void rcRasterizeTriangle(const float* v0, const float* v1, const float* v2,
						 unsigned char flags, unsigned char areaFlags, rcHeightfield& solid,
						 const int flagMergeThr = 1);

// Rasterizes indexed triangle mesh into heightfield spans.
// Params:
//	verts - (in) array of vertices
//	nv - (in) vertex count
//	tris - (in) array of triangle vertex indices
//	flags - (in) array of triangle flags (uses WALKABLE)
//	areaFlag - (in) area flag to apply to spans
//	nt - (in) triangle count
//	solid - (in) heighfield where the triangles are rasterized
//  flagMergeThr - (in) distance in voxel where walkable flag is favored over non-walkable.
void rcRasterizeTriangles(const float* verts, const int nv,
						  const int* tris, const unsigned char* flags, const unsigned char areaFlag, const int nt,
						  rcHeightfield& solid, const int flagMergeThr = 1);

// Rasterizes indexed triangle mesh into heightfield spans.
// Params:
//	verts - (in) array of vertices
//	nv - (in) vertex count
//	tris - (in) array of triangle vertex indices
//	flags - (in) array of triangle flags (uses WALKABLE)
//	areaFlags - (in) array of area flags to apply to spans
//	nt - (in) triangle count
//	solid - (in) heighfield where the triangles are rasterized
//  flagMergeThr - (in) distance in voxel where walkable flag is favored over non-walkable.
void rcRasterizeTriangles(const float* verts, const int nv,
						  const int* tris, const unsigned char* flags, const unsigned char* areaFlags, const int nt,
						  rcHeightfield& solid, const int flagMergeThr = 1);

// Rasterizes indexed triangle mesh into heightfield spans.
// Params:
//	verts - (in) array of vertices
//	nv - (in) vertex count
//	tris - (in) array of triangle vertex indices
//	flags - (in) array of triangle flags (uses WALKABLE)
//	areaFlags - (in) array of area flags to apply to spans
//	nt - (in) triangle count
//	solid - (in) heighfield where the triangles are rasterized
//  flagMergeThr - (in) distance in voxel where walkable flag is favored over non-walkable.
void rcRasterizeTriangles(const float* verts, const int nv,
						  const unsigned short* tris, const unsigned char* flags, const unsigned char* areaFlags, const int nt,
						  rcHeightfield& solid, const int flagMergeThr = 1);

// Rasterizes the triangles into heightfield spans.
// Params:
//	verts - (in) array of vertices
//	flags - (in) array of triangle flags (uses WALKABLE)
//	areaFlags - (in) array of area flags to apply to spans
//	nt - (in) triangle count
//	solid - (in) heighfield where the triangles are rasterized
void rcRasterizeTriangles(const float* verts, const unsigned char* flags, const unsigned char* areaFlags, const int nt,
						  rcHeightfield& solid, const int flagMergeThr = 1);

// Marks non-walkable low obstacles as walkable if they are closer than walkableClimb
// from a walkable surface. Applying this filter allows to step over low hanging
// low obstacles.
// Params:
//	walkableHeight - (in) minimum height where the agent can still walk
//	solid - (in/out) heightfield describing the solid space
// TODO: Missuses ledge flag, must be called before rcFilterLedgeSpans!
void rcFilterLowHangingWalkableObstacles(const int walkableClimb, rcHeightfield& solid);

// Removes WALKABLE flag from all spans that are at ledges. This filtering
// removes possible overestimation of the conservative voxelization so that
// the resulting mesh will not have regions hanging in air over ledges.
// Params:
//	walkableHeight - (in) minimum height where the agent can still walk
//	walkableClimb - (in) maximum height between grid cells the agent can climb
//	solid - (in/out) heightfield describing the solid space
void rcFilterLedgeSpans(const int walkableHeight,
						const int walkableClimb,
						rcHeightfield& solid);

// Removes WALKABLE flag from all spans which have smaller than
// 'walkableHeight' clearane above them.
// Params:
//	walkableHeight - (in) minimum height where the agent can still walk
//	solid - (in/out) heightfield describing the solid space
void rcFilterWalkableLowHeightSpans(int walkableHeight,
									rcHeightfield& solid);

// Builds compact representation of the heightfield.
// Params:
//	walkableHeight - (in) minimum height where the agent can still walk
//	walkableClimb - (in) maximum height between grid cells the agent can climb
//	hf - (in) heightfield to be compacted
//	chf - (out) compact heightfield representing the open space.
// Returns false if operation ran out of memory.
bool rcBuildCompactHeightfield(const int walkableHeight, const int walkableClimb,
							   unsigned char flags,
							   rcHeightfield& hf,
							   rcCompactHeightfield& chf);

// Erodes specified area id and replaces the are with null.
// Params:
//  areaId - (in) area to erode.
//  radius - (in) radius of erosion (max 255).
//	chf - (in/out) compact heightfield to erode.
bool rcErodeArea(unsigned char areaId, int radius, rcCompactHeightfield& chf);

// Marks the area of the convex polygon into the area type of the compact heighfield.
// Params:
//  bmin/bmax - (in) bounds of the axis aligned box.
//  areaId - (in) area ID to mark.
//	chf - (in/out) compact heightfield to mark.
void rcMarkBoxArea(const float* bmin, const float* bmax, unsigned char areaId,
				   rcCompactHeightfield& chf);

// Marks the area of the convex polygon into the area type of the compact heighfield.
// Params:
//  verts - (in) vertices of the convex polygon.
//  nverts - (in) number of vertices in the polygon.
//  hmin/hmax - (in) min and max height of the polygon.
//  areaId - (in) area ID to mark.
//	chf - (in/out) compact heightfield to mark.
void rcMarkConvexPolyArea(const float* verts, const int nverts,
						  const float hmin, const float hmax, unsigned char areaId,
						  rcCompactHeightfield& chf);


// Builds distance field and stores it into the combat heightfield.
// Params:
//	chf - (in/out) compact heightfield representing the open space.
// Returns false if operation ran out of memory.
bool rcBuildDistanceField(rcCompactHeightfield& chf);

// Divides the walkable heighfied into simple regions using watershed partitioning.
// Each region has only one contour and no overlaps.
// The regions are stored in the compact heightfield 'reg' field.
// The regions will be shrinked by the radius of the agent.
// The process sometimes creates small regions. The parameter
// 'minRegionSize' specifies the smallest allowed regions size.
// If the area of a regions is smaller than allowed, the regions is
// removed or merged to neighbour region. 
// Params:
//	chf - (in/out) compact heightfield representing the open space.
//	minRegionSize - (in) the smallest allowed regions size.
//	maxMergeRegionSize - (in) the largest allowed regions size which can be merged.
// Returns false if operation ran out of memory.
bool rcBuildRegions(rcCompactHeightfield& chf,
					int borderSize, int minRegionSize, int mergeRegionSize);

// Divides the walkable heighfied into simple regions using simple monotone partitioning.
// Each region has only one contour and no overlaps.
// The regions are stored in the compact heightfield 'reg' field.
// The regions will be shrinked by the radius of the agent.
// The process sometimes creates small regions. The parameter
// 'minRegionSize' specifies the smallest allowed regions size.
// If the area of a regions is smaller than allowed, the regions is
// removed or merged to neighbour region. 
// Params:
//	chf - (in/out) compact heightfield representing the open space.
//	minRegionSize - (in) the smallest allowed regions size.
//	maxMergeRegionSize - (in) the largest allowed regions size which can be merged.
// Returns false if operation ran out of memory.
bool rcBuildRegionsMonotone(rcCompactHeightfield& chf,
							int borderSize, int minRegionSize, int mergeRegionSize);

// Builds simplified contours from the regions outlines.
// Params:
//	chf - (in) compact heightfield which has regions set.
//	maxError - (in) maximum allowed distance between simplified countour and cells.
//	maxEdgeLen - (in) maximum allowed contour edge length in cells.
//	cset - (out) Resulting contour set.
// Returns false if operation ran out of memory.
bool rcBuildContours(rcCompactHeightfield& chf,
					 const float maxError, const int maxEdgeLen,
					 rcContourSet& cset);

// Builds connected convex polygon mesh from contour polygons.
// Params:
//	cset - (in) contour set.
//	nvp - (in) maximum number of vertices per polygon.
//	mesh - (out) poly mesh.
// Returns false if operation ran out of memory.
bool rcBuildPolyMesh(rcContourSet& cset, int nvp, rcPolyMesh& mesh);

bool rcMergePolyMeshes(rcPolyMesh** meshes, const int nmeshes, rcPolyMesh& mesh);

// Builds detail triangle mesh for each polygon in the poly mesh.
// Params:
//	mesh - (in) poly mesh to detail.
//	chf - (in) compacy height field, used to query height for new vertices.
//  sampleDist - (in) spacing between height samples used to generate more detail into mesh.
//  sampleMaxError - (in) maximum allowed distance between simplified detail mesh and height sample.
//	pmdtl - (out) detail mesh.
// Returns false if operation ran out of memory.
bool rcBuildPolyMeshDetail(const rcPolyMesh& mesh, const rcCompactHeightfield& chf,
						   const float sampleDist, const float sampleMaxError,
						   rcPolyMeshDetail& dmesh);

bool rcMergePolyMeshDetails(rcPolyMeshDetail** meshes, const int nmeshes, rcPolyMeshDetail& mesh);


#endif // RECAST_H
