#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <limits>
#include <set>
#include <algorithm>
#include <regex>

#include "LandTableInfo.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//SA1 types
#define SALVL_OBJFLAG_NO_POSITION 0x01
#define SALVL_OBJFLAG_NO_ROTATE   0x02
#define SALVL_OBJFLAG_NO_SCALE    0x04
#define SALVL_OBJFLAG_NO_DISPLAY  0x08
#define SALVL_OBJFLAG_NO_CHILDREN 0x10
#define SALVL_OBJFLAG_ROTATE_XYZ  0x20
#define SALVL_OBJFLAG_NO_ANIMATE  0x40
#define SALVL_OBJFLAG_NO_MORPH    0x80

typedef Uint32 SA1LVL_SurfFlag;
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
	SA1LVL_SurfFlag surf_flag = 0;
};

struct SALVL_MeshPartInstance
{
	//Referenced mesh part
	SALVL_MeshPart *meshpart = nullptr;

	//Positioning
	NJS_MATRIX matrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	NJS_VECTOR pos = {};

	//Flags
	SA1LVL_SurfFlag surf_flag = 0;
};

#include <wincrypt.h> //ugh
#ifndef CRYPT_STRING_BASE64
#define CRYPT_STRING_BASE64 1 //Shut up IntelliSense
#endif
#include "md5.h"

struct SALVL_CSGMesh
{
	//Encoded data
	std::string enc_base64; //Base64 encoded
	std::string enc_hash; //MD5 hash

	std::string Base64(Uint8 *data, size_t size)
	{
		//Encode to C string
		DWORD base64_len = 0;
		if (CryptBinaryToStringA(data, size, CRYPT_STRING_BASE64, NULL, &base64_len))
		{
			LPSTR base64_str = (LPSTR)malloc(base64_len);
			if (base64_str != nullptr && CryptBinaryToStringA(data, size, CRYPT_STRING_BASE64, base64_str, &base64_len))
			{
				//Get encoded string
				std::string resultado = base64_str;
				free(base64_str);
				resultado.erase(std::remove(resultado.begin(), resultado.end(), '\r'), resultado.end());
				resultado.erase(std::remove(resultado.begin(), resultado.end(), '\n'), resultado.end());

				//Break line appropriately
				std::string result;
				for (size_t i = 0; i < resultado.size(); i += 72)
				{
					if (!result.empty())
						result.push_back('\n');
					result += resultado.substr(i, 72);
				}
				return result;
			}
		}
		return "";
	}

	void Encode(std::vector<Uint8> &data)
	{
		//Encode to Base64 (UGH)
		enc_base64 = Base64(data.data(), data.size());

		//Encode to MD5 then to Base64
		MD5 md5;
		md5.Init();
		md5.Update((unsigned char*)enc_base64.c_str(), enc_base64.size());
		md5.Final();

		enc_hash = Base64(md5.digestRaw, 16);
	}
};

//Ninja reimplementation
void Reimp_njRotateX(NJS_MATRIX cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	Float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	Float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	Float m10 = cframe[M10];
	Float m11 = cframe[M11];
	Float m12 = cframe[M12];
	Float m13 = cframe[M13];

	cframe[M10] = sin * cframe[M20] + m10 * cos;
	cframe[M11] = sin * cframe[M21] + m11 * cos;
	cframe[M12] = sin * cframe[M22] + m12 * cos;
	cframe[M13] = sin * cframe[M23] + m13 * cos;
	cframe[M20] = cos * cframe[M20] - m10 * sin;
	cframe[M21] = cos * cframe[M21] - m11 * sin;
	cframe[M22] = cos * cframe[M22] - m12 * sin;
	cframe[M23] = cos * cframe[M23] - m13 * sin;
}

