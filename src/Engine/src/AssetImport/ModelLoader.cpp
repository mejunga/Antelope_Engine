#include <Engine/AssetImport/ModelLoader.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>
#include <Engine/Debug/Log.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


namespace Antelope
{
    static glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from)
    {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    static void ProcessNode(aiNode* ai_node, const aiScene* scene, ModelNode& outNode)
    {
        outNode.Name = ai_node->mName.C_Str();
        outNode.LocalTransform = ConvertMatrixToGLMFormat(ai_node->mTransformation);

        for (unsigned int i = 0; i < ai_node->mNumMeshes; i++)
        {
            outNode.MeshIndices.push_back(ai_node->mMeshes[i]);
        }

        for (unsigned int i = 0; i < ai_node->mNumChildren; i++)
        {
            ModelNode childNode;
            ProcessNode(ai_node->mChildren[i], scene, childNode);
            outNode.Children.push_back(childNode);
        }
    }

    ModelData ModelLoader::Load(const std::string& filepath, bool preserveSkeleton)
    {
        Assimp::Importer importer;

        unsigned int flags { aiProcess_Triangulate | aiProcess_FlipUVs |
                            aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
                            aiProcess_ImproveCacheLocality }; 

        if (!preserveSkeleton) { flags |= aiProcess_PreTransformVertices; }

        const aiScene* scene { importer.ReadFile(filepath, flags) };

        if (!scene || !scene->mRootNode)
        {
            AE_ENGINE_ERROR("Assimp Error: {0}", importer.GetErrorString());
            return {};
        }

        ModelData model;

        for (unsigned int m { 0 }; m < scene->mNumMeshes; m++) 
        {
            aiMesh* mesh { scene->mMeshes[m] };
            SubMeshData subMesh;
            subMesh.Name = mesh->mName.C_Str();
            subMesh.MaterialIndex = mesh->mMaterialIndex;

            for (unsigned int i { 0 }; i < mesh->mNumVertices; i++)
            {
                glm::vec3 pos { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

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

        if (preserveSkeleton) 
        {
            ProcessNode(scene->mRootNode, scene, model.RootNode);
            AE_ENGINE_INFO("Model Loaded with Hierarchy: {0}", filepath);
        }
        else
        {
            AE_ENGINE_INFO("Model Loaded as Flat Mesh: {0}", filepath);
        }

        return model;
    }
}