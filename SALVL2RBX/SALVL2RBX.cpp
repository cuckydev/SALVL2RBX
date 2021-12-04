#include "SALVL2RBX.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <limits>
#include <set>
#include <algorithm>
#include <regex>
#include <iomanip>

#include <Winsock2.h>
#include <wininet.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//CSG mesh
#include "md5.h"

struct SALVL_CSGMesh
{
	//Encoded data
	std::string enc_base64; //Base64 encoded
	std::string enc_hash; //MD5 hash

	std::string Base64(const Uint8 *data, size_t size)
	{
		//Get basic output
		static const char *lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::string out;
		out.reserve(size);

		int val = 0;
		int valb = -6;

		for (size_t i = 0; i < size; i++)
		{
			val = (val << 8) + data[i];
			valb += 8;
			while (valb >= 0)
			{
				out.push_back(lookup[(val >> valb) & 0x3F]);
				valb -= 6;
			}
		}

		if (valb > -6)
			out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);

		while (out.size() % 4)
			out.push_back('=');

		//Line break
		std::string result;
		for (size_t i = 0; i < out.size(); i += 72)
		{
			if (i != 0)
				result.push_back('\n');
			result += out.substr(i, 72);
		}

		return result;
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
	cframe[M10] = sin * cframe[M20] + cframe[M10] * cos;
	cframe[M11] = sin * cframe[M21] + cframe[M11] * cos;
	cframe[M12] = sin * cframe[M22] + cframe[M12] * cos;
	cframe[M13] = sin * cframe[M23] + cframe[M13] * cos;
	cframe[M20] = cos * cframe[M20] - cframe[M10] * sin;
	cframe[M21] = cos * cframe[M21] - cframe[M11] * sin;
	cframe[M22] = cos * cframe[M22] - cframe[M12] * sin;
	cframe[M23] = cos * cframe[M23] - cframe[M13] * sin;
}

void Reimp_njRotateY(Float *cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	Float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	Float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	cframe[M00] = cframe[M00] * cos - sin * cframe[M20];
	cframe[M01] = cframe[M01] * cos - sin * cframe[M21];
	cframe[M02] = cframe[M02] * cos - sin * cframe[M22];
	cframe[M03] = cframe[M03] * cos - sin * cframe[M23];
	cframe[M20] = cos * cframe[M20] + cframe[M00] * sin;
	cframe[M21] = cos * cframe[M21] + cframe[M01] * sin;
	cframe[M22] = cos * cframe[M22] + cframe[M02] * sin;
	cframe[M23] = cos * cframe[M23] + cframe[M03] * sin;
}

