#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"

class Model
{
public:

	Model(const std::string path, const glm::vec3& defaultColor, std::vector<Texture>* loadedTextures)
	{
		m_defaultColor = defaultColor;
		loadModel(path, loadedTextures);
	}

	Model(const std::string path, std::vector<Texture>* loadedTextures) : Model{ path, glm::vec3{}, loadedTextures }
	{}

	void draw(float scale, const glm::vec3& position, const glm::vec3& radians, Shader& shader);

private:
	float     m_scale;
	glm::vec3 m_position;
	glm::vec3 m_rotation;

	std::vector<Mesh> m_meshes;
	std::string		  m_directory;

	glm::vec3 m_defaultColor;

	void loadModel(const std::string path, std::vector<Texture>* loadedTextures);
	void processNode(aiNode* node,   const aiScene* scene, std::vector<Texture>* loadedTextures);
	Mesh processMesh(aiMesh* aimesh, const aiScene* scene, std::vector<Texture>* loadedTextures);
	Texture loadMaterialTexture(aiMaterial* mat, aiTextureType type, Format internalFormat, std::vector<Texture>* loadedTextures);

};