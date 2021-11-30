#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <limits>

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
	NJS_VECTOR pos = {};
	NJS_POINT2 tex = {};
	NJS_VECTOR nor = {};
	NJS_VECTOR tan = {};
	float ts = 0.0f;
	float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

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
	std::vector<SALVL_Vertex> vertex;
	std::vector<Sint16> indices;

	unsigned int ind = 0;
	NJS_VECTOR aabb_correct = {};
	NJS_VECTOR size = {};

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

		//Add vertex and push index
		indices.push_back(AddVertex(vertex));
	}

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
		if (size.x < 0.1)
			size.x = 0.1;
		if (size.y < 0.1)
			size.y = 0.1;
		if (size.z < 0.1)
			size.z = 0.1;

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
	std::unordered_map<Uint16, SALVL_MeshPart> parts;
};

struct SALVL_MeshInstance
{
	SALVL_Mesh *mesh = nullptr;
	
	NJS_MATRIX matrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	NJS_VECTOR pos = {};
	SA1LVL_SurfFlag surf_flag = 0;
};

struct SALVL_MeshPartInstance
{
	SALVL_MeshPart *meshpart = nullptr;

	NJS_MATRIX matrix = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	NJS_VECTOR pos = {};

	float transparency;
};

void Reimp_njRotateX(NJS_MATRIX cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	float m10 = cframe[M10];
	float m11 = cframe[M11];
	float m12 = cframe[M12];
	float m13 = cframe[M13];

	cframe[M10] = sin * cframe[M20] + m10 * cos;
	cframe[M11] = sin * cframe[M21] + m11 * cos;
	cframe[M12] = sin * cframe[M22] + m12 * cos;
	cframe[M13] = sin * cframe[M23] + m13 * cos;
	cframe[M20] = cos * cframe[M20] - m10 * sin;
	cframe[M21] = cos * cframe[M21] - m11 * sin;
	cframe[M22] = cos * cframe[M22] - m12 * sin;
	cframe[M23] = cos * cframe[M23] - m13 * sin;
}

void Reimp_njRotateY(float *cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	float m00 = cframe[M00];
	float m01 = cframe[M01];
	float m02 = cframe[M02];
	float m03 = cframe[M03];

	cframe[M00] = m00 * cos - sin * cframe[M20];
	cframe[M01] = m01 * cos - sin * cframe[M21];
	cframe[M02] = m02 * cos - sin * cframe[M22];
	cframe[M03] = m03 * cos - sin * cframe[M23];
	cframe[M20] = cos * cframe[M20] + m00 * sin;
	cframe[M21] = cos * cframe[M21] + m01 * sin;
	cframe[M22] = cos * cframe[M22] + m02 * sin;
	cframe[M23] = cos * cframe[M23] + m03 * sin;
}

void Reimp_njRotateZ(float *cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	float m00 = cframe[M00];
	float m01 = cframe[M01];
	float m02 = cframe[M02];
	float m03 = cframe[M03];

	cframe[M00] = m00 * cos + sin * cframe[M10];
	cframe[M01] = m01 * cos + sin * cframe[M11];
	cframe[M02] = m02 * cos + sin * cframe[M12];
	cframe[M03] = m03 * cos + sin * cframe[M13];
	cframe[M10] = cos * cframe[M10] - m00 * sin;
	cframe[M11] = cos * cframe[M11] - m01 * sin;
	cframe[M12] = cos * cframe[M12] - m02 * sin;
	cframe[M13] = cos * cframe[M13] - m03 * sin;
}

