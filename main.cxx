#include <iostream>
#include <unordered_map>
#include <Windows.h>
#include <filesystem>

//========================================================================
// 3rdParty

#include "3rdParty/tinyxml2.h"

//========================================================================
// SDK

#define SDK_NO_RVA_PREPROCESSOR
#include <SDK/Preprocessors.hxx>
#include <SDK/Math/.Math.hxx>
#include <SDK/Optional/PermFile/.Includes.hpp>
#include <SDK/Optional/StringHash.hpp>

//========================================================================

void Unpack(std::filesystem::path p_FilePath)
{
	SDK::PermFile_t _PermFile;
	if (!_PermFile.LoadFile(p_FilePath.string().c_str())) 
	{
		printf("[ Error ] Couldn't load file...\n");
		return;
	}

	if (_PermFile.m_Resources.empty())
	{
		printf("[ Error ] No resource found in file...\n");
		return;
	}

	std::string _UnpackPath = (p_FilePath.remove_filename().string() + "ShadersUnpack");
	std::filesystem::create_directory(_UnpackPath);

	tinyxml2::XMLDocument _XMLDoc;
	tinyxml2::XMLElement* _XMLShaders = _XMLDoc.NewElement("Shaders");
	_XMLDoc.InsertEndChild(_XMLShaders);

	for (UFG::ResourceEntry_t* _ResourceEntry : _PermFile.m_Resources)
	{
		if (_ResourceEntry->m_TypeUID != 0x985BE50C) {
			continue;
		}

		Illusion::ShaderBinary_t* _ShaderBinary = reinterpret_cast<Illusion::ShaderBinary_t*>(_ResourceEntry->GetData());

		FILE* _ShaderFile = nullptr;
		fopen_s(&_ShaderFile, (_UnpackPath + "\\" + _ShaderBinary->m_ShaderName).c_str(), "wb");
		if (!_ShaderFile) {
			continue;
		}

		tinyxml2::XMLElement* _XMLShader = _XMLShaders->InsertNewChildElement("Shader");
		{
			_XMLShader->SetAttribute("StageType", static_cast<int>(_ShaderBinary->m_ShaderStageType));
			_XMLShader->SetText(_ShaderBinary->m_ShaderName);
		}

		printf("[ ~ ] %s [Size: 0x%X]\n", _ShaderBinary->m_ShaderName, _ShaderBinary->m_DataByteSize);

		if (_ShaderBinary->m_DataByteSize && _ShaderBinary->m_DataOffset) {
			fwrite(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&_ShaderBinary->m_DataOffset) + _ShaderBinary->m_DataOffset), 1, static_cast<size_t>(_ShaderBinary->m_DataByteSize), _ShaderFile);
		}

		fclose(_ShaderFile);
	}


	_XMLDoc.SaveFile((_UnpackPath + "\\config.xml").c_str());
}

void Pack(std::filesystem::path p_ConfigPath)
{
	tinyxml2::XMLDocument _XMLDoc;
	if (_XMLDoc.LoadFile(p_ConfigPath.string().c_str()) != tinyxml2::XML_SUCCESS)
	{
		printf("[ Error ] Couldn't load file...\n");
		return;
	}

	tinyxml2::XMLElement* _XMLShaders = _XMLDoc.FirstChildElement("Shaders");
	if (!_XMLShaders)
	{
		printf("[ Error ] 'Shaders' node missing..\n");
		return;
	}

	std::string _PackPath = p_ConfigPath.remove_filename().string();

	FILE* _ShadersFile = nullptr;
	fopen_s(&_ShadersFile, (_PackPath + "shaders.temp.bin").c_str(), "wb");
	if (!_ShadersFile) 
	{
		printf("[ Error ] Failed to open 'shaders.temp.bin' for writing...\n");
		return;
	}

	for (tinyxml2::XMLElement* _XMLShader = _XMLShaders->FirstChildElement("Shader"); _XMLShader; _XMLShader = _XMLShader->NextSiblingElement("Shader"))
	{
		const char* _ShaderName = _XMLShader->GetText();
		size_t _ShaderNameLength = strlen(_ShaderName);

		FILE* _ShaderFile = nullptr;
		fopen_s(&_ShaderFile, (_PackPath + _ShaderName).c_str(), "rb");
		if (!_ShaderFile) 
		{
			printf("[ Warning ] Failed to open %s\n", _ShaderName);
			continue;
		}

		fseek(_ShaderFile, 0, SEEK_END);

		size_t _ShaderFileSize = static_cast<size_t>(ftell(_ShaderFile));

		fseek(_ShaderFile, 0, SEEK_SET);

		size_t _ShaderBinaryTotalSize = (sizeof(Illusion::ShaderBinary_t) + _ShaderFileSize);
		Illusion::ShaderBinary_t* _ShaderBinary = reinterpret_cast<Illusion::ShaderBinary_t*>(malloc(_ShaderBinaryTotalSize));
		{
			// Resource Data
			memset(_ShaderBinary, 0, _ShaderBinaryTotalSize);
			_ShaderBinary->SetEntrySize(static_cast<uint32_t>(_ShaderBinaryTotalSize));
			_ShaderBinary->m_TypeUID = 0x985BE50C;
			_ShaderBinary->m_NameUID = SDK::StringHash32(_ShaderName);
			_ShaderBinary->m_ChunkUID = 0xE80F42E1;
			memcpy(_ShaderBinary->m_DebugName, _ShaderName, min(sizeof(UFG::ResourceData_t::m_DebugName) - 1, _ShaderNameLength));

			// Shader Data...
			_ShaderBinary->m_DataByteSize = static_cast<uint32_t>(_ShaderFileSize);
			_ShaderBinary->m_ShaderStageType = static_cast<Illusion::ShaderBinary_t::eStageType>(_XMLShader->IntAttribute("StageType"));
			_ShaderBinary->m_DataOffset = 0x40;
			memcpy(_ShaderBinary->m_ShaderName, _ShaderName, min(sizeof(Illusion::ShaderBinary_t::m_ShaderName) - 1, _ShaderNameLength));
		}

		fread(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&_ShaderBinary->m_DataOffset) + _ShaderBinary->m_DataOffset), 1, static_cast<size_t>(_ShaderBinary->m_DataByteSize), _ShaderFile);

		fclose(_ShaderFile);

		fwrite(_ShaderBinary, 1, _ShaderBinaryTotalSize, _ShadersFile);
		printf("[ ~ ] %s [Size: 0x%X]\n", _ShaderBinary->m_ShaderName, _ShaderBinary->m_DataByteSize);
		free(_ShaderBinary);
	}

	fclose(_ShadersFile);
}

int main(int p_Argc, char** p_Argv)
{
	if (p_Argc == 3) 
	{
		if (!strcmp(p_Argv[1], "-unpack"))
		{
			Unpack(p_Argv[2]);
			return 0;
		}

		if (!strcmp(p_Argv[1], "-pack"))
		{
			Pack(p_Argv[2]);
			return 0;
		}
	}

	printf("No action specified\n");
	printf("usage: %s [OPTION] FILE...\n\n", p_Argv[0]);

	printf(" -unpack\n");
	printf("\t\tUnpack shaders temp file\n");

	printf(" -pack\n");
	printf("\t\tPack shaders temp file with config file\n");
}