void Reimp_njRotateZ(Float *cframe, Angle x)
{
	//Calculate the sine and cosine of our angle
	Float sin = sinf((float)x * 3.14159265358979323846f / 0x8000);
	Float cos = cosf((float)x * 3.14159265358979323846f / 0x8000);

	//Apply rotation onto matrix
	cframe[M00] = cframe[M00] * cos + sin * cframe[M10];
	cframe[M01] = cframe[M01] * cos + sin * cframe[M11];
	cframe[M02] = cframe[M02] * cos + sin * cframe[M12];
	cframe[M03] = cframe[M03] * cos + sin * cframe[M13];
	cframe[M10] = cos * cframe[M10] - cframe[M00] * sin;
	cframe[M11] = cos * cframe[M11] - cframe[M01] * sin;
	cframe[M12] = cos * cframe[M12] - cframe[M02] * sin;
	cframe[M13] = cos * cframe[M13] - cframe[M03] * sin;
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

//Asset upload
std::string URLEncode(const std::string &value)
{
	std::ostringstream escaped;
	escaped.fill('0');
	escaped << std::hex;

	for (auto &i : value)
	{
		//Keep alphanumeric and other accepted characters intact
		if (isalnum(i) || i == '-' || i == '_' || i == '.' || i == '~')
		{
			escaped << i;
			continue;
		}

		//Any other characters are percent-encoded
		escaped << std::uppercase;
		escaped << '%' << std::setw(2) << int((unsigned char)i);
		escaped << std::nouppercase;
	}

	return escaped.str();
}

char *GetHTTPData(HINTERNET request, DWORD query, DWORD *size)
{
	//Request response
	DWORD response_size = 0;
	if (HttpQueryInfoA(request, query, nullptr, &response_size, nullptr) == FALSE && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		std::cout << "Failed to query response " << GetLastError() << std::endl;
		return nullptr;
	}

	char *responseb = new char[response_size] {};
	if (HttpQueryInfoA(request, query, responseb, &response_size, nullptr) == FALSE)
	{
		std::cout << "Failed to query response " << GetLastError() << std::endl;
		return nullptr;
	}

	if (size != nullptr)
		*size = response_size;
	return responseb;
}

std::string GetHTTPString(HINTERNET request, DWORD query)
{
	//Request response
	DWORD size;
	char *responseb = GetHTTPData(request, query, &size);

	std::string response(responseb, size);
	delete[] responseb;
	return response;
}

class AssetManager
{
private:
	//Internal state
	std::string xsrf_token = "FETCH";

	//Internet instances
	HINTERNET internet = nullptr, roblox = nullptr;

public:
	bool Start()
	{
		//Setup WinInet
		if ((internet = InternetOpenA("RobloxStudio/WinInet", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0)) == nullptr)
		{
			std::cout << "Failed to create Internet " << GetLastError() << std::endl;
			return true;
		}
		if ((roblox = InternetConnectA(internet, "data.roblox.com", INTERNET_DEFAULT_HTTP_PORT, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0)) == nullptr)
		{
			std::cout << "Failed to connect to data.roblox.com " << GetLastError() << std::endl;
			return true;
		}

		return false;
	}

	std::string UploadAsset(std::string object, std::vector<char> data)
	{
		//Open request to upload service
		static PCSTR accept_types[] = { "*/*", nullptr };

		HINTERNET mesh_request = HttpOpenRequestA(roblox, "POST", object.c_str(), nullptr, nullptr, accept_types, 0, 0);
		if (mesh_request == nullptr)
		{
			std::cout << "Failed to create HTTP request to mesh service " << GetLastError() << std::endl;
			return "";
		}

		//Try to send request 10 times
		for (int i = 0; i < 10; i++)
		{
			//Setup headers
			HttpAddRequestHeadersA(mesh_request, "Content-Type: */*\n", -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
			HttpAddRequestHeadersA(mesh_request, "User-Agent: RobloxStudio/WinInet\n", -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
			HttpAddRequestHeadersA(mesh_request, ("X-CSRF-TOKEN: " + xsrf_token + "\n").c_str(), -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

			//Send request
			if (HttpSendRequestA(mesh_request, nullptr, -1, (void *)data.data(), data.size()) == FALSE)
				break;

			//Handle XSRF property
			std::string response = GetHTTPString(mesh_request, HTTP_QUERY_STATUS_TEXT);
			std::cout << response << std::endl;

			if (response.find("XSRF") != std::string::npos)
			{
				//Get headers
				char *headers = GetHTTPData(mesh_request, HTTP_QUERY_RAW_HEADERS, nullptr);
				if (headers == nullptr)
					break;

				for (char *headerp = headers; *headerp != '\0'; headerp += strlen(headerp) + 1)
				{
					//Split header
					std::string header(headerp);

					auto split = header.find_first_of(": ");
					if (split != std::string::npos)
					{
						//Get key and value
						std::string key = header.substr(0, split);
						std::string value = header.substr(split + 2);

						//Check if XSRF
						if (key == "x-csrf-token")
							xsrf_token = value;
					}
				}
			}
			else if (response == "OK")
			{
				//Read response
				DWORD blocksize = 4096;
				DWORD received = 0;
				std::string block(blocksize, 0);
				std::string result;
				while (InternetReadFile(mesh_request, &block[0], blocksize, &received) && received)
				{
					block.resize(received);
					result += block;
				}

				//Return response
				InternetCloseHandle(mesh_request);
				return result;
			}
			else
			{
				Sleep(3000);
			}
		}

		//Close request
		InternetCloseHandle(mesh_request);
		return "";
	}

	~AssetManager()
	{
		if (roblox != nullptr)
			InternetCloseHandle(roblox);
		if (internet != nullptr)
			InternetCloseHandle(internet);
	}
};

//Console static initialization
class ConsoleInit
{
	public:
		ConsoleInit() { AttachConsole(GetCurrentProcessId()); }
		~ConsoleInit() { FreeConsole(); }
};
static ConsoleInit consoleinit_;

//WSA static initialization
class WSInit
{
	public:
		WSInit()
		{
			WSADATA wsaData;
			if (WSAStartup(MAKEWORD(2, 2), &wsaData))
				std::cout << "Failed to initialize WSA" << std::endl;
		}
		~WSInit() { WSACleanup(); }
};
static WSInit wsinit_;

//Entry point
int SALVL2RBX(int argc, char *argv[], int (loader)(SALVL&, std::string))
{
	SALVL lvl;

	//Randomize mutation
	srand((int)time(nullptr));

	//Check arguments
	std::string targv[5];
	if (argc < 5)
	{
		std::cout << "Please input: upload/content_directory scale sa1lvl texlist_index_txt" << std::endl;
		std::cin >> std::quoted(targv[1]);
		std::cin >> std::quoted(targv[2]);
		std::cin >> std::quoted(targv[3]);
		std::cin >> std::quoted(targv[4]);
	}
	else
	{
		targv[1] = std::string(argv[1]);
		targv[2] = std::string(argv[2]);
		targv[3] = std::string(argv[3]);
		targv[4] = std::string(argv[4]);
	}

	//Get content folder
	std::string path_content = targv[1];

	AssetManager asset_manager;
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

		//Start asset manager
		if (asset_manager.Start())
			return 1;

		//Get ROBLOSECURITY from Roblox Studio registry
		HKEY studio_key;
		if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Roblox\\RobloxStudioBrowser\\roblox.com"), 0, KEY_READ, &studio_key) == ERROR_SUCCESS)
		{
			//Read cookies from registry
			std::string cookie;

			static std::string values[] = {
				".RBXID",
				".ROBLOSECURITY",
			};

			for (auto &i : values)
			{
				char value[1024] = {};
				DWORD value_size = sizeof(value);

				LSTATUS result = RegQueryValueExA(studio_key, i.c_str(), 0, NULL, (LPBYTE)value, &value_size);
				if (result == ERROR_SUCCESS)
				{
					std::string values = std::string(value);
					auto start = values.find("COOK::");
					if (start != std::string::npos)
						InternetSetCookieA("https://data.roblox.com", i.c_str(), values.substr(start + 7, values.size() - (start + 8)).c_str());
				}
			}

			RegCloseKey(studio_key);
		}
		else
		{
			std::cout << "Failed to find Roblox Studio registry. Do you have Roblox Studio installed?" << std::endl;
			system("pause");
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

	float scale;
	try
	{ scale = std::stof(targv[2]); }
	catch (...)
	{ std::cout << "Invalid scale parameter" << std::endl; return 1; }

	std::string path_lvl = targv[3];

	std::string path_texlist = targv[4];
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
		system("pause");
		return 1;
	}

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
				system("pause");
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
			unsigned char *tex_fu = (unsigned char *)STBI_MALLOC(tex_p * 2 * tex_h);
			unsigned char *tex_fv = (unsigned char *)STBI_MALLOC(tex_p * tex_h * 2);
			unsigned char *tex_fuv = (unsigned char *)STBI_MALLOC(tex_p * 2 * tex_h * 2);
			if (tex_fv == nullptr || tex_fu == nullptr || tex_fuv == nullptr)
			{
				std::cout << "Failed to allocate texture flip buffers" << std::endl;
				system("pause");
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

			//Mutate textures
			unsigned char *charp[4];
			charp[0] = tex_src + (tex_p * (rand() % tex_h)) + ((rand() % tex_w) * 4) + (rand() % 3);
			charp[1] = tex_fu + ((tex_p * 2) * (rand() % tex_h)) + ((rand() % (tex_w * 2)) * 4) + (rand() % 3);
			charp[2] = tex_fv + (tex_p * (rand() % (tex_h * 2))) + ((rand() % tex_w) * 4) + (rand() % 3);
			charp[3] = tex_fuv + ((tex_p * 2) * (rand() % (tex_h * 2))) + ((rand() % (tex_w * 2)) * 4) + (rand() % 3);

			for (int i = 0; i < 4; i++)
			{
				if (rand() & 1)
					*charp[i] = (*charp[i] != 0) ? (*charp[i] - 1) : 0;
				else
					*charp[i] = (*charp[i] != 0xFF) ? (*charp[i] + 1) : 0xFF;
			}

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
				system("pause");
				return 1;
			}

			stbi_image_free(tex_src);
			STBI_FREE(tex_fu);
			STBI_FREE(tex_fv);
			STBI_FREE(tex_fuv);

			//Push to texture list
			lvl.textures.push_back(texture);
		}
	}

	//Read landtable from LVL file
	std::cout << "Converting LVL " << path_lvl << " to landtable..." << std::endl;

	if (loader(lvl, path_lvl))
		return 1;

	//Write RBX meshes
	std::cout << "Writing RBX meshes..." << std::endl;
	unsigned int mesh_ind = 0;
	for (auto &i : lvl.meshes)
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
				system("pause");
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

		int num_meshes = 0;
		for (auto &i : lvl.meshes)
			for (auto &j : i.second.parts)
				num_meshes++;

		std::cout << "  Uploading " << num_meshes << " meshes..." << std::endl;
		for (auto &i : lvl.meshes)
		{
			for (auto &j : i.second.parts)
			{
				//Upload mesh
				std::ifstream meshbuf_stream(j.second.path, std::ios::binary);
				std::vector<char> meshbuf((std::istreambuf_iterator<char>(meshbuf_stream)), std::istreambuf_iterator<char>());
				std::string object = "/ide/publish/UploadNewMesh?name=" + URLEncode(j.second.name) + "&description=" + URLEncode("Generated by SALVL2RBX");
				if ((j.second.url = "rbxassetid://" + asset_manager.UploadAsset(object, meshbuf)).empty())
				{
					std::cout << "Failed to upload mesh" << std::endl;
					system("pause");
					return 1;
				}
				std::cout << "  Uploaded mesh " << j.second.name << " to " << j.second.url << std::endl;

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
		std::unordered_map<std::string, std::string> uploaded_texs;

		std::cout << "  Uploading " << upload_texs.size() << " textures..." << std::endl;
		for (auto &i : upload_texs)
		{
			std::ifstream texbuf_stream(i.second, std::ios::binary);
			std::vector<char> texbuf((std::istreambuf_iterator<char>(texbuf_stream)), std::istreambuf_iterator<char>());
			std::string object = "/data/upload/json?assetTypeId=13&name=" + URLEncode(i.first) + "&description=" + URLEncode("Generated by SALVL2RBX");
			std::string result = asset_manager.UploadAsset(object, texbuf);
			std::cout << result << std::endl;
			if (result.empty())
			{
				std::wcout << "Failed to upload decal" << std::endl;
				system("pause");
				return 1;
			}

			auto ids = result.find("\"BackingAssetId\":");
			if (ids == std::string::npos)
			{
				std::cout << "Didn't get BackingAssetId from decal upload" << std::endl;
				system("pause");
				return 1;
			}
			result = result.substr(ids + 17);

			auto enda = result.find(",");
			auto endb = result.find("}");
			if (enda != std::string::npos)
				result = result.substr(0, enda);
			else if (endb != std::string::npos)
				result = result.substr(0, endb);
			else
			{
				std::cout << "Decal upload returned non terminated json" << std::endl;
				system("pause");
				return 1;
			}

			uploaded_texs[i.first] = "rbxassetid://" + result;
			std::cout << "  Uploaded texture " << i.first << " to " << uploaded_texs[i.first] << std::endl;
		}

		//Assign uploaded textures to meshes
		for (auto &i : lvl.meshes)
			for (auto &j : i.second.parts)
				j.second.url_texture = uploaded_texs[j.second.name_texture];
	}
	else
	{
		//Get rbxasset URLs
		std::cout << "Getting rbxasset:// URLs..." << std::endl;

		for (auto &i : lvl.textures)
		{
			i.url = "rbxasset://salvl/" + i.name;
			i.url_fu = "rbxasset://salvl/" + i.name_fu;
			i.url_fv = "rbxasset://salvl/" + i.name_fv;
			i.url_fuv = "rbxasset://salvl/" + i.name_fuv;
		}
		for (auto &i : lvl.meshes)
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

	for (auto &i : lvl.meshinstances)
	{
		if (i.mesh == nullptr)
			continue;
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
			if (i.surf_flag & SALVL_SURFFLAG_SOLID)
				mesh_collision.push_back(meshpart_instance);
			else if (i.surf_flag & SALVL_SURFFLAG_VISIBLE)
				mesh_visual.push_back(meshpart_instance);
		}
	}

	//Write RBXMX
	std::cout << "Writing RBXMX " << path_rbxmx << "..." << std::endl;

	std::unordered_map<SALVL_MeshPart *, SALVL_CSGMesh> meshpart_csgmesh;

	std::ofstream stream_rbxmx(path_rbxmx);
	if (!stream_rbxmx.is_open())
	{
		std::cout << "Failed to open RBXMX " << path_rbxmx << std::endl;
		system("pause");
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
			stream_rbxmx << i.meshpart->url_texture;
			stream_rbxmx << "</url></Content>" << std::endl;
		}
		stream_rbxmx << "<SharedString name=\"PhysicalConfigData\">" << csgmesh->enc_hash << "</SharedString>" << std::endl;
		stream_rbxmx << "<float name=\"Transparency\">" << ((i.surf_flag & SALVL_SURFFLAG_VISIBLE) ? 0.0f : 1.0f) << "</float>" << std::endl;
		stream_rbxmx << "<Color3uint8 name = \"Color3uint8\">" << i.meshpart->diffuse << "</Color3uint8>" << std::endl;
		stream_rbxmx << "</Properties>" << std::endl;
		if ((i.meshpart->matflags & NJD_FLAG_USE_TEXTURE) && i.meshpart->texture != nullptr && i.meshpart->texture->transparent)
		{
			//SurfaceAppearance
			stream_rbxmx << "<Item class = \"SurfaceAppearance\">" << std::endl;
			stream_rbxmx << "<Properties>" << std::endl;
			stream_rbxmx << "<token name=\"AlphaMode\">1</token>" << std::endl;
			stream_rbxmx << "<Content name=\"ColorMap\"><url>";
			stream_rbxmx << i.meshpart->url_texture;
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
			stream_rbxmx << i.meshpart->url_texture;
			stream_rbxmx << "</url></Content>" << std::endl;
		}
		stream_rbxmx << "<float name=\"Transparency\">" << ((i.surf_flag & SALVL_SURFFLAG_VISIBLE) ? 0.0f : 1.0f) << "</float>" << std::endl;
		stream_rbxmx << "<Color3uint8 name = \"Color3uint8\">" << i.meshpart->diffuse << "</Color3uint8>" << std::endl;
		stream_rbxmx << "</Properties>" << std::endl;
		if ((i.meshpart->matflags & NJD_FLAG_USE_TEXTURE) && i.meshpart->texture != nullptr && i.meshpart->texture->transparent)
		{
			//SurfaceAppearance
			stream_rbxmx << "<Item class = \"SurfaceAppearance\">" << std::endl;
			stream_rbxmx << "<Properties>" << std::endl;
			stream_rbxmx << "<token name=\"AlphaMode\">1</token>" << std::endl;
			stream_rbxmx << "<Content name=\"ColorMap\"><url>";
			stream_rbxmx << i.meshpart->url_texture;
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

	//Cleanup WSA
	if (upload)
		WSACleanup();

	std::cout << "Complete!" << std::endl;
	system("pause");
	return 0;
}
