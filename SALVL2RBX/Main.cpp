#include <iostream>
#include <fstream>
#include <vector>
#include <set>

#include "LandTableInfo.h"

//SA1 types
#define SALVL_OBJFLAG_NO_POSITION 0x01
#define SALVL_OBJFLAG_NO_ROTATE   0x02
#define SALVL_OBJFLAG_NO_SCALE    0x04
#define SALVL_OBJFLAG_NO_DISPLAY  0x08
#define SALVL_OBJFLAG_NO_CHILDREN 0x10
#define SALVL_OBJFLAG_ROTATE_XYZ  0x20
#define SALVL_OBJFLAG_NO_ANIMATE  0x40
#define SALVL_OBJFLAG_NO_MORPH    0x80

typedef uint32_t SA1LVL_SurfFlag;
#define SA1LVL_SURFFLAG_SOLID                  0x1
#define SA1LVL_SURFFLAG_WATER                  0x2
#define SA1LVL_SURFFLAG_NO_FRICTION            0x4
#define SA1LVL_SURFFLAG_NO_ACCELERATION        0x8
#define SA1LVL_SURFFLAG_CANNOT_LAND            0x40
#define SA1LVL_SURFFLAG_INCREASED_ACCELERATION 0x80
#define SA1LVL_SURFFLAG_DIGGABLE               0x100
#define SA1LVL_SURFFLAG_UNCLIMBABLE            0x1000
#define SA1LVL_SURFFLAG_HURT                   0x10000
#define SA1LVL_SURFFLAG_FOOTPRINTS             0x100000
#define SA1LVL_SURFFLAG_VISIBLE                0x80000000

//Intermediate types
struct SALVL_Vertex
{
	float px = 0.0f, py = 0.0f, pz = 0.0f;
	float u = 0.0f, v = 0.0f;
	float nx = 0.0f, ny = 1.0f, nz = 0.0f;
	float tx = 0.0f, ty = 0.0f, tz = -1.0f, ts = 1.0f;
	float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

	inline bool operator==(const SALVL_Vertex &rhs)
	{
		return px == rhs.px && py == rhs.py && pz == rhs.pz && u == rhs.u && v == rhs.v && nx == rhs.nx && ny == rhs.ny && nz == rhs.nz && tx == rhs.tx && ty == rhs.ty && tz == rhs.tz && ts == rhs.ts && r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
	}
	inline bool operator!=(const SALVL_Vertex &rhs)
	{
		return !(*this == rhs);
	}
};

struct SALVL_Face
{
	int v0, v1, v2;
};

struct SALVL_MeshPart
{
	SA1LVL_SurfFlag surf_flag = 0;
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
		vertex.px = model->points[i].x;
		vertex.py = model->points[i].y;
		vertex.pz = model->points[i].z;
		vertex.nx = model->normals[i].x;
		vertex.ny = model->normals[i].y;
		vertex.nz = model->normals[i].z;

		//Add vertex and push index
		indices.push_back(AddVertex(vertex));
	}
};

//Entry point
int main(int argc, char *argv[])
{
	std::ofstream stream_obj("test.obj");

	//Check arguments
	if (argc < 2)
	{
		std::cout << "usage: SA1LVL2RBX sa1lvl" << std::endl;
		return 0;
	}
	const char *path_lvl = argv[1];

	//Read landtable from LVL file
	std::ifstream stream_lvl(path_lvl, std::ios::binary);
	if (!stream_lvl.is_open())
	{
		std::cout << "Failed to open LVL " << path_lvl << std::endl;
		return 1;
	}

	LandTableInfo landtable_lvl(path_lvl);

	LandTable *landtable = landtable_lvl.getlandtable();
	if (landtable == nullptr)
	{
		std::cout << "Failed to convert LVL " << path_lvl << std::endl;
		return 1;
	}

	//Read COLs
	std::vector<SALVL_MeshPart> meshparts;

	COL *colp = landtable->Col;
	for (int16_t i = 0; i < landtable->COLCount; i++, colp++)
	{
		//Convert object
		NJS_OBJECT *object = colp->Model;
		if (object != nullptr)
		{
			NJS_MODEL_SADX *model = object->getbasicdxmodel();
			if (model != nullptr)
			{
				//Create mesh part
				SALVL_MeshPart meshpart;

				//Read meshsets
				NJS_MESHSET_SADX *meshset = model->meshsets;
				for (Uint16 k = 0; k < model->nbMeshset; k++, meshset++)
				{
					Uint16 material = meshset->type_matId & 0x3FFF;
					Uint16 polytype = meshset->type_matId >> 14;

					Sint16 *indp = meshset->meshes;
					for (Uint16 p = 0, j = 0; p < meshset->nbMesh; p++)
					{
						switch (polytype)
						{
							case 0: //Triangles
							{
								meshpart.IndexVertex(model, meshset, indp[0], j++);
								meshpart.IndexVertex(model, meshset, indp[1], j++);
								meshpart.IndexVertex(model, meshset, indp[2], j++);
								indp += 3;
								break;
							}
							case 1: //Quads
							{
								meshpart.IndexVertex(model, meshset, indp[0], j++);
								meshpart.IndexVertex(model, meshset, indp[1], j++);
								meshpart.IndexVertex(model, meshset, indp[2], j++);
								meshpart.IndexVertex(model, meshset, indp[2], j++);
								meshpart.IndexVertex(model, meshset, indp[1], j++);
								meshpart.IndexVertex(model, meshset, indp[3], j++);
								indp += 4;
								break;
							}
							case 3: //Strip
							{
								Uint16 first = *indp++;
								for (Uint16 l = 0; l < (first & 0x7FFF) - 2; l++)
								{
									first ^= 0x8000;
									if (first & 0x8000)
									{
										meshpart.IndexVertex(model, meshset, indp[l + 0], j++);
										meshpart.IndexVertex(model, meshset, indp[l + 1], j++);
										meshpart.IndexVertex(model, meshset, indp[l + 2], j++);
									}
									else
									{
										meshpart.IndexVertex(model, meshset, indp[l + 1], j++);
										meshpart.IndexVertex(model, meshset, indp[l + 0], j++);
										meshpart.IndexVertex(model, meshset, indp[l + 2], j++);
									}
								}
								indp += (first & 0x7FFF);
								break;
							}
						}
					}
				}

				//Push meshpart
				meshparts.push_back(meshpart);
			}
		}
	}

	//Write to obj
	int caca = 1;
	for (auto &i : meshparts)
	{
		for (auto &j : i.vertex)
		{
			stream_obj << "v " << j.px << " " << j.py << " " << j.pz << std::endl;
			stream_obj << "vn " << j.nx << " " << j.ny << " " << j.nz << std::endl;
		}
		for (int j = 0; j < i.indices.size(); j += 3)
		{
			stream_obj << "f " <<
				(i.indices[j + 0] + caca) << "//" << (i.indices[j + 0] + caca) << " " <<
				(i.indices[j + 1] + caca) << "//" << (i.indices[j + 1] + caca) << " " <<
				(i.indices[j + 2] + caca) << "//" << (i.indices[j + 2] + caca) << std::endl;
		}
		caca += i.vertex.size();
	}

	return 0;
}
