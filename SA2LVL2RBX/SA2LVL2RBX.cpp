#include "SALVL2RBX.h"

#include <iostream>

#include "LandtableInfo.h"

//SA2LVL types
#define SA2LVL_OBJFLAG_NO_POSITION 0x01
#define SA2LVL_OBJFLAG_NO_ROTATE   0x02
#define SA2LVL_OBJFLAG_NO_SCALE    0x04
#define SA2LVL_OBJFLAG_NO_DISPLAY  0x08
#define SA2LVL_OBJFLAG_NO_CHILDREN 0x10
#define SA2LVL_OBJFLAG_ROTATE_XYZ  0x20
#define SA2LVL_OBJFLAG_NO_ANIMATE  0x40
#define SA2LVL_OBJFLAG_NO_MORPH    0x80

#define SA2LVL_SURFFLAG_SOLID          0x1
#define SA2LVL_SURFFLAG_WATER          0x2
#define SA2LVL_SURFFLAG_DIGGABLE       0x20
#define SA2LVL_SURFFLAG_UNCLIMBABLE    0x80
#define SA2LVL_SURFFLAG_STAIRS         0x100
#define SA2LVL_SURFFLAG_HURT           0x400
#define SA2LVL_SURFFLAG_FEET_SOUND     0x800
#define SA2LVL_SURFFLAG_CANNOT_LAND    0x1000
#define SA2LVL_SURFFLAG_WATER_NO_ALPHA 0x2000
#define SA2LVL_SURFFLAG_NO_SHADOW      0x80000
#define SA2LVL_SURFFLAG_ACCELERATE     0x100000
#define SA2LVL_SURFFLAG_NO_FOG         0x400000
#define SA2LVL_SURFFLAG_DYNAMIC        0x8000000
#define SA2LVL_SURFFLAG_UNK1           0x20000000
#define SA2LVL_SURFFLAG_UNK2           0x40000000
#define SA2LVL_SURFFLAG_VISIBLE        0x80000000

//SA2LVL loader
void SA2LVL_IndexVertexBasic(SALVL_MeshPart &meshpart, NJS_MODEL *model, NJS_MESHSET *meshset, Sint16 i, Sint16 j)
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
		if (meshpart.matflags & NJD_FLAG_FLIP_U)
			vertex.tex.x *= 0.5f;
		if (meshpart.matflags & NJD_FLAG_FLIP_V)
			vertex.tex.y *= 0.5f;
	}

	//Add vertex and push index
	meshpart.indices.push_back(meshpart.AddVertex(vertex));
}

void SA2LVL_LoadBasic(SALVL &lvl, COL *colp, NJS_MODEL *model)
{
	//Read mesh parts
	SALVL_Mesh mesh;

	NJS_MESHSET *meshset = model->meshsets;
	for (Uint16 k = 0; k < model->nbMeshset; k++, meshset++)
	{
		//Create mesh part
		Uint16 material = meshset->type_matId & 0x3FFF;
		SALVL_MeshPart *meshpart = &mesh.parts[material];

		//Read material
		NJS_MATERIAL *nmaterial;
		if (material < model->nbMat)
			nmaterial = &model->mats[material];
		else
			nmaterial = nullptr;

		if (nmaterial != nullptr)
		{
			meshpart->matflags = nmaterial->attrflags;

			meshpart->texture = (nmaterial->attrflags & NJD_FLAG_USE_TEXTURE) ? &lvl.textures[nmaterial->attr_texId] : nullptr;
			meshpart->diffuse = ((Uint32)nmaterial->diffuse.argb.r << 16) | ((Uint32)nmaterial->diffuse.argb.g << 8) | ((Uint32)nmaterial->diffuse.argb.b);
		}

		//Read vertices
		Uint16 polytype = meshset->type_matId >> 14;
		Sint16 *indp = meshset->meshes;
		for (Uint16 p = 0, j = 0; p < meshset->nbMesh; p++)
		{
			switch (polytype)
			{
				case 0: //Triangles
				{
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[0], j + 0);
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[1], j + 1);
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[2], j + 2);
					indp += 3;
					j += 3;
					break;
				}
				case 1: //Quads
				{
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[0], j + 0);
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[1], j + 1);
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[2], j + 2);
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[2], j + 2);
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[1], j + 1);
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[3], j + 3);
					indp += 4;
					j += 4;
					break;
				}
				case 3: //Strip
				{
					Uint16 first = *indp++;
					for (Uint16 l = 0; l < (first & 0x7FFF) - 2; l++, j++)
					{
						first ^= 0x8000;
						if (first & 0x8000)
						{
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[l + 0], j + 0);
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[l + 1], j + 1);
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[l + 2], j + 2);
						}
						else
						{
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[l + 1], j + 1);
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[l + 0], j + 0);
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, indp[l + 2], j + 2);
						}
					}
					j += 2;
					indp += (first & 0x7FFF);
					break;
				}
			}
		}
	}

	//Push mesh to map and instance
	lvl.meshes[model] = mesh;
}

