#include "Model.h"

void Model::draw(const glm::vec3& scale, const glm::vec3& position, const glm::vec3& radians, Shader& shader)
{
	for (size_t i = 0; i < m_meshes.size(); i++)
	{
		m_meshes[i].draw(scale, position, radians, shader);
	}
}

void Model::draw(float scale, const glm::vec3& position, const glm::vec3& radians, Shader& shader)
{
	this->draw(glm::vec3{ scale }, position, radians, shader);
}

void Model::loadModel(const std::string path, std::vector<Texture>* loadedTextures)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cerr << "[Assimp error] :" << importer.GetErrorString() << std::endl;
		return;
	}

	//m_directory = path.substr(0, path.find_last_of('/'));
	m_path = path;

	processNode(scene->mRootNode, scene, loadedTextures);
}

void Model::processNode(aiNode* node, const aiScene* scene, std::vector<Texture>* loadedTextures)
{
	for (size_t i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* aimesh = scene->mMeshes[node->mMeshes[i]];
		m_meshes.push_back(processMesh(aimesh, scene, loadedTextures));
	}

	for (size_t i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene, loadedTextures);
	}
}

Mesh Model::processMesh(aiMesh* aimesh, const aiScene* scene, std::vector<Texture>* loadedTextures)
{
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	Texture diffuse;
	Texture specular;
	Texture normal;
	float shininess; 

	/* retrieve vertices */
	for (size_t i = 0; i < aimesh->mNumVertices; i++)
	{
		Vertex vertex;

		vertex.position = { aimesh->mVertices[i].x, aimesh->mVertices[i].y, aimesh->mVertices[i].z };
		vertex.normal   = { aimesh->mNormals[i].x,   aimesh->mNormals[i].y,  aimesh->mNormals[i].z };

		if (aimesh->mTextureCoords[0]) // TODO: add to documentation that we process only the first texture coordinates (assimp allows for multiple)
		{
			vertex.texCoords = { aimesh->mTextureCoords[0][i].x,   aimesh->mTextureCoords[0][i].y };
			vertex.tangent = { aimesh->mTangents[i].x, aimesh->mTangents[i].y, aimesh->mTangents[i].z };
		}
		else {
			vertex.texCoords = glm::vec2(0.0f, 0.0f);
			vertex.tangent = { 1.0, 0.0, 0.0 };

			std::cerr << "The mesh does not have UV coordinates: cannot assign proper tangent vectors." << std::endl;
			assert(false);
		}


		vertices.push_back(vertex);
	}


	/* retrieve indices */
	for (size_t i = 0; i < aimesh->mNumFaces; i++)
	{
		aiFace face = aimesh->mFaces[i];
		for (size_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	/* retrive material */
	if (aimesh->mMaterialIndex >= 0)
	{
		aiMaterial* aimaterial = scene->mMaterials[aimesh->mMaterialIndex];

		// Check what kinds of texture that are present
		for (int i = 0; i < aiTextureType::aiTextureType_UNKNOWN; i++) {
			enum aiTextureType type = (enum aiTextureType) i;
			if (aimaterial->GetTextureCount(type)) {
				aiString texturePath;
				aimaterial->GetTexture(type, 0, &texturePath);
				std::cout << "Texture type: " << type << ", path: " << texturePath.C_Str() << std::endl;
			}
		}

		diffuse  = loadMaterialTexture(aimaterial, aiTextureType_DIFFUSE,  Format::sRGBA, loadedTextures);
		specular = loadMaterialTexture(aimaterial, aiTextureType_SPECULAR, Format::RGBA, loadedTextures);
		normal   = loadMaterialTexture(aimaterial, aiTextureType_HEIGHT,   Format::RGBA, loadedTextures);

		if (diffuse.getPath() == "nopath")
		{
			int width = 1;
			int height = 1;
			int nrChannels = 3;
			unsigned char* color = (unsigned char*) alloca(nrChannels);
			color[0] = 255 * m_defaultColor[0];
			color[1] = 255 * m_defaultColor[1];
			color[2] = 255 * m_defaultColor[2];
			diffuse.generate(width, height,nrChannels, Format::sRGBA, color);
		}

		if (specular.getPath() == "nopath")
		{
			int width = 1;
			int height = 1;
			int nrChannels = 3;
			unsigned char* color = (unsigned char*)alloca(nrChannels);
			color[0] = 0;
			color[1] = 0;
			color[2] = 0;
			specular.generate(width, height, nrChannels, Format::RGBA, color);
		}

		if (normal.getPath() == "nopath")
		{
			int width = 1;
			int height = 1;
			int nrChannels = 3;
			unsigned char* color = (unsigned char*)alloca(nrChannels);
			color[0] = 133;
			color[1] = 133;
			color[2] = 255; // TODO: this does not work! You can see the bug by drawing (e.g.) a plane without a normal map just below a point light: only 
							// half of the plane gets correct lighting
			normal.generate(width, height, nrChannels, Format::RGBA, color);
		}

		shininess = 256.0f; // TODO: how to specify the shininess?
	}

	Material material;
	material.fill(diffuse, specular, normal, shininess);
	Mesh newMesh;
	newMesh.fill(vertices, indices, material);
	return newMesh;
}

Texture Model::loadMaterialTexture(aiMaterial* mat, aiTextureType type, Format internalFormat, std::vector<Texture>* loadedTextures)
{
	if (mat->GetTextureCount(type)) {

		aiString texturePath;
		mat->GetTexture(type, 0, &texturePath);
		//std::string fullPath = m_directory + "/" + std::string(texturePath.C_Str());
		std::string fullPath = std::string(texturePath.C_Str());

		// if texture does not already exist, generate it
		bool alreadyLoaded = false;
		for (size_t i = 0; i < loadedTextures->size(); i++)
		{
			
			if (std::strcmp(fullPath.c_str(), loadedTextures->at(i).getPath().c_str()) == 0)
			{
				// already loaded. return a copy of the texture (it copies only the path and the reference ID)
				return loadedTextures->at(i);
			}
		}

		if (!alreadyLoaded)
		{
			// add it to the vector of loaded textures and give the pointer to the input texture
			loadedTextures->push_back(Texture{});
			loadedTextures->back().generate(fullPath, internalFormat); //TODO: how to specify the format?	
			return loadedTextures->back();
		}

	}
	else
	{
		return Texture{};
	}
 }
