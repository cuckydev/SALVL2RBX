#include "SALVL2RBX.h"

#include <iostream>

#include "LandtableInfo.h"

//SA2LVL types
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

//SA2LVL chunk loader
struct SA2LVL_PolyChunk_Material
{
	//Material information
	Sint16 texflags = 0;
	Sint16 texid = -1;
	Uint8 r = 255, g = 255, b = 255, a = 255;

	inline bool operator==(const SA2LVL_PolyChunk_Material &rhs)
	{
		return
			texflags == rhs.texflags && texid == rhs.texid &&
			r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
	}
	inline bool operator!=(const SA2LVL_PolyChunk_Material &rhs)
	{
		return !(*this == rhs);
	}
};

struct SA2LVL_Chunk
{
	//Chunk data
	std::unordered_map<Sint32, SALVL_Vertex> vertex;
	std::vector<SA2LVL_PolyChunk_Material> materials;
	SALVL_Mesh mesh;

	Uint16 AddMaterial(SA2LVL_PolyChunk_Material &adder)
	{
		//Check if identical vertex already exists
		Uint16 j = 0;
		for (auto &i : materials)
		{
			if (i == adder)
				return j;
			j++;
		}

		//Push new material
		materials.push_back(adder);
		return j;
	}
};

void SA2LVL_LoadChunk(SALVL &lvl, COL *colp, NJS_CNK_MODEL *model)
{
	SA2LVL_Chunk chunk_model;

	//Read vertex chunks
	for (Sint32 *chunkp = model->vlist; chunkp != nullptr; chunkp = NextChunk(chunkp))
	{
		//Read chunk header
		Sint32 type = chunkp[0] & 0xFF;
		Sint32 num_verts = chunkp[1] >> 16;
		Sint32 index_offset = chunkp[1] & 0xFFFF;

		//Read chunk data by header type
		switch (type)
		{
			case NJD_CV:
			{
				//Read vertex data
				struct NJS_CV { NJS_VECTOR pos, nor; };
				NJS_CV *chunkcast = (NJS_CV*)(chunkp + 2);

				for (Sint32 i = 0; i < num_verts; i++, chunkcast++)
				{
					SALVL_Vertex v;
					v.pos.x = chunkcast->pos.x;
					v.pos.y = chunkcast->pos.y;
					v.pos.z = chunkcast->pos.z;

					chunk_model.vertex[index_offset + i] = v;
				}
				break;
			}
			case NJD_CV_VN:
			{
				//Read vertex and normal data
				struct NJS_CV_VN { NJS_VECTOR pos, nor; };
				NJS_CV_VN *chunkcast = (NJS_CV_VN*)(chunkp + 2);

				for (Sint32 i = 0; i < num_verts; i++, chunkcast++)
				{
					SALVL_Vertex v;
					v.pos.x = chunkcast->pos.x;
					v.pos.y = chunkcast->pos.y;
					v.pos.z = chunkcast->pos.z;
					v.nor.x = chunkcast->nor.x;
					v.nor.y = chunkcast->nor.y;
					v.nor.z = chunkcast->nor.z;

					chunk_model.vertex[index_offset + i] = v;
				}
				break;
			}
			case NJD_CV_D8:
			{
				//Read vertex and diffuse data
				struct NJS_CV_D8 { NJS_VECTOR pos; NJS_COLOR col; };
				NJS_CV_D8 *chunkcast = (NJS_CV_D8*)(chunkp + 2);

				for (Sint32 i = 0; i < num_verts; i++, chunkcast++)
				{
					SALVL_Vertex v;
					v.pos.x = chunkcast->pos.x;
					v.pos.y = chunkcast->pos.y;
					v.pos.z = chunkcast->pos.z;
					v.r = chunkcast->col.argb.r;
					v.g = chunkcast->col.argb.g;
					v.b = chunkcast->col.argb.b;
					v.a = chunkcast->col.argb.a;

					chunk_model.vertex[index_offset + i] = v;
				}
				break;
			}
			case NJD_CE:
			case NJD_CN:
			{
				//End of chunk or null chunk
				break;
			}
			default:
			{
				//All types for SA2LVL should be accounted for
				std::cout << "Unrecognized vertex chunk type " << type << std::endl;
				break;
			}
		}
	}

	//Read polygon chunks
	SA2LVL_PolyChunk_Material material;

	for (Sint16 *chunkp = model->plist; chunkp != nullptr; chunkp = NextChunk(chunkp))
	{
		//Read chunk header
		Sint16 type = chunkp[0] & 0xFF;

		//Read chunk data by header type
		switch (type)
		{
			case NJD_CM_D:
			case NJD_CM_DA:
			case NJD_CM_DS:
			case NJD_CM_DAS:
			{
				//Read material data
				struct NJS_CM { NJS_COLOR col; };
				NJS_CM *chunkcast = (NJS_CM*)(chunkp + 1);

				material.r = chunkcast->col.argb.r;
				material.g = chunkcast->col.argb.g;
				material.b = chunkcast->col.argb.b;
				material.a = chunkcast->col.argb.a;
				break;
			}
			case NJD_CT_TID:
			{
				//Read texture data
				material.texflags = chunkp[0] & 0xFF00;
				material.texid = chunkp[1] & 0x1FFF;
				break;
			}
			case NJD_CS:
			{
				//Get mesh part
				SALVL_MeshPart *meshpart = &chunk_model.mesh.parts[chunk_model.AddMaterial(material)];
				
				//Set material data
				meshpart->texture = nullptr;
				
				//Read chunk header
				Sint16 strip_count = chunkp[2] & 0x3FFF;

				Sint16 *pchunkp = chunkp + 3;
				for (Sint16 j = 0; j < strip_count; j++)
				{
					//Read index data
					std::vector<Sint16> rawindices;
					Uint16 indices = ((pchunkp[0] < 0) ? -pchunkp[0] : pchunkp[0]);
					Uint16 reversed = pchunkp[0] & 0x8000;
					pchunkp++;

					for (Uint16 i = 0; i < indices; i++)
					{
						//Get vertex
						SALVL_Vertex v = chunk_model.vertex[*pchunkp++];

						//Add vertex to part
						rawindices.push_back(meshpart->AddVertex(v));
					}

					//Unstrip indices
					for (Uint16 i = 0; i < indices - 2; i++)
					{
						reversed ^= 0x8000;
						if (reversed & 0x8000)
							meshpart->indices.push_back({rawindices[i + 0], rawindices[i + 1], rawindices[i + 2]});
						else
							meshpart->indices.push_back({rawindices[i + 1], rawindices[i + 0], rawindices[i + 2]});
					}
				}
				break;
			}
			case NJD_CS_UVN:
			case NJD_CS_UVH:
			{
				//Get mesh part
				SALVL_MeshPart *meshpart = &chunk_model.mesh.parts[chunk_model.AddMaterial(material)];
				
				//Set material data
				meshpart->matflags |= NJD_FLAG_USE_TEXTURE;
				meshpart->matflags |= SALVL_FLAG_REMAP(material.texflags, NJD_FFL_U, NJD_FLAG_FLIP_U);
				meshpart->matflags |= SALVL_FLAG_REMAP(material.texflags, NJD_FFL_V, NJD_FLAG_FLIP_V);
				meshpart->texture = (material.texid >= 0) ? &lvl.textures[material.texid] : nullptr;

				float du = ((type == NJD_CS_UVN) ? 256.0f : 1024.0f) * ((meshpart->matflags & NJD_FLAG_FLIP_U) ? 2.0f : 1.0f);
				float dv = ((type == NJD_CS_UVN) ? 256.0f : 1024.0f) * ((meshpart->matflags & NJD_FLAG_FLIP_V) ? 2.0f : 1.0f);

				//Read chunk header
				Sint16 strip_count = chunkp[2] & 0x3FFF;

				Sint16 *pchunkp = chunkp + 3;
				for (Sint16 j = 0; j < strip_count; j++)
				{
					//Read index data
					std::vector<Sint16> rawindices;
					Uint16 indices = ((pchunkp[0] < 0) ? -pchunkp[0] : pchunkp[0]);
					Uint16 reversed = pchunkp[0] & 0x8000;
					pchunkp++;

					for (Uint16 i = 0; i < indices; i++)
					{
						//Get vertex
						SALVL_Vertex v = chunk_model.vertex[*pchunkp++];

						struct NJD_CS_UV { NJS_TEX tex; };
						NJD_CS_UV *chunkcast = (NJD_CS_UV *)pchunkp;
						pchunkp += sizeof(NJD_CS_UV) / sizeof(*pchunkp);

						v.tex.x = (Float)chunkcast->tex.u / du;
						v.tex.y = (Float)chunkcast->tex.v / dv;

						//Add vertex to part
						rawindices.push_back(meshpart->AddVertex(v));
					}

					//Unstrip indices
					for (Uint16 i = 0; i < indices - 2; i++)
					{
						reversed ^= 0x8000;
						if (reversed & 0x8000)
							meshpart->indices.push_back({rawindices[i + 0], rawindices[i + 1], rawindices[i + 2]});
						else
							meshpart->indices.push_back({rawindices[i + 1], rawindices[i + 0], rawindices[i + 2]});
					}
				}
				break;
			}
			case NJD_CE:
			case NJD_CN:
			{
				//End of chunk or null chunk
				break;
			}
			default:
			{
				//All types for SA2LVL should be accounted for
				std::cout << "Unrecognized polygon chunk type " << type << std::endl;
				break;
			}
		}
	}

	//Push mesh to map
	lvl.meshes[model] = chunk_model.mesh;
}

//SA2LVL basic loader
void SA2LVL_IndexVertexBasic(SALVL_MeshPart &meshpart, NJS_MODEL *model, NJS_MESHSET *meshset, SALVL_MeshFace i, SALVL_MeshFace j)
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
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[0], indp[1], indp[2]}, {(Sint16)(l + 0), (Sint16)(l + 1), (Sint16)(l + 2)});
					indp += 3;
					l += 3;
					break;
				}
				case 1: //Quads
				{
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[2], indp[1], indp[3]}, {(Sint16)(l + 2), (Sint16)(l + 1), (Sint16)(l + 2)});
					SA2LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[2], indp[1], indp[3]}, {(Sint16)(l + 2), (Sint16)(l + 1), (Sint16)(l + 3)});
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
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[x + 0], indp[x + 1], indp[x + 2]}, {(Sint16)(l + 0), (Sint16)(l + 1), (Sint16)(l + 2)});
						else
							SA2LVL_IndexVertexBasic(*meshpart, model, meshset, {indp[x + 1], indp[x + 0], indp[x + 2]}, {(Sint16)(l + 1), (Sint16)(l + 0), (Sint16)(l + 2)});
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

//SA2LVL loader
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
					//Load mesh of appropriate type
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
