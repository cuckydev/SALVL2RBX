#include "SALVL2RBX.h"

#include <iostream>

#include "LandtableInfo.h"

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
			//Get model to use
			NJS_MODEL_SADX *model = object->getbasicdxmodel();

			if (model != nullptr)
			{
				//Create mesh instance
				SALVL_MeshInstance meshinstance;
				meshinstance.surf_flag = colp->Flags;

				if (object->evalflags & SALVL_OBJFLAG_ROTATE_XYZ)
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
				auto meshind = lvl.meshes.find(model);
				if (meshind != lvl.meshes.end())
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
								meshpart->IndexVertex(model, meshset, indp[0], j + 0);
								meshpart->IndexVertex(model, meshset, indp[1], j + 1);
								meshpart->IndexVertex(model, meshset, indp[2], j + 2);
								indp += 3;
								j += 3;
								break;
							}
							case 1: //Quads
							{
								meshpart->IndexVertex(model, meshset, indp[0], j + 0);
								meshpart->IndexVertex(model, meshset, indp[1], j + 1);
								meshpart->IndexVertex(model, meshset, indp[2], j + 2);
								meshpart->IndexVertex(model, meshset, indp[2], j + 2);
								meshpart->IndexVertex(model, meshset, indp[1], j + 1);
								meshpart->IndexVertex(model, meshset, indp[3], j + 3);
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
										meshpart->IndexVertex(model, meshset, indp[l + 0], j + 0);
										meshpart->IndexVertex(model, meshset, indp[l + 1], j + 1);
										meshpart->IndexVertex(model, meshset, indp[l + 2], j + 2);
									}
									else
									{
										meshpart->IndexVertex(model, meshset, indp[l + 1], j + 1);
										meshpart->IndexVertex(model, meshset, indp[l + 0], j + 0);
										meshpart->IndexVertex(model, meshset, indp[l + 2], j + 2);
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
					meshinstance.mesh = &lvl.meshes[model];
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