void SA2LVL_LoadChunk(SALVL &lvl, COL *colp, NJS_CNK_MODEL *model)
{

}

int SA2LVL_Loader(SALVL &lvl, std::string path_lvl)
{
	//Load landtable from path
	LandTableInfo landtable_lvl(path_lvl);
	
	LandTable *landtable = landtable_lvl.getlandtable();
	if (landtable == nullptr)
	{
		std::cout << "Failed to convert LVL " << path_lvl << std::endl;
		system("pause");
		return 1;
	}

	//Read COLs
	std::cout << "Reading COLs..." << std::endl;

	COL *colp = landtable->COLList;
	for (int16_t i = 0; i < landtable->COLCount; i++, colp++)
	{
		//Convert object
		NJS_OBJECT *object = colp->Model;
		if (object != nullptr)
		{
			if (object->model != nullptr)
			{
				//Create mesh instance
				SALVL_MeshInstance meshinstance;
				meshinstance.surf_flag |= SALVL_FLAG_REMAP(colp->Flags, SA2LVL_SURFFLAG_SOLID, SALVL_SURFFLAG_SOLID);
				meshinstance.surf_flag |= SALVL_FLAG_REMAP(colp->Flags, SA2LVL_SURFFLAG_VISIBLE, SALVL_SURFFLAG_VISIBLE);

				if (object->evalflags & SA2LVL_OBJFLAG_ROTATE_XYZ)
				{
					Reimp_njRotateZ(meshinstance.matrix, object->ang[2]);
					Reimp_njRotateY(meshinstance.matrix, object->ang[1]);
					Reimp_njRotateX(meshinstance.matrix, object->ang[0]);
				}
				else
				{
					Reimp_njRotateY(meshinstance.matrix, object->ang[1]);
					Reimp_njRotateX(meshinstance.matrix, object->ang[0]);
					Reimp_njRotateZ(meshinstance.matrix, object->ang[2]);
				}

				meshinstance.pos.x = object->pos[0];
				meshinstance.pos.y = object->pos[1];
				meshinstance.pos.z = object->pos[2];

				//Get mesh
				auto meshind = lvl.meshes.find(object->model);
				if (meshind != lvl.meshes.end())
				{
					//Use already created mesh instace
					meshinstance.mesh = &meshind->second;
				}
				else
				{
					//Determine model loader
					if ((landtable->VisibleModelCount >= 0) ? (i < landtable->VisibleModelCount) : ((colp->Flags & SA2LVL_SURFFLAG_VISIBLE) != 0))
						SA2LVL_LoadChunk(lvl, colp, object->getchunkmodel());
					else
						SA2LVL_LoadBasic(lvl, colp, object->getbasicmodel());
					meshinstance.mesh = &lvl.meshes[object->model];
				}

				//Push mesh instance
				lvl.meshinstances.push_back(meshinstance);
			}
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	//Run SALVL2RBX program
	return SALVL2RBX(argc, argv, SA2LVL_Loader);
}