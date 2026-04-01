#include <Engine/AssetImport/ModelLoader.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>
#include <Engine/Debug/Log.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


namespace Antelope
{
    ModelData ModelLoader::Load(const std::string& filepath) 
    {
        Assimp::Importer importer;
        const aiScene* scene { importer.ReadFile(filepath,
                               aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
                               aiProcess_GenSmoothNormals | aiProcess_PreTransformVertices) };

        if (!scene || !scene->mRootNode)
        {
            AE_ENGINE_ERROR("Assimp Error: {0}", importer.GetErrorString());
            return {};
        }

        ModelData model;
        glm::vec3 minBound { FLT_MAX };
        glm::vec3 maxBound { -FLT_MAX };

        for (unsigned int m { 0 }; m < scene->mNumMeshes; m++) 
        {
            aiMesh* mesh { scene->mMeshes[m] };
            SubMeshData subMesh;
            subMesh.Name = mesh->mName.C_Str();
            subMesh.MaterialIndex = mesh->mMaterialIndex;

            for (unsigned int i { 0 }; i < mesh->mNumVertices; i++)
            {
                glm::vec3 pos { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
                minBound = glm::min(minBound, pos);
                maxBound = glm::max(maxBound, pos);

                subMesh.Data.positions.push_back({ pos });
                subMesh.Data.normals.push_back({ glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) });
                subMesh.Data.colors.push_back({ glm::vec3(1.0f) });
                
                if (mesh->mTextureCoords[0])
                    subMesh.Data.uvs.push_back({ {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y} });
                else
                    subMesh.Data.uvs.push_back({ {0.0f, 0.0f} });
            }

            for (unsigned int i { 0 }; i < mesh->mNumFaces; i++)
            {
                aiFace face { mesh->mFaces[i] };
                subMesh.Data.faces.push_back({ face.mIndices[0], face.mIndices[1], face.mIndices[2], 0 });
            }
            
            model.SubMeshes.push_back(subMesh);
        }

        AE_ENGINE_INFO("Model Loaded: {0}", filepath);
        return model;
    }
}