void Reimp_njRotateY(Float *cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	Float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	Float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	Float m00 = cframe[M00];
	Float m01 = cframe[M01];
	Float m02 = cframe[M02];
	Float m03 = cframe[M03];

	cframe[M00] = m00 * cos - sin * cframe[M20];
	cframe[M01] = m01 * cos - sin * cframe[M21];
	cframe[M02] = m02 * cos - sin * cframe[M22];
	cframe[M03] = m03 * cos - sin * cframe[M23];
	cframe[M20] = cos * cframe[M20] + m00 * sin;
	cframe[M21] = cos * cframe[M21] + m01 * sin;
	cframe[M22] = cos * cframe[M22] + m02 * sin;
	cframe[M23] = cos * cframe[M23] + m03 * sin;
}

void Reimp_njRotateZ(Float *cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	Float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	Float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	Float m00 = cframe[M00];
	Float m01 = cframe[M01];
	Float m02 = cframe[M02];
	Float m03 = cframe[M03];

	cframe[M00] = m00 * cos + sin * cframe[M10];
	cframe[M01] = m01 * cos + sin * cframe[M11];
	cframe[M02] = m02 * cos + sin * cframe[M12];
	cframe[M03] = m03 * cos + sin * cframe[M13];
	cframe[M10] = cos * cframe[M10] - m00 * sin;
	cframe[M11] = cos * cframe[M11] - m01 * sin;
	cframe[M12] = cos * cframe[M12] - m02 * sin;
	cframe[M13] = cos * cframe[M13] - m03 * sin;
}

//File writes
void Write16(std::ofstream &stream, Uint16 x)
{
	stream.put((char)x);
	stream.put((char)(x >> 8));
}
void Write32(std::ofstream &stream, Uint32 x)
{
	stream.put((char)x);
	stream.put((char)(x >> 8));
	stream.put((char)(x >> 16));
	stream.put((char)(x >> 24));
}
void WriteFloat(std::ofstream &stream, float x)
{
	Write32(stream, *(Uint32*)&x);
}

template<typename T> void Push16(std::vector<T> &stream, Uint16 x)
{
	stream.push_back((T)x);
	stream.push_back((T)(x >> 8));
}
template<typename T> void Push32(std::vector<T> &stream, Uint32 x)
{
	stream.push_back((T)x);
	stream.push_back((T)(x >> 8));
	stream.push_back((T)(x >> 16));
	stream.push_back((T)(x >> 24));
}
template<typename T> void PushFloat(std::vector<T> &stream, float x)
{
	Push32<T>(stream, *(Uint32*)&x);
}

