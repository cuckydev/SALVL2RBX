#pragma once

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "ninja.h"

#include <Winsock2.h>
#include <wininet.h>

// SALVL types
typedef Uint32 SALVL_SurfFlag;
#define SALVL_SURFFLAG_SOLID   (1 << 0)
#define SALVL_SURFFLAG_VISIBLE (1 << 1)

#define SALVL_FLAG_REMAP(x, from, to) ((x & from) ? to : 0)

// SALVL types
struct SALVL_Texture
{
	// File information
	std::string name, name_fu, name_fv, name_fuv;
	std::string path, path_fu, path_fv, path_fuv;
	std::string url, url_fu, url_fv, url_fuv;
	std::string material = "Plastic";
	int xres = 0, yres = 0;
	bool transparent = false;
};

struct SALVL_Vertex
{
	NJS_VECTOR pos = {};
	NJS_POINT2 tex = {};
	NJS_VECTOR nor = {0.0f, 1.0f, 0.0f};
	NJS_VECTOR tan = {};
	Float ts = 0.0f;
	Uint8 r = 255, g = 255, b = 255, a = 255;

	inline bool operator==(const SALVL_Vertex &rhs)
	{
		return
			pos.x == rhs.pos.x && pos.y == rhs.pos.y && pos.z == rhs.pos.z &&
			tex.x == rhs.tex.x && tex.y == rhs.tex.y &&
			nor.x == rhs.nor.x && nor.y == rhs.nor.y && nor.z == rhs.nor.z &&
			tan.x == rhs.tan.x && tan.y == rhs.tan.y && tan.z == rhs.tan.z && ts == rhs.ts &&
			r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
	}
	inline bool operator!=(const SALVL_Vertex &rhs)
	{
		return !(*this == rhs);
	}
};

struct SALVL_MeshFace
{
	Sint16 i[3] = {};

	SALVL_MeshFace() {}
	SALVL_MeshFace(Sint16 a, Sint16 b, Sint16 c)
	{
		i[0] = a;
		i[1] = b;
		i[2] = c;
	}
};

struct edge_hash
{
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1, T2> &p) const
	{
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);

		return h1 ^ ((int)h2 << 16);
	}
};

struct SALVL_MeshPart
{
	// Mesh data
	std::vector<SALVL_Vertex> vertex;
	std::vector<SALVL_MeshFace> indices;

	Uint16 AddVertex(SALVL_Vertex &adder)
	{
		// Check if identical vertex already exists
		Uint16 j = 0;
		for (auto &i : vertex)
		{
			if (i == adder)
				return j;
			j++;
		}

		// Push new vertex
		vertex.push_back(adder);
		return j;
	}

	// Material information
	Uint32 matflags = 0;

	SALVL_Texture *texture = nullptr;
	Uint32 diffuse = 0xB2B2B2;

	// File information
	std::string name;
	std::string name_texture;
	std::string path;
	std::string url;
	std::string url_texture;

	unsigned int ind = 0;

	// AABB
	NJS_VECTOR aabb_correct = {};
	NJS_VECTOR size = {};

	void AABBCorrect()
	{
		// Get current bounding box
		float minx, miny, minz;
		float maxx, maxy, maxz;
		minx = miny = minz = std::numeric_limits<float>::infinity();
		maxx = maxy = maxz = -std::numeric_limits<float>::infinity();

		for (auto &i : vertex)
		{
			if (i.pos.x < minx)
				minx = i.pos.x;
			if (i.pos.x > maxx)
				maxx = i.pos.x;

			if (i.pos.y < miny)
				miny = i.pos.y;
			if (i.pos.y > maxy)
				maxy = i.pos.y;

			if (i.pos.z < minz)
				minz = i.pos.z;
			if (i.pos.z > maxz)
				maxz = i.pos.z;
		}

		// Get size
		size.x = maxx - minx;
		size.y = maxy - miny;
		size.z = maxz - minz;
		if (size.x < 0.2f)
			size.x = 0.2f;
		if (size.y < 0.2f)
			size.y = 0.2f;
		if (size.z < 0.2f)
			size.z = 0.2f;

		// Correct AABB
		aabb_correct.x = (minx + maxx) * 0.5f;
		aabb_correct.y = (miny + maxy) * 0.5f;
		aabb_correct.z = (minz + maxz) * 0.5f;

		for (auto &i : vertex)
		{
			i.pos.x -= aabb_correct.x;
			i.pos.y -= aabb_correct.y;
			i.pos.z -= aabb_correct.z;
		}
	}

