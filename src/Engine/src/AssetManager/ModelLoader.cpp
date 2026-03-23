#include <Engine/AssetManager/ModelLoader.hpp>
#include <Engine/Debug/Log.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


namespace Antelope {

    MeshData ModelLoader::Load(const std::string& filepath) 
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filepath,
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals | aiProcess_PreTransformVertices
        );

        if (!scene || !scene->mRootNode)
        {
            AE_ENGINE_ERROR("Assimp Error: {0}", importer.GetErrorString());
            return {};
        }

        MeshData combinedData;
        glm::vec3 minBound(FLT_MAX);
        glm::vec3 maxBound(-FLT_MAX);

        for (unsigned int m = 0; m < scene->mNumMeshes; m++) 
        {
            aiMesh* mesh = scene->mMeshes[m];
            uint32_t vertexOffset = static_cast<uint32_t>(combinedData.positions.size());

            for (unsigned int i = 0; i < mesh->mNumVertices; i++)
            {
                glm::vec3 pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
                
                minBound = glm::min(minBound, pos);
                maxBound = glm::max(maxBound, pos);

                combinedData.positions.push_back({ glm::vec4(pos, 1.0f) });
                combinedData.normals.push_back({ glm::vec4(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z, 0.0f) });
                combinedData.colors.push_back({ glm::vec4(1.0f) });
                
                if (mesh->mTextureCoords[0])
                {
                    combinedData.uvs.push_back({ {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y} });
                }
                else
                {
                    combinedData.uvs.push_back({ {0.0f, 0.0f} });
                }
            }

            for (unsigned int i = 0; i < mesh->mNumFaces; i++)
            {
                aiFace face = mesh->mFaces[i];
                combinedData.faces.push_back({ 
                    face.mIndices[0] + vertexOffset, 
                    face.mIndices[1] + vertexOffset, 
                    face.mIndices[2] + vertexOffset, 
                    0 
                });
            }
        }

        glm::vec3 center {(minBound + maxBound) * 0.5f };

        for (auto& vPos : combinedData.positions)
        {
            vPos.pos.x -= center.x;
            vPos.pos.y -= center.y;
            vPos.pos.z -= center.z;
        }

        AE_ENGINE_INFO("Model Loaded and Centered: {0}. Center Offset: ({1}, {2}, {3})", filepath, center.x, center.y, center.z);
        
        return combinedData;
    }
}