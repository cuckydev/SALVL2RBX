#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "ninja.h"

#include <Winsock2.h>
#include <wininet.h>

//SALVL types
typedef Uint32 SALVL_SurfFlag;
#define SALVL_SURFFLAG_SOLID   (1 << 0)
#define SALVL_SURFFLAG_VISIBLE (1 << 1)

#define SALVL_FLAG_REMAP(x, from, to) ((x & from) ? to : 0)

//SALVL types
struct SALVL_Texture
{
	//File information
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
	NJS_VECTOR nor = {0.0f, 1.0f, 0.0f}; //MainMemory suggested straight up
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
	Sint16 i[3];
};

struct SALVL_MeshPart
{
	//Mesh data
	std::vector<SALVL_Vertex> vertex;
	std::vector<SALVL_MeshFace> indices;

	Uint16 AddVertex(SALVL_Vertex &adder)
	{
		//Check if identical vertex already exists
		Uint16 j = 0;
		for (auto &i : vertex)
		{
			if (i == adder)
				return j;
			j++;
		}

		//Push new vertex
		vertex.push_back(adder);
		return j;
	}

	//Material information
	Uint32 matflags = 0;

	SALVL_Texture *texture = nullptr;
	Uint32 diffuse = 0xB2B2B2;

	//File information
	std::string name;
	std::string name_texture;
	std::string path;
	std::string url;
	std::string url_texture;

	unsigned int ind = 0;

	//AABB
	NJS_VECTOR aabb_correct = {};
	NJS_VECTOR size = {};

	void AABBCorrect()
	{
		//Get current bounding box
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

		//Get size
		size.x = maxx - minx;
		size.y = maxy - miny;
		size.z = maxz - minz;
		if (size.x < 0.2f)
			size.x = 0.2f;
		if (size.y < 0.2f)
			size.y = 0.2f;
		if (size.z < 0.2f)
			size.z = 0.2f;

		//Correct AABB
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
};

struct SALVL_Mesh
{
	//Contained mesh parts
	std::unordered_map<Uint16, SALVL_MeshPart> parts;
	bool do_upload = false; //Only upload parts if visible
};

struct SALVL_MeshInstance
{
	//Referenced mesh
	SALVL_Mesh *mesh = nullptr;
	
	//Positioning
	NJS_MATRIX matrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	NJS_VECTOR pos = {};

	//Flags
	SALVL_SurfFlag surf_flag = 0;
};

struct SALVL_MeshPartInstance
{
	//Referenced mesh part
	SALVL_MeshPart *meshpart = nullptr;

	//Positioning
	NJS_MATRIX matrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	NJS_VECTOR pos = {};

	//Flags
	SALVL_SurfFlag surf_flag = 0;
};

//Level container
struct SALVL
{
	//Level assets
	std::vector<SALVL_Texture> textures;
	std::unordered_map<void*, SALVL_Mesh> meshes;
	std::vector<SALVL_MeshInstance> meshinstances;
};

//Ninja reimplementation
void Reimp_njRotateX(NJS_MATRIX cframe, Angle x);
void Reimp_njRotateY(Float *cframe, Angle x);
void Reimp_njRotateZ(Float *cframe, Angle x);

//Entry point
int SALVL2RBX(int argc, char *argv[], int (loader)(SALVL&, std::string));