	void AutoNormals()
	{
		// Make sure faces connect with the same winding order by their edges, and flip if necessary
		std::unordered_map<std::pair<Sint16, Sint16>, std::vector<int>, edge_hash> edges;
		std::unordered_set<int> fixed;

		for (auto &i : indices)
		{
			const auto e0 = std::pair<Sint16, Sint16>{ i.i[0], i.i[1] };
			const auto e1 = std::pair<Sint16, Sint16>{ i.i[1], i.i[2] };
			const auto e2 = std::pair<Sint16, Sint16>{ i.i[2], i.i[0] };
			edges[e0].push_back(&i - indices.data());
			edges[e1].push_back(&i - indices.data());
			edges[e2].push_back(&i - indices.data());
		}

		std::function<void(SALVL_MeshFace&)> fix;
		fix = [&fix, &fixed, this, &edges](SALVL_MeshFace& i) -> void
		{
			const auto e0 = std::pair<Sint16, Sint16>{ i.i[0], i.i[1] };
			const auto e1 = std::pair<Sint16, Sint16>{ i.i[1], i.i[2] };
			const auto e2 = std::pair<Sint16, Sint16>{ i.i[2], i.i[0] };

			const auto oe0 = std::pair<Sint16, Sint16>{ i.i[0], i.i[1] };
			const auto oe1 = std::pair<Sint16, Sint16>{ i.i[1], i.i[2] };
			const auto oe2 = std::pair<Sint16, Sint16>{ i.i[2], i.i[0] };

			fixed.insert(&i - indices.data());

			for (auto& o : edges[oe0])
			{
				auto it = fixed.find(o);
				if (it != fixed.end())
					continue;

				auto& oi = indices[o];
				std::swap(oi.i[0], oi.i[1]);
				fix(oi);
			}

			for (auto& o : edges[oe1])
			{
				auto it = fixed.find(o);
				if (it != fixed.end())
					continue;

				auto& oi = indices[o];
				std::swap(oi.i[1], oi.i[2]);
				fix(oi);
			}

			for (auto& o : edges[oe2])
			{
				auto it = fixed.find(o);
				if (it != fixed.end())
					continue;

				auto& oi = indices[o];
				std::swap(oi.i[2], oi.i[0]);
				fix(oi);
			}

			for (auto &o : edges[e0])
			{
				auto it = fixed.find(o);
				if (it != fixed.end())
					continue;

				auto &oi = indices[o];
				fix(oi);
			}

			for (auto &o : edges[e1])
			{
				auto it = fixed.find(o);
				if (it != fixed.end())
					continue;

				auto &oi = indices[o];
				fix(oi);
			}

			for (auto &o : edges[e2])
			{
				auto it = fixed.find(o);
				if (it != fixed.end())
					continue;

				auto &oi = indices[o];
				fix(oi);
			}
		};

		for (auto& i : indices)
		{
			auto it = fixed.find(&i - indices.data());
			if (it != fixed.end())
				continue;

			fix(i);
		}

		// Clear normals
		for (auto &i : vertex)
		{
			i.nor.x = 0.0f;
			i.nor.y = 0.0f;
			i.nor.z = 0.0f;
		}

		// Calculate normals
		std::vector<int> sums(vertex.size());

		for (auto &i : indices)
		{
			int i0 = i.i[0];
			int i1 = i.i[1];
			int i2 = i.i[2];
			auto &va = vertex[i0];
			auto &vb = vertex[i1];
			auto &vc = vertex[i2];

			NJS_VECTOR ab = {vb.pos.x - va.pos.x, vb.pos.y - va.pos.y, vb.pos.z - va.pos.z};
			NJS_VECTOR ac = {vc.pos.x - va.pos.x, vc.pos.y - va.pos.y, vc.pos.z - va.pos.z};
			NJS_VECTOR normal = {ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};

			float dot = normal.x * va.nor.x + normal.y * va.nor.y + normal.z * va.nor.z;

			float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			normal.x /= length;
			normal.y /= length;
			normal.z /= length;

			va.nor.x += normal.x;
			va.nor.y += normal.y;
			va.nor.z += normal.z;

			vb.nor.x += normal.x;
			vb.nor.y += normal.y;
			vb.nor.z += normal.z;

			vc.nor.x += normal.x;
			vc.nor.y += normal.y;
			vc.nor.z += normal.z;

			sums[i0]++;
			sums[i1]++;
			sums[i2]++;
		}

		for (unsigned int i = 0; i < vertex.size(); i++)
		{
			auto &v = vertex[i];
			int length = sums[i];
			v.nor.x /= length;
			v.nor.y /= length;
			v.nor.z /= length;

			float length2 = sqrtf(v.nor.x * v.nor.x + v.nor.y * v.nor.y + v.nor.z * v.nor.z);
			v.nor.x /= length2;
			v.nor.y /= length2;
			v.nor.z /= length2;
		}
	}
};

struct SALVL_Mesh
{
	// Contained mesh parts
	std::unordered_map<int, SALVL_MeshPart> parts;
	bool do_upload = false; // Only upload parts if visible
};

struct SALVL_MeshInstance
{
	// Referenced mesh
	SALVL_Mesh *mesh = nullptr;
	
	// Positioning
	NJS_MATRIX matrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	NJS_VECTOR pos = {};

	// Flags
	SALVL_SurfFlag surf_flag = 0;
};

struct SALVL_MeshPartInstance
{
	// Referenced mesh part
	SALVL_MeshPart *meshpart = nullptr;

	// Positioning
	NJS_MATRIX matrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	NJS_VECTOR pos = {};

	// Flags
	SALVL_SurfFlag surf_flag = 0;
};

// Level container
struct SALVL
{
	// Level assets
	std::vector<SALVL_Texture> textures;
	std::unordered_map<void*, SALVL_Mesh> meshes;
	std::vector<SALVL_MeshInstance> meshinstances;
};

// Ninja reimplementation
void Reimp_njRotateX(NJS_MATRIX cframe, Angle x);
void Reimp_njRotateY(NJS_MATRIX cframe, Angle x);
void Reimp_njRotateZ(NJS_MATRIX cframe, Angle x);

// Entry point
int SALVL2RBX(int argc, char *argv[], int (loader)(SALVL&, std::string));