//Entry point
int main(int argc, char *argv[])
{
	//Check arguments
	if (argc < 5)
	{
		std::cout << "usage: SA1LVL2RBX upload/content_directory scale sa1lvl texlist_index_txt" << std::endl;
		return 0;
	}

	//Get content folder
	std::string path_content = argv[1];

	std::string roblosecurity;
	bool upload = false;

	if (path_content == "upload")
	{
		//Upload mode, generic content path
		path_content = "./";
		upload = true;

		//WARNING
		std::cout << "WARNING:" << std::endl;
		std::cout << "By using upload mode, you agree to two terms." << std::endl;
		std::cout << " 1. This program will retrieve your Roblox Studio session (ROBLOSECURITY) to upload assets onto Roblox. Note that this program does not communicate to any servers other than Roblox's." << std::endl;
		std::cout << " 2. Roblox may, whether deserved or not, take moderation action against your account for the assets uploaded. Please don't run this logged into your main account." << std::endl;
		std::cout << std::endl << "IF YOU AGREE TO THESE TERMS, ENTER 'y'" << std::endl;

		char pressed;
		std::cin >> pressed;
		if (pressed != 'y' || pressed == 'Y')
			return 0;

		//Get ROBLOSECURITY from Roblox Studio registry
		HKEY studio_key;
		if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Roblox\\RobloxStudioBrowser\\roblox.com"), 0, KEY_READ, &studio_key) == ERROR_SUCCESS)
		{
			//Read string from registry
			char value[1024];
			DWORD value_size = sizeof(value);

			LSTATUS result = RegQueryValueExA(studio_key, ".ROBLOSECURITY", 0, NULL, (LPBYTE)value, &value_size);
			RegCloseKey(studio_key);

			if (result != ERROR_SUCCESS)
			{
				std::cout << "Failed to get value of .ROBLOSECURITY" << std::endl;
				return 1;
			}

			//Get actual cookie via regex
			std::string roblosecurity_value = std::string(value);
			std::smatch roblosecurity_match;
			std::regex roblosecurity_regex("COOK::<([^>]*)>");

			if (std::regex_search(roblosecurity_value, roblosecurity_match, roblosecurity_regex) == false || roblosecurity_match.size() < 2)
			{
				std::cout << "Failed to regex vlaue of .ROBLOSECURITY" << std::endl;
				return 1;
			}

			roblosecurity = roblosecurity_match[1].str();
		}
		else
		{
			std::cout << "Failed to find Roblox Studio registry. Do you have Roblox Studio installed?" << std::endl;
			return 1;
		}
	}
	else
	{
		//Ensure content path has slash at the end
		if (path_content.back() != '/' && path_content.back() != '\\')
			path_content += "/";
	}
	CreateDirectoryA((path_content + "salvl").c_str(), NULL);

	//Get other arguments
	std::string path_rbxmx = path_content + "salvl/level.rbxmx";

	float scale = strtof(argv[2], nullptr);

	std::string path_lvl = argv[3];

	std::string path_texlist = argv[4];
	std::string path_texbase;

	auto path_base_cut = path_texlist.find_last_of("/\\");
	if (path_base_cut != std::string::npos)
		path_texbase = path_texlist.substr(0, path_base_cut + 1);

	//Read texlist
	std::cout << "Reading texlist..." << std::endl;

	std::ifstream stream_texlist(path_texlist);
	if (!stream_texlist.is_open())
	{
		std::cout << "Failed to open texlist index " << path_texlist << std::endl;
		return 1;
	}

	std::vector<SALVL_Texture> textures;

	while (!stream_texlist.eof())
	{
		//Read line
		std::string line;
		std::getline(stream_texlist, line);

		//Read texture name
		SALVL_Texture texture;
		
		auto delim_pathstart = line.find_first_of(",");
		auto delim_pathend = line.find_last_of(",");

		if (delim_pathstart != std::string::npos && delim_pathend != std::string::npos)
		{
			texture.name = line.substr(delim_pathstart + 1, (delim_pathend - delim_pathstart) - 1);
			std::cout << "  " << texture.name << std::endl;

			texture.name_fu = "fu_" + texture.name;
			texture.name_fv = "fv_" + texture.name;
			texture.name_fuv = "fuv_" + texture.name;
			
			//Read original image
			int tex_w, tex_h;
			unsigned char *tex_src = stbi_load((path_texbase + texture.name).c_str(), &tex_w, &tex_h, NULL, 4);
			if (tex_src == nullptr)
			{
				std::cout << "Failed to read texture " << (path_texbase + texture.name) << std::endl;
				return 1;
			}
			int tex_p = tex_w * 4;
			
			texture.xres = tex_w;
			texture.yres = tex_h;

			//Check if transparent
			for (int i = 0; i < tex_w * tex_h; i++)
				if (tex_src[i * 4 + 3] != 0xFF)
					texture.transparent = true;

			//Create flipped versions
			unsigned char *tex_fu = (unsigned char*)STBI_MALLOC(tex_p * 2 * tex_h);
			unsigned char *tex_fv = (unsigned char*)STBI_MALLOC(tex_p * tex_h * 2);
			unsigned char *tex_fuv = (unsigned char*)STBI_MALLOC(tex_p * 2 * tex_h * 2);
			if (tex_fv == nullptr || tex_fu == nullptr || tex_fuv == nullptr)
			{
				std::cout << "Failed to allocate texture flip buffers" << std::endl;
				return 1;
			}

			//Horizontal flip
			for (int x = 0; x < tex_w * 2; x++)
			{
				int src_x = x;
				if (src_x >= tex_w)
					src_x = tex_w * 2 - src_x - 1;
				for (int y = 0; y < tex_h; y++)
					memcpy(tex_fu + (y * tex_p * 2) + (x * 4), tex_src + (y * tex_p) + (src_x * 4), 4);
			}

			//Vertical flip
			memcpy(tex_fv, tex_src, tex_p * tex_h);
			for (int y = 0; y < tex_h; y++)
				memcpy(tex_fv + tex_p * (tex_h + y), tex_src + tex_p * (tex_h - y - 1), tex_p);

			//Vertical and horizontal flip
			memcpy(tex_fuv, tex_fu, tex_p * 2 * tex_h);
			for (int y = 0; y < tex_h; y++)
				memcpy(tex_fuv + tex_p * 2 * (tex_h + y), tex_fu + tex_p * 2 * (tex_h - y - 1), tex_p * 2);

			//Write textures
			texture.path = path_content + "salvl/" + texture.name;
			texture.path_fu = path_content + "salvl/" + texture.name_fu;
			texture.path_fv = path_content + "salvl/" + texture.name_fv;
			texture.path_fuv = path_content + "salvl/" + texture.name_fuv;

			if (stbi_write_png(texture.path.c_str(), tex_w, tex_h, 4, tex_src, tex_p) == 0 ||
				stbi_write_png(texture.path_fu.c_str(), tex_w * 2, tex_h, 4, tex_fu, tex_p * 2) == 0 ||
				stbi_write_png(texture.path_fv.c_str(), tex_w, tex_h * 2, 4, tex_fv, tex_p) == 0 ||
				stbi_write_png(texture.path_fuv.c_str(), tex_w * 2, tex_h * 2, 4, tex_fuv, tex_p * 2) == 0)
			{
				std::cout << "Failed to write textures" << std::endl;
				return 1;
			}

			stbi_image_free(tex_src);
			STBI_FREE(tex_fu);
			STBI_FREE(tex_fv);
			STBI_FREE(tex_fuv);

			//Push to texture list
			textures.push_back(texture);
		}
	}

	//Read landtable from LVL file
	std::cout << "Converting LVL " << path_lvl << " to landtable..." << std::endl;

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
	std::cout << "Reading COLs..." << std::endl;

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

							meshpart->texture = (nmaterial->attrflags & NJD_FLAG_USE_TEXTURE) ? &textures[nmaterial->attr_texId] : nullptr;
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
					meshes[model] = mesh;
					meshinstance.mesh = &meshes[model];
				}

				//Push mesh instance
				meshinstances.push_back(meshinstance);
			}
		}
	}
	
	//Write RBX meshes
	std::cout << "Writing RBX meshes..." << std::endl;
	unsigned int mesh_ind = 0;
	for (auto &i : meshes)
	{
		//Write mesh parts
		SALVL_Mesh *mesh = &i.second;

		for (auto &j : mesh->parts)
		{
			//Correct meshpart AABB
			SALVL_MeshPart *meshpart = &j.second;
			meshpart->AABBCorrect();

			//Open mesh file
			meshpart->name = std::to_string(mesh_ind) + ".mesh";
			std::cout << "  " << meshpart->name << std::endl;

			meshpart->ind = mesh_ind;

			std::string path_mesh = path_content + "salvl/" + meshpart->name;
			meshpart->path = path_mesh;

			std::ofstream stream_mesh(path_mesh, std::ios::binary);
			if (!stream_mesh.is_open())
			{
				std::cout << "Failed to open mesh " << path_mesh << std::endl;
				return 1;
			}
			stream_mesh.write("version 2.00\n", 13);

			//Write mesh header
			Write16(stream_mesh, 12); //sizeof_MeshHeader
			stream_mesh.put((char)0x28); //sizeof_MeshVertex
			stream_mesh.put((char)0x0C); //sizeof_MeshFace

			unsigned int num_verts = meshpart->vertex.size();
			Write32(stream_mesh, num_verts);
			unsigned int num_faces = meshpart->indices.size() / 3;
			Write32(stream_mesh, num_faces);

			//Write vertex data
			for (auto &k : meshpart->vertex)
			{
				WriteFloat(stream_mesh, k.pos.x); WriteFloat(stream_mesh, k.pos.y); WriteFloat(stream_mesh, k.pos.z); //Position
				WriteFloat(stream_mesh, k.nor.x); WriteFloat(stream_mesh, k.nor.y); WriteFloat(stream_mesh, k.nor.z); //Normal
				WriteFloat(stream_mesh, k.tex.x); WriteFloat(stream_mesh, k.tex.y); //Texture
				stream_mesh.put((char)0x00); stream_mesh.put((char)0x00); stream_mesh.put((char)0x00); stream_mesh.put((char)0x00); //Tangent
				stream_mesh.put((char)0xFF); stream_mesh.put((char)0xFF); stream_mesh.put((char)0xFF); stream_mesh.put((char)0xFF); //RGBA tint
			}

			//Write indices
			for (auto &k : meshpart->indices)
				Write32(stream_mesh, k);

			//Increment mesh index
			mesh_ind++;
		}
	}

	//Get URLs of assets
	if (upload)
	{
		//Upload content to Roblox
		std::cout << "Uploading content to Roblox..." << std::endl;

		//Upload meshes
		std::unordered_map<std::string, std::string> upload_texs;

		for (auto &i : meshes)
		{
			for (auto &j : i.second.parts)
			{
				//Upload mesh

				//Set texture to be loaded
				if ((j.second.matflags & NJD_FLAG_USE_TEXTURE) && j.second.texture != nullptr)
				{
					if (j.second.matflags & NJD_FLAG_FLIP_U)
					{
						j.second.name_texture = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->name_fuv : j.second.texture->name_fu);
						upload_texs[j.second.name_texture] = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->path_fuv : j.second.texture->path_fu);
					}
					else
					{
						j.second.name_texture = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->name_fv : j.second.texture->name);
						upload_texs[j.second.name_texture] = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->path_fv : j.second.texture->path);
					}
				}
			}
		}

		//Upload textures
		for (auto &i : upload_texs)
		{

		}

		//Assign uploaded textures to meshes
		for (auto &i : meshes)
			for (auto &j : i.second.parts)
				j.second.url_texture = upload_texs[j.second.name_texture];
	}
	else
	{
		//Get rbxasset URLs
		std::cout << "Getting rbxasset:// URLs..." << std::endl;

		for (auto &i : textures)
		{
			i.url = "rbxasset://salvl/" + i.name;
			i.url_fu = "rbxasset://salvl/" + i.name_fu;
			i.url_fv = "rbxasset://salvl/" + i.name_fv;
			i.url_fuv = "rbxasset://salvl/" + i.name_fuv;
		}
		for (auto &i : meshes)
		{
			for (auto &j : i.second.parts)
			{
				j.second.url = "rbxasset://salvl/" + j.second.name;
				if ((j.second.matflags & NJD_FLAG_USE_TEXTURE) && j.second.texture != nullptr)
				{
					if (j.second.matflags & NJD_FLAG_FLIP_U)
					{
						j.second.name_texture = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->name_fuv : j.second.texture->name_fu);
						j.second.url_texture = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->url_fuv : j.second.texture->url_fu);
					}
					else
					{
						j.second.name_texture = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->name_fv : j.second.texture->name);
						j.second.url_texture = ((j.second.matflags & NJD_FLAG_FLIP_V) ? j.second.texture->url_fv : j.second.texture->url);
					}
				}
			}
		}
	}

	//Get where meshes should be placed
	std::cout << "Placing MeshPart instances..." << std::endl;

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
			meshpart_instance.matrix[M00] = i.matrix[M00]; //NOTE: Flip row and columns
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
			meshpart_instance.surf_flag = i.surf_flag;

			//Add to appropriate list
			if (i.surf_flag & SA1LVL_SURFFLAG_SOLID)
				mesh_collision.push_back(meshpart_instance);
			else if (i.surf_flag & SA1LVL_SURFFLAG_VISIBLE)
				mesh_visual.push_back(meshpart_instance);
		}
	}

	//Write RBXMX
	std::cout << "Writing RBXMX " << path_rbxmx << "..." << std::endl;

	std::unordered_map<SALVL_MeshPart*, SALVL_CSGMesh> meshpart_csgmesh;

	std::ofstream stream_rbxmx(path_rbxmx);
	if (!stream_rbxmx.is_open())
	{
		std::cout << "Failed to open RBXMX " << path_rbxmx << std::endl;
		return 1;
	}

	//ROBLOX model tree
	stream_rbxmx << "<roblox xmlns:xmime=\"http://www.w3.org/2005/05/xmlmime\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"http://www.roblox.com/roblox.xsd\" version=\"4\">" << std::endl;
		//Level folder
		stream_rbxmx << "<Item class = \"Folder\">" << std::endl;
			stream_rbxmx << "<Properties>" << std::endl;
				stream_rbxmx << "<string name=\"Name\">Level</string>" << std::endl;
			stream_rbxmx << "</Properties>" << std::endl;
			//Map folder
			stream_rbxmx << "<Item class = \"Folder\">" << std::endl;
				stream_rbxmx << "<Properties>" << std::endl;
					stream_rbxmx << "<string name=\"Name\">Map</string>" << std::endl;
				stream_rbxmx << "</Properties>" << std::endl;
				//Collision Folder
				stream_rbxmx << "<Item class = \"Folder\">" << std::endl;
					stream_rbxmx << "<Properties>" << std::endl;
						stream_rbxmx << "<string name=\"Name\">Collision</string>" << std::endl;
					stream_rbxmx << "</Properties>" << std::endl;
					for (auto &i : mesh_collision)
					{
						//Calculate CSG mesh
						auto csgmeshind = meshpart_csgmesh.find(i.meshpart);

						SALVL_CSGMesh *csgmesh;
						if (csgmeshind == meshpart_csgmesh.end())
						{
							//Generate CSG mesh data
							std::vector<Uint8> csgmesh_data;
							csgmesh_data.push_back('C'); csgmesh_data.push_back('S'); csgmesh_data.push_back('G'); csgmesh_data.push_back('P'); csgmesh_data.push_back('H'); csgmesh_data.push_back('S');
							Push32(csgmesh_data, 3);

							//Write sub-meshes
							for (size_t j = 0; j < i.meshpart->indices.size(); j += 3)
							{
								//Get vertices
								SALVL_Vertex *v0 = &i.meshpart->vertex[i.meshpart->indices[j + 0]];
								SALVL_Vertex *v1 = &i.meshpart->vertex[i.meshpart->indices[j + 1]];
								SALVL_Vertex *v2 = &i.meshpart->vertex[i.meshpart->indices[j + 2]];

								//Write dummied out header
								Push32(csgmesh_data, 16); //sizeof_TriIndices
								csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); //TriIndices[16]
								csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00);
								csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00);
								csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00);

								Push32(csgmesh_data, 16); //sizeof_TransformOffsets
								csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); //TransformOffsets[16]
								csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00);
								csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x00); csgmesh_data.push_back(0x80); csgmesh_data.push_back(0x3F);

								//Write vertices
								Push32(csgmesh_data, 18); //numCoords
								Push32(csgmesh_data, 4); //sizeof_float

								PushFloat(csgmesh_data, v0->pos.x); PushFloat(csgmesh_data, v0->pos.y); PushFloat(csgmesh_data, v0->pos.z); //Vertex 0
								PushFloat(csgmesh_data, v1->pos.x); PushFloat(csgmesh_data, v1->pos.y); PushFloat(csgmesh_data, v1->pos.z); //Vertex 1
								PushFloat(csgmesh_data, v2->pos.x); PushFloat(csgmesh_data, v2->pos.y); PushFloat(csgmesh_data, v2->pos.z); //Vertex 2

								PushFloat(csgmesh_data, v0->pos.x - v0->nor.x * 0.125f); PushFloat(csgmesh_data, v0->pos.y - v0->nor.y * 0.125f); PushFloat(csgmesh_data, v0->pos.z - v0->nor.z * 0.125f); //Vertex 3
								PushFloat(csgmesh_data, v1->pos.x - v1->nor.x * 0.125f); PushFloat(csgmesh_data, v1->pos.y - v1->nor.y * 0.125f); PushFloat(csgmesh_data, v1->pos.z - v1->nor.z * 0.125f); //Vertex 4
								PushFloat(csgmesh_data, v2->pos.x - v2->nor.x * 0.125f); PushFloat(csgmesh_data, v2->pos.y - v2->nor.y * 0.125f); PushFloat(csgmesh_data, v2->pos.z - v2->nor.z * 0.125f); //Vertex 5

								//Write indices
								Push32(csgmesh_data, 6); //numIndices
								Push32(csgmesh_data, 0); Push32(csgmesh_data, 1); Push32(csgmesh_data, 2); //Triangle 0 (front)
								Push32(csgmesh_data, 5); Push32(csgmesh_data, 4); Push32(csgmesh_data, 3); //Triangle 1 (back)
							}

							//Create new mesh, encode, and push to map
							SALVL_CSGMesh csgmeshe;
							csgmeshe.Encode(csgmesh_data);
							meshpart_csgmesh[i.meshpart] = csgmeshe;
							csgmesh = &meshpart_csgmesh[i.meshpart];
						}
						else
						{
							//Use already existing mesh
							csgmesh = &csgmeshind->second;
						}

						//MeshPart
						stream_rbxmx << "<Item class = \"MeshPart\">" << std::endl;
							stream_rbxmx << "<Properties>" << std::endl;
								stream_rbxmx << "<string name=\"Name\">MeshPart</string>" << std::endl;
								stream_rbxmx << "<bool name=\"Anchored\">true</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"CanCollide\">true</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"CanTouch\">false</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"DoubleSided\">true</bool>" << std::endl;
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
								stream_rbxmx << "<Content name=\"MeshID\"><url>" << i.meshpart->url << "</url></Content>" << std::endl;
								if ((i.meshpart->matflags & NJD_FLAG_USE_TEXTURE) && i.meshpart->texture != nullptr && !i.meshpart->texture->transparent)
								{
									stream_rbxmx << "<Content name=\"TextureID\"><url>";
									if (i.meshpart->matflags & NJD_FLAG_FLIP_U)
										stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fuv : i.meshpart->texture->url_fu);
									else
										stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fv : i.meshpart->texture->url);
									stream_rbxmx << "</url></Content>" << std::endl;
								}
								stream_rbxmx << "<SharedString name=\"PhysicalConfigData\">" << csgmesh->enc_hash << "</SharedString>" << std::endl;
								stream_rbxmx << "<float name=\"Transparency\">" << ((i.surf_flag & SA1LVL_SURFFLAG_VISIBLE) ? 0.0f : 1.0f) << "</float>" << std::endl;
								stream_rbxmx << "<Color3uint8 name = \"Color3uint8\">" << i.meshpart->diffuse << "</Color3uint8>" << std::endl;
							stream_rbxmx << "</Properties>" << std::endl;
							if ((i.meshpart->matflags & NJD_FLAG_USE_TEXTURE) && i.meshpart->texture != nullptr && i.meshpart->texture->transparent)
							{
								//SurfaceAppearance
								stream_rbxmx << "<Item class = \"SurfaceAppearance\">" << std::endl;
									stream_rbxmx << "<Properties>" << std::endl;
										stream_rbxmx << "<token name=\"AlphaMode\">1</token>" << std::endl;
										stream_rbxmx << "<Content name=\"ColorMap\"><url>";
										if (i.meshpart->matflags & NJD_FLAG_FLIP_U)
											stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fuv : i.meshpart->texture->url_fu);
										else
											stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fv : i.meshpart->texture->url);
										stream_rbxmx << "</url></Content>" << std::endl;
									stream_rbxmx << "</Properties>" << std::endl;
								stream_rbxmx << "</Item>" << std::endl;
							}
						stream_rbxmx << "</Item>" << std::endl;
					}
				stream_rbxmx << "</Item>" << std::endl;
				//Visual Folder
				stream_rbxmx << "<Item class = \"Folder\">" << std::endl;
					stream_rbxmx << "<Properties>" << std::endl;
						stream_rbxmx << "<string name=\"Name\">Visual</string>" << std::endl;
					stream_rbxmx << "</Properties>" << std::endl;
					for (auto &i : mesh_visual)
					{
						//MeshPart
						stream_rbxmx << "<Item class = \"MeshPart\">" << std::endl;
							stream_rbxmx << "<Properties>" << std::endl;
								stream_rbxmx << "<string name=\"Name\">MeshPart</string>" << std::endl;
								stream_rbxmx << "<bool name=\"Anchored\">true</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"CanCollide\">false</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"CanTouch\">false</bool>" << std::endl;
								stream_rbxmx << "<bool name=\"DoubleSided\">true</bool>" << std::endl;
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
								stream_rbxmx << "<Content name=\"MeshID\"><url>" << i.meshpart->url << "</url></Content>" << std::endl;
								if ((i.meshpart->matflags & NJD_FLAG_USE_TEXTURE) && i.meshpart->texture != nullptr && !i.meshpart->texture->transparent)
								{
									stream_rbxmx << "<Content name=\"TextureID\"><url>";
									if (i.meshpart->matflags & NJD_FLAG_FLIP_U)
										stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fuv : i.meshpart->texture->url_fu);
									else
										stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fv : i.meshpart->texture->url);
									stream_rbxmx << "</url></Content>" << std::endl;
								}
								stream_rbxmx << "<float name=\"Transparency\">" << ((i.surf_flag & SA1LVL_SURFFLAG_VISIBLE) ? 0.0f : 1.0f) << "</float>" << std::endl;
								stream_rbxmx << "<Color3uint8 name = \"Color3uint8\">" << i.meshpart->diffuse << "</Color3uint8>" << std::endl;
							stream_rbxmx << "</Properties>" << std::endl;
							if ((i.meshpart->matflags & NJD_FLAG_USE_TEXTURE) && i.meshpart->texture != nullptr && i.meshpart->texture->transparent)
							{
								//SurfaceAppearance
								stream_rbxmx << "<Item class = \"SurfaceAppearance\">" << std::endl;
									stream_rbxmx << "<Properties>" << std::endl;
										stream_rbxmx << "<token name=\"AlphaMode\">1</token>" << std::endl;
										stream_rbxmx << "<Content name=\"ColorMap\"><url>";
										if (i.meshpart->matflags & NJD_FLAG_FLIP_U)
											stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fuv : i.meshpart->texture->url_fu);
										else
											stream_rbxmx << ((i.meshpart->matflags & NJD_FLAG_FLIP_V) ? i.meshpart->texture->url_fv : i.meshpart->texture->url);
										stream_rbxmx << "</url></Content>" << std::endl;
									stream_rbxmx << "</Properties>" << std::endl;
								stream_rbxmx << "</Item>" << std::endl;
							}
						stream_rbxmx << "</Item>" << std::endl;
					}
				stream_rbxmx << "</Item>" << std::endl;
			stream_rbxmx << "</Item>" << std::endl;
		stream_rbxmx << "</Item>" << std::endl;
		//Shared Strings (CSGMesh hashes)
		stream_rbxmx << "<SharedStrings>" << std::endl;
		std::set<std::string> csgmesh_key;
		for (auto &i : meshpart_csgmesh)
		{
			if (csgmesh_key.find(i.second.enc_hash) == csgmesh_key.end())
			{
				stream_rbxmx << "<SharedString md5=\"" << i.second.enc_hash << "\">" << i.second.enc_base64 << "</SharedString>" << std::endl;
				csgmesh_key.insert(i.second.enc_hash);
			}
		}
		stream_rbxmx << "</SharedStrings>" << std::endl;
	stream_rbxmx << "</roblox>" << std::endl;

	std::cout << "Complete!" << std::endl;
	return 0;
}