//Entry point
int main(int argc, char *argv[])
{
	//Check arguments
	if (argc < 4)
	{
		std::cout << "usage: SA1LVL2RBX rbx_content scale sa1lvl" << std::endl;
		return 0;
	}

	std::string path_content = argv[1];
	if (path_content.back() != '/' && path_content.back() != '\\')
		path_content += "/";

	std::string path_rbxmx = path_content + "salvl/level.rbxmx";
	float scale = strtof(argv[2], nullptr);
	std::string path_lvl = argv[3];

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
	std::unordered_map<NJS_MODEL_SADX*, SALVL_Mesh> meshes;
	std::vector<SALVL_MeshInstance> meshinstances;

	COL *colp = landtable->Col;
	for (int16_t i = 0; i < landtable->COLCount; i++, colp++)
	{
		//Convert object
		NJS_OBJECT *object = colp->Model;
		if (object != nullptr)
		{
			//Get model to use
			NJS_MODEL_SADX *model = object->getbasicdxmodel();

			if (model != nullptr)
			{
				//Create mesh instance
				SALVL_MeshInstance meshinstance;
				meshinstance.surf_flag = colp->Flags;

				if (object->evalflags & SALVL_OBJFLAG_ROTATE_XYZ)
				{
					Reimp_njRotateX(meshinstance.matrix, object->ang[0]);
					Reimp_njRotateY(meshinstance.matrix, object->ang[1]);
					Reimp_njRotateZ(meshinstance.matrix, object->ang[2]);
				}
				else
				{
					Reimp_njRotateZ(meshinstance.matrix, object->ang[2]);
					Reimp_njRotateX(meshinstance.matrix, object->ang[0]);
					Reimp_njRotateY(meshinstance.matrix, object->ang[1]);
				}
				
				meshinstance.pos.x = object->pos[0];
				meshinstance.pos.y = object->pos[1];
				meshinstance.pos.z = object->pos[2];

				//Get mesh
				auto meshind = meshes.find(model);
				if (meshind != meshes.end())
				{
					//Use already created mesh instace
					meshinstance.mesh = &meshind->second;
				}
				else
				{
					//Read mesh parts
					SALVL_Mesh mesh;

					NJS_MESHSET_SADX *meshset = model->meshsets;
					for (Uint16 k = 0; k < model->nbMeshset; k++, meshset++)
					{
						Uint16 material = meshset->type_matId & 0x3FFF;
						Uint16 polytype = meshset->type_matId >> 14;

						//Create mesh part
						SALVL_MeshPart *meshpart = &mesh.parts[material];

						Sint16 *indp = meshset->meshes;
						for (Uint16 p = 0, j = 0; p < meshset->nbMesh; p++)
						{
							switch (polytype)
							{
								case 0: //Triangles
								{
									meshpart->IndexVertex(model, meshset, indp[0], j++);
									meshpart->IndexVertex(model, meshset, indp[1], j++);
									meshpart->IndexVertex(model, meshset, indp[2], j++);
									indp += 3;
									break;
								}
								case 1: //Quads
								{
									meshpart->IndexVertex(model, meshset, indp[0], j++);
									meshpart->IndexVertex(model, meshset, indp[1], j++);
									meshpart->IndexVertex(model, meshset, indp[2], j++);
									meshpart->IndexVertex(model, meshset, indp[2], j++);
									meshpart->IndexVertex(model, meshset, indp[1], j++);
									meshpart->IndexVertex(model, meshset, indp[3], j++);
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
											meshpart->IndexVertex(model, meshset, indp[l + 0], j++);
											meshpart->IndexVertex(model, meshset, indp[l + 1], j++);
											meshpart->IndexVertex(model, meshset, indp[l + 2], j++);
										}
										else
										{
											meshpart->IndexVertex(model, meshset, indp[l + 1], j++);
											meshpart->IndexVertex(model, meshset, indp[l + 0], j++);
											meshpart->IndexVertex(model, meshset, indp[l + 2], j++);
										}
									}
									indp += (first & 0x7FFF);
									break;
								}
							}
						}
					}

					//Push mesh to map and instance
					meshes[model] = mesh;
					meshinstance.mesh = &meshes[model];
				}

				//Push mesh instance
				meshinstances.push_back(meshinstance);
			}
		}
	}
	
	//Write RBX meshes
	std::unordered_map<SALVL_Mesh*, unsigned int> mesh_inds;

	unsigned int mesh_ind = 0;
	for (auto &i : meshes)
	{
		//Write mesh parts
		SALVL_Mesh *mesh = &i.second;
		mesh_inds[mesh] = mesh_ind;

		for (auto &j : mesh->parts)
		{
			//Correct meshpart AABB
			SALVL_MeshPart *meshpart = &j.second;
			meshpart->ind = mesh_ind;
			meshpart->AABBCorrect();

			//Open mesh file
			std::string path_mesh = path_content + "salvl/" + std::to_string(mesh_ind) + ".msh";
			std::ofstream stream_mesh(path_mesh, std::ios::binary);
			if (!stream_mesh.is_open())
			{
				std::cout << "Failed to open MSH " << path_mesh << std::endl;
				return 1;
			}
			stream_mesh.write("version 2.00\n", 13);

			//Write mesh header
			stream_mesh.put(0x0C); stream_mesh.put(0x00); //sizeof_MeshHeader
			stream_mesh.put(0x28); //sizeof_MeshVertex
			stream_mesh.put(0x0C); //sizeof_MeshFace

			unsigned int numVerts = meshpart->vertex.size();
			stream_mesh.write((const char*)&numVerts, 4);
			unsigned int numFaces = meshpart->indices.size() / 3;
			stream_mesh.write((const char*)&numFaces, 4);

			//Write vertex data
			for (auto &k : meshpart->vertex)
			{
				stream_mesh.write((const char*)&k.pos, sizeof(NJS_VECTOR)); //Position
				stream_mesh.write((const char*)&k.nor, sizeof(NJS_VECTOR)); //Normal
				stream_mesh.write((const char*)&k.tex, sizeof(NJS_POINT2)); //Texture
				stream_mesh.put(0x00); stream_mesh.put(0x00); stream_mesh.put(0x00); stream_mesh.put(0x00); //Tangent
				stream_mesh.put(0xFF); stream_mesh.put(0xFF); stream_mesh.put(0xFF); stream_mesh.put(0xFF); //RGBA tint
			}
			for (auto &k : meshpart->indices)
			{
				unsigned int index = k;
				stream_mesh.write((const char*)&index, 4);
			}

			//Increment mesh index
			mesh_ind++;
		}
	}

	//Get where meshes should be placed
	std::vector<SALVL_MeshPartInstance> mesh_collision;
	std::vector<SALVL_MeshPartInstance> mesh_visual;

	for (auto &i : meshinstances)
	{
		for (auto &j : i.mesh->parts)
		{
			//Construct mesh part instance
			SALVL_MeshPart *meshpart = &j.second;

			SALVL_MeshPartInstance meshpart_instance;
			meshpart_instance.meshpart = meshpart;
			meshpart_instance.matrix[M00] = i.matrix[M00];
			meshpart_instance.matrix[M10] = i.matrix[M01];
			meshpart_instance.matrix[M20] = i.matrix[M02];
			meshpart_instance.matrix[M01] = i.matrix[M10];
			meshpart_instance.matrix[M11] = i.matrix[M11];
			meshpart_instance.matrix[M21] = i.matrix[M12];
			meshpart_instance.matrix[M02] = i.matrix[M20];
			meshpart_instance.matrix[M12] = i.matrix[M21];
			meshpart_instance.matrix[M22] = i.matrix[M22];
			meshpart_instance.pos.x = i.pos.x + meshpart->aabb_correct.z * i.matrix[M20] + meshpart->aabb_correct.y * i.matrix[M10] + meshpart->aabb_correct.x * i.matrix[M00];
			meshpart_instance.pos.y = i.pos.y + meshpart->aabb_correct.z * i.matrix[M21] + meshpart->aabb_correct.y * i.matrix[M11] + meshpart->aabb_correct.x * i.matrix[M01];
			meshpart_instance.pos.z = i.pos.z + meshpart->aabb_correct.z * i.matrix[M22] + meshpart->aabb_correct.y * i.matrix[M12] + meshpart->aabb_correct.x * i.matrix[M02];
			meshpart_instance.transparency = (i.surf_flag & SA1LVL_SURFFLAG_VISIBLE) ? 0.0f : 1.0f;

			//Add to appropriate list
			if (i.surf_flag & SA1LVL_SURFFLAG_SOLID)
				mesh_collision.push_back(meshpart_instance);
			else if (i.surf_flag & SA1LVL_SURFFLAG_VISIBLE)
				mesh_visual.push_back(meshpart_instance);
		}
	}

	//Write RBXMX
	std::ofstream stream_rbxmx(path_rbxmx);
	if (!stream_rbxmx.is_open())
	{
		std::cout << "Failed to open RBXMX " << path_rbxmx << std::endl;
		return 1;
	}

	stream_rbxmx << "<roblox xmlns:xmime=\"http://www.w3.org/2005/05/xmlmime\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://www.roblox.com/roblox.xsd\" version=\"4\">" << std::endl;
		stream_rbxmx << "<Item class = \"Folder\">" << std::endl;
			stream_rbxmx << "<Properties>" << std::endl;
				stream_rbxmx << "<string name=\"Name\">Level</string>" << std::endl;
			stream_rbxmx << "</Properties>" << std::endl;
			stream_rbxmx << "<Item class = \"Folder\">" << std::endl;
				stream_rbxmx << "<Properties>" << std::endl;
					stream_rbxmx << "<string name=\"Name\">Map</string>" << std::endl;
				stream_rbxmx << "</Properties>" << std::endl;
				stream_rbxmx << "<Item class = \"Folder\">" << std::endl;
					stream_rbxmx << "<Properties>" << std::endl;
						stream_rbxmx << "<string name=\"Name\">Collision</string>" << std::endl;
					stream_rbxmx << "</Properties>" << std::endl;
					for (auto &i : mesh_collision)
					{
						stream_rbxmx << "<Item class = \"MeshPart\">" << std::endl;
							stream_rbxmx << "<Properties>" << std::endl;
								stream_rbxmx << "<string name=\"Name\">MeshPart</string>" << std::endl;
								stream_rbxmx << "<bool name=\"Anchored\">true</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"CanCollide\">true</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"CanTouch\">false</bool>" << std::endl;
								stream_rbxmx << "<CoordinateFrame name = \"CFrame\">" << std::endl;
									stream_rbxmx << "<X>" << i.pos.x * scale << "</X>" << std::endl;
									stream_rbxmx << "<Y>" << i.pos.y * scale << "</Y>" << std::endl;
									stream_rbxmx << "<Z>" << i.pos.z * scale << "</Z>" << std::endl;
									stream_rbxmx << "<R00>" << i.matrix[M00] << "</R00>" << std::endl;
									stream_rbxmx << "<R01>" << i.matrix[M01] << "</R01>" << std::endl;
									stream_rbxmx << "<R02>" << i.matrix[M02] << "</R02>" << std::endl;
									stream_rbxmx << "<R10>" << i.matrix[M10] << "</R10>" << std::endl;
									stream_rbxmx << "<R11>" << i.matrix[M11] << "</R11>" << std::endl;
									stream_rbxmx << "<R12>" << i.matrix[M12] << "</R12>" << std::endl;
									stream_rbxmx << "<R20>" << i.matrix[M20] << "</R20>" << std::endl;
									stream_rbxmx << "<R21>" << i.matrix[M21] << "</R21>" << std::endl;
									stream_rbxmx << "<R22>" << i.matrix[M22] << "</R22>" << std::endl;
								stream_rbxmx << "</CoordinateFrame>" << std::endl;
								stream_rbxmx << "<Vector3 name = \"size\">" << std::endl;
									stream_rbxmx << "<X>" << i.meshpart->size.x * scale << "</X>" << std::endl;
									stream_rbxmx << "<Y>" << i.meshpart->size.y * scale << "</Y>" << std::endl;
									stream_rbxmx << "<Z>" << i.meshpart->size.z * scale << "</Z>" << std::endl;
								stream_rbxmx << "</Vector3>" << std::endl;
								stream_rbxmx << "<Vector3 name = \"InitialSize\">" << std::endl;
									stream_rbxmx << "<X>" << i.meshpart->size.x << "</X>" << std::endl;
									stream_rbxmx << "<Y>" << i.meshpart->size.y << "</Y>" << std::endl;
									stream_rbxmx << "<Z>" << i.meshpart->size.z << "</Z>" << std::endl;
								stream_rbxmx << "</Vector3>" << std::endl;
								stream_rbxmx << "<Content name=\"MeshID\"><url>rbxasset://salvl/" << i.meshpart->ind << ".msh</url></Content>" << std::endl;
								stream_rbxmx << "<Content name=\"MeshId\"><url>rbxasset://salvl/" << i.meshpart->ind << ".msh</url></Content>" << std::endl;
								stream_rbxmx << "<float name=\"Transparency\">" << i.transparency << "</float>" << std::endl;
							stream_rbxmx << "</Properties>" << std::endl;
						stream_rbxmx << "</Item>" << std::endl;
					}
				stream_rbxmx << "</Item>" << std::endl;
				for (auto &i : mesh_visual)
				{
					stream_rbxmx << "<Item class = \"MeshPart\">" << std::endl;
						stream_rbxmx << "<Properties>" << std::endl;
							stream_rbxmx << "<string name=\"Name\">MeshPart</string>" << std::endl;
							stream_rbxmx << "<bool name=\"Anchored\">true</bool>" << std::endl;
							stream_rbxmx << "<bool name=\"CanCollide\">false</bool>" << std::endl;
							stream_rbxmx << "<bool name=\"CanTouch\">false</bool>" << std::endl;
							stream_rbxmx << "<CoordinateFrame name = \"CFrame\">" << std::endl;
								stream_rbxmx << "<X>" << i.pos.x * scale << "</X>" << std::endl;
								stream_rbxmx << "<Y>" << i.pos.y * scale << "</Y>" << std::endl;
								stream_rbxmx << "<Z>" << i.pos.z * scale << "</Z>" << std::endl;
								stream_rbxmx << "<R00>" << i.matrix[M00] << "</R00>" << std::endl;
								stream_rbxmx << "<R01>" << i.matrix[M01] << "</R01>" << std::endl;
								stream_rbxmx << "<R02>" << i.matrix[M02] << "</R02>" << std::endl;
								stream_rbxmx << "<R10>" << i.matrix[M10] << "</R10>" << std::endl;
								stream_rbxmx << "<R11>" << i.matrix[M11] << "</R11>" << std::endl;
								stream_rbxmx << "<R12>" << i.matrix[M12] << "</R12>" << std::endl;
								stream_rbxmx << "<R20>" << i.matrix[M20] << "</R20>" << std::endl;
								stream_rbxmx << "<R21>" << i.matrix[M21] << "</R21>" << std::endl;
								stream_rbxmx << "<R22>" << i.matrix[M22] << "</R22>" << std::endl;
							stream_rbxmx << "</CoordinateFrame>" << std::endl;
							stream_rbxmx << "<Vector3 name = \"size\">" << std::endl;
								stream_rbxmx << "<X>" << i.meshpart->size.x * scale << "</X>" << std::endl;
								stream_rbxmx << "<Y>" << i.meshpart->size.y * scale << "</Y>" << std::endl;
								stream_rbxmx << "<Z>" << i.meshpart->size.z * scale << "</Z>" << std::endl;
							stream_rbxmx << "</Vector3>" << std::endl;
							stream_rbxmx << "<Vector3 name = \"InitialSize\">" << std::endl;
								stream_rbxmx << "<X>" << i.meshpart->size.x << "</X>" << std::endl;
								stream_rbxmx << "<Y>" << i.meshpart->size.y << "</Y>" << std::endl;
								stream_rbxmx << "<Z>" << i.meshpart->size.z << "</Z>" << std::endl;
							stream_rbxmx << "</Vector3>" << std::endl;
							stream_rbxmx << "<Content name=\"MeshID\"><url>rbxasset://salvl/" << i.meshpart->ind << ".msh</url></Content>" << std::endl;
							stream_rbxmx << "<Content name=\"MeshId\"><url>rbxasset://salvl/" << i.meshpart->ind << ".msh</url></Content>" << std::endl;
							stream_rbxmx << "<float name=\"Transparency\">" << i.transparency << "</float>" << std::endl;
						stream_rbxmx << "</Properties>" << std::endl;
					stream_rbxmx << "</Item>" << std::endl;
				}
			stream_rbxmx << "</Item>" << std::endl;
		stream_rbxmx << "</Item>" << std::endl;
	stream_rbxmx << "</roblox>" << std::endl;

	return 0;
}
