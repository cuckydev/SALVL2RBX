#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "ninja.h"

#include <Winsock2.h>
#include <wininet.h>

//SALVL types
#define SALVL_OBJFLAG_NO_POSITION 0x01
#define SALVL_OBJFLAG_NO_ROTATE   0x02
#define SALVL_OBJFLAG_NO_SCALE    0x04
#define SALVL_OBJFLAG_NO_DISPLAY  0x08
#define SALVL_OBJFLAG_NO_CHILDREN 0x10
#define SALVL_OBJFLAG_ROTATE_XYZ  0x20
#define SALVL_OBJFLAG_NO_ANIMATE  0x40
#define SALVL_OBJFLAG_NO_MORPH    0x80

typedef Uint32 SALVL_SurfFlag;
#define SALVL_SURFFLAG_SOLID                  0x1
#define SALVL_SURFFLAG_WATER                  0x2
#define SALVL_SURFFLAG_NO_FRICTION            0x4
#define SALVL_SURFFLAG_NO_ACCELERATION        0x8
#define SALVL_SURFFLAG_CANNOT_LAND            0x40
#define SALVL_SURFFLAG_INCREASED_ACCELERATION 0x80
#define SALVL_SURFFLAG_DIGGABLE               0x100
#define SALVL_SURFFLAG_UNCLIMBABLE            0x1000
#define SALVL_SURFFLAG_HURT                   0x10000
#define SALVL_SURFFLAG_FOOTPRINTS             0x100000
#define SALVL_SURFFLAG_VISIBLE                0x80000000

//SALVL types
struct SALVL_Texture
{
	//File information
	std::string name, name_fu, name_fv, name_fuv;
	std::string path, path_fu, path_fv, path_fuv;
	std::string url, url_fu, url_fv, url_fuv;
	int xres = 0, yres = 0;
	bool transparent = false;
};

struct SALVL_Vertex
{
	NJS_VECTOR pos = {};
	NJS_POINT2 tex = {};
	NJS_VECTOR nor = {};
	NJS_VECTOR tan = {};
	Float ts = 0.0f;
	Float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

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

struct SALVL_MeshPart
{
	//Mesh data
	std::vector<SALVL_Vertex> vertex;
	std::vector<Sint16> indices;

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

	void IndexVertex(NJS_MODEL_SADX *model, NJS_MESHSET_SADX *meshset, Sint16 i, Sint16 j)
	{
		//Construct vertex
		SALVL_Vertex vertex;
		vertex.pos.x = model->points[i].x;
		vertex.pos.y = model->points[i].y;
		vertex.pos.z = model->points[i].z;
		vertex.nor.x = model->normals[i].x;
		vertex.nor.y = model->normals[i].y;
		vertex.nor.z = model->normals[i].z;
		if (meshset->vertuv != nullptr)
		{
			vertex.tex.x = meshset->vertuv[j].u / 256.0f;
			vertex.tex.y = meshset->vertuv[j].v / 256.0f;
			if (matflags & NJD_FLAG_FLIP_U)
				vertex.tex.x *= 0.5f;
			if (matflags & NJD_FLAG_FLIP_V)
				vertex.tex.y *= 0.5f;
		}

		//Add vertex and push index
		indices.push_back(AddVertex(vertex));
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
		if (size.x < 0.1f)
			size.x = 0.1f;
		if (size.y < 0.1f)
			size.y = 0.1f;
		if (size.z < 0.1f)
			size.z = 0.1f;

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
