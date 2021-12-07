#include "SALVL2RBX.h"

#include <iostream>

#include "LandtableInfo.h"

//SA1LVL types
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

//SA1LVL basic loader
void SA1LVL_IndexVertexBasic(SALVL_MeshPart &meshpart, NJS_MODEL_SADX *model, NJS_MESHSET_SADX *meshset, SALVL_MeshFace i, SALVL_MeshFace j)
{
	SALVL_Vertex vertex[3];
	SALVL_MeshFace pi;

	for (int k = 0; k < 3; k++)
	{
		//Construct vertex
		vertex[k].pos.x = model->points[i.i[k]].x;
		vertex[k].pos.y = model->points[i.i[k]].y;
		vertex[k].pos.z = model->points[i.i[k]].z;
		vertex[k].nor.x = model->normals[i.i[k]].x;
		vertex[k].nor.y = model->normals[i.i[k]].y;
		vertex[k].nor.z = model->normals[i.i[k]].z;
		if (meshset->vertuv != nullptr)
		{
			vertex[k].tex.x = meshset->vertuv[j.i[k]].u / 256.0f;
			vertex[k].tex.y = meshset->vertuv[j.i[k]].v / 256.0f;
			if (meshpart.matflags & NJD_FLAG_FLIP_U)
				vertex[k].tex.x *= 0.5f;
			if (meshpart.matflags & NJD_FLAG_FLIP_V)
				vertex[k].tex.y *= 0.5f;
		}

		//Add vertex and get index
		pi.i[k] = meshpart.AddVertex(vertex[k]);
	}

	//Push indices
	meshpart.indices.push_back(pi);
}

void SA1LVL_LoadBasic(SALVL &lvl, COL *colp, NJS_MODEL_SADX *model)
{
	//Read mesh parts
	SALVL_Mesh mesh;

	NJS_MESHSET_SADX *meshset = model->meshsets;
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

			meshpart->texture = (meshpart->matflags & NJD_FLAG_USE_TEXTURE) ? &lvl.textures[nmaterial->attr_texId] : nullptr;
			meshpart->diffuse = ((Uint32)nmaterial->diffuse.argb.r << 16) | ((Uint32)nmaterial->diffuse.argb.g << 8) | ((Uint32)nmaterial->diffuse.argb.b);
		}

		//Read vertices
		Uint16 polytype = meshset->type_matId >> 14;
		Sint16 *indp = meshset->meshes;
		Sint16 l = 0;

		for (Uint16 p = 0; p < meshset->nbMesh; p++)
		{
			switch (polytype)
			{
				case 0: //Triangles
				{
					SA1LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[0], indp[1], indp[2]}, {(Sint16)(l + 0), (Sint16)(l + 1), (Sint16)(l + 2)});
					indp += 3;
					l += 3;
					break;
				}
				case 1: //Quads
				{
					SA1LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[2], indp[1], indp[3]}, {(Sint16)(l + 2), (Sint16)(l + 1), (Sint16)(l + 2)});
					SA1LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[2], indp[1], indp[3]}, {(Sint16)(l + 2), (Sint16)(l + 1), (Sint16)(l + 3)});
					indp += 4;
					l += 4;
					break;
				}
				case 3: //Strip
				{
					Uint16 first = *indp++;
					for (Uint16 x = 0; x < (first & 0x7FFF) - 2; x++, l++)
					{
						first ^= 0x8000;
						if (first & 0x8000)
							SA1LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[x + 0], indp[x + 1], indp[x + 2]}, {(Sint16)(l + 0), (Sint16)(l + 1), (Sint16)(l + 2)});
						else
							SA1LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[x + 1], indp[x + 0], indp[x + 2]}, {(Sint16)(l + 1), (Sint16)(l + 0), (Sint16)(l + 2)});
					}
					l += 2;
					indp += (first & 0x7FFF);
					break;
				}
			}
		}
	}

	//Push mesh to map
	lvl.meshes[model] = mesh;
}

//SA1LVL loader
int SA1LVL_Loader(SALVL &lvl, std::string path_lvl)
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

	COL *colp = landtable->Col;
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
				meshinstance.surf_flag |= SALVL_FLAG_REMAP(colp->Flags, SA1LVL_SURFFLAG_SOLID, SALVL_SURFFLAG_SOLID);
				meshinstance.surf_flag |= SALVL_FLAG_REMAP(colp->Flags, SA1LVL_SURFFLAG_VISIBLE, SALVL_SURFFLAG_VISIBLE);

				if (object->evalflags & NJD_EVAL_ZXY_ANG)
				{
					Reimp_njRotateY(meshinstance.matrix, object->ang[1]);
					Reimp_njRotateX(meshinstance.matrix, object->ang[0]);
					Reimp_njRotateZ(meshinstance.matrix, object->ang[2]);
				}
				else
				{
					Reimp_njRotateZ(meshinstance.matrix, object->ang[2]);
					Reimp_njRotateY(meshinstance.matrix, object->ang[1]);
					Reimp_njRotateX(meshinstance.matrix, object->ang[0]);
				}

				meshinstance.pos.x = object->pos[0];
				meshinstance.pos.y = object->pos[1];
				meshinstance.pos.z = object->pos[2];

				//Get mesh
				auto meshind = lvl.meshes.find(object->model);
				if (meshind != lvl.meshes.end())
				{
					//Use already created mesh instance
					meshinstance.mesh = &meshind->second;
				}
				else
				{
					//Load mesh
					SA1LVL_LoadBasic(lvl, colp, object->getbasicdxmodel());
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
	return SALVL2RBX(argc, argv, SA1LVL_Loader);
}
