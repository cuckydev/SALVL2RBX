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

//SA2LVL chunk loader
struct SA2LVL_VertexChunk
{
	//Header
	Sint32 type, num_verts, index_offset;

	//Vertex data
	std::vector<SALVL_Vertex> vertex;
};

struct SA2LVL_PolyChunk_Material
{
	//Material information
	Sint16 texid;
	Uint8 r, g, b, a;

	inline bool operator==(const SA2LVL_PolyChunk_Material &rhs)
	{
		return
			texid == rhs.texid &&
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
	std::vector<SA2LVL_VertexChunk> vertex_chunks;
	std::vector<SA2LVL_PolyChunk_Material> materials;

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
	SA2LVL_Chunk chunk;

	//Read vertex chunks
	std::vector<SA2LVL_VertexChunk> vertex_chunks;

	for (Sint32 *chunkp = model->vlist; chunkp != nullptr; chunkp = NextChunk(chunkp))
	{
		//Read chunk header
		SA2LVL_VertexChunk chunk;

		chunk.type = chunkp[0] & 0xFF;
		chunk.num_verts = chunkp[1] >> 16;
		chunk.index_offset = chunkp[1] & 0xFFFF;

		//Read chunk data by header type
		switch (chunk.type)
		{
			case NJD_CV:
			{
				//Read vertex data
				struct NJS_CV { NJS_VECTOR pos, nor; };
				NJS_CV *chunkcast = (NJS_CV*)(chunkp + 2);

				for (Sint32 i = 0; i < chunk.num_verts; i++, chunkcast++)
				{
					SALVL_Vertex v;
					v.pos.x = chunkcast->pos.x;
					v.pos.y = chunkcast->pos.y;
					v.pos.z = chunkcast->pos.z;

					chunk.vertex.push_back(v);
				}

				//Push chunk
				vertex_chunks.push_back(chunk);
				break;
			}
			case NJD_CV_VN:
			{
				//Read vertex and normal data
				struct NJS_CV_VN { NJS_VECTOR pos, nor; };
				NJS_CV_VN *chunkcast = (NJS_CV_VN*)(chunkp + 2);

				for (Sint32 i = 0; i < chunk.num_verts; i++, chunkcast++)
				{
					SALVL_Vertex v;
					v.pos.x = chunkcast->pos.x;
					v.pos.y = chunkcast->pos.y;
					v.pos.z = chunkcast->pos.z;
					v.nor.x = chunkcast->nor.x;
					v.nor.y = chunkcast->nor.y;
					v.nor.z = chunkcast->nor.z;

					chunk.vertex.push_back(v);
				}

				//Push chunk
				vertex_chunks.push_back(chunk);
				break;
			}
			case NJD_CV_D8:
			{
				//Read vertex and diffuse data
				struct NJS_CV_D8 { NJS_VECTOR pos; NJS_COLOR col; };
				NJS_CV_D8 *chunkcast = (NJS_CV_D8*)(chunkp + 2);

				for (Sint32 i = 0; i < chunk.num_verts; i++, chunkcast++)
				{
					SALVL_Vertex v;
					v.pos.x = chunkcast->pos.x;
					v.pos.y = chunkcast->pos.y;
					v.pos.z = chunkcast->pos.z;
					v.r = chunkcast->col.argb.r;
					v.g = chunkcast->col.argb.g;
					v.b = chunkcast->col.argb.b;
					v.a = chunkcast->col.argb.a;

					chunk.vertex.push_back(v);
				}

				//Push chunk
				vertex_chunks.push_back(chunk);
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
				//Not necessarily a bad thing
				std::cout << "Unrecognized chunk type " << chunk.type << std::endl;
				break;
			}
		}
	}

	//Read polygon chunks
	for (Sint16 *chunkp = model->plist; chunkp != nullptr; chunkp = NextChunk(chunkp))
	{
		//Read chunk header
	}
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

			meshpart->texture = (nmaterial->attrflags & NJD_FLAG_USE_TEXTURE) ? &lvl.textures[nmaterial->attr_texId] : nullptr;
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

	//Push mesh to map and instance
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
