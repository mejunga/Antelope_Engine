#include <Engine/Asset/ModelLoader.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Core/JobSystem.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <fstream>


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

        for (unsigned int i { 0 }; i < ai_node->mNumMeshes; i++)
        {
            outNode.MeshIndices.push_back(ai_node->mMeshes[i]);
        }

        for (unsigned int i { 0 }; i < ai_node->mNumChildren; i++)
        {
            ModelNode childNode;
            ProcessNode(ai_node->mChildren[i], scene, childNode);
            outNode.Children.push_back(childNode);
        }
    }

    static std::string ResolveTexturePath(const aiScene* scene, const char* rawPath, const std::filesystem::path& modelDir)
    {
        std::string path(rawPath);

        if (path[0] != '*')
            return std::filesystem::path(path).filename().string();

        int index { std::stoi(path.substr(1)) };
        const aiTexture* embTex { scene->mTextures[index] };

        std::string filename(embTex->mFilename.C_Str());
        if (filename.empty())
        {
            filename = "embedded_" + std::to_string(index) + "." + std::string(embTex->achFormatHint);
        }
        else
        {
            filename = std::filesystem::path(filename).filename().string();
        }

        std::filesystem::path outPath { modelDir / filename };

        if (!std::filesystem::exists(outPath) && embTex->mHeight == 0)
        {
            std::ofstream file(outPath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(embTex->pcData), embTex->mWidth);
            AE_ENGINE_INFO("Extracted embedded texture: '{0}'", outPath.string());
        }

        return filename;
    }

    static void ProcessMesh(aiMesh* mesh, SubMeshData& out)
    {
        out.Name = mesh->mName.C_Str();
        out.MaterialIndex = mesh->mMaterialIndex;

        out.Data.positions.reserve(mesh->mNumVertices);
        out.Data.normals.reserve(mesh->mNumVertices);
        out.Data.tangents.reserve(mesh->mNumVertices);
        out.Data.colors.reserve(mesh->mNumVertices);
        out.Data.uvs.reserve(mesh->mNumVertices);

        for (unsigned int i { 0 }; i < mesh->mNumVertices; ++i)
        {
            out.Data.positions.push_back({ glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z) });
            out.Data.normals.push_back({ glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) });

            if (mesh->mTangents)
            {
                out.Data.tangents.push_back({ glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z) });
            }
            else
            {
                out.Data.tangents.push_back({ glm::vec3(1.0f, 0.0f, 0.0f) });
            }

            out.Data.colors.push_back({ glm::vec3(1.0f) });

            if (mesh->mTextureCoords[0])
            {
                out.Data.uvs.push_back({ {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y} });
            }
            else
            {
                out.Data.uvs.push_back({ {0.0f, 0.0f} });
            }
        }

        out.Data.faces.reserve(mesh->mNumFaces);

        for (unsigned int i { 0 }; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face { mesh->mFaces[i] };
            out.Data.faces.push_back({ face.mIndices[0], face.mIndices[1], face.mIndices[2], 0 });
        }
    }

    ModelData ModelLoader::Load(const std::string& filepath, bool preserveSkeleton)
    {
        Assimp::Importer importer;

        unsigned int flags { aiProcess_Triangulate | aiProcess_FlipUVs |
                             aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
                             aiProcess_ImproveCacheLocality | aiProcess_CalcTangentSpace };

        if (!preserveSkeleton) { flags |= aiProcess_PreTransformVertices; }

        const aiScene* scene { importer.ReadFile(filepath, flags) };

        if (!scene || !scene->mRootNode)
        {
            AE_ENGINE_ERROR("Assimp Error: {0}", importer.GetErrorString());
            return {};
        }

        ModelData model;
        model.SubMeshes.resize(scene->mNumMeshes);
        model.Materials.reserve(scene->mNumMaterials);

        const uint32_t numMeshes { scene->mNumMeshes };
        auto& jobSystem { Application::Get().GetJobSystem() };

        if (numMeshes <= 4)
        {
            for (uint32_t m { 0 }; m < numMeshes; ++m)
            {
                ProcessMesh(scene->mMeshes[m], model.SubMeshes[m]);
            }
        }
        else
        {
            std::vector<JobHandle> handles;
            handles.reserve(numMeshes);

            for (uint32_t m { 0 }; m < numMeshes; ++m)
            {
                handles.push_back(jobSystem.Submit("MeshLoad", [m, scene, &model]()
                {
                    ProcessMesh(scene->mMeshes[m], model.SubMeshes[m]);
                }));
            }

            for (auto& h : handles) { h.wait(); }
        }

        std::filesystem::path modelDir { std::filesystem::path(filepath).parent_path() };

        for (unsigned int m { 0 }; m < scene->mNumMaterials; m++)
        {
            aiMaterial* aiMat { scene->mMaterials[m] };
            ModelMaterial modelMat;

            aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);

            if (aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS || 
                aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS)
            {
                modelMat.AlbedoFactor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
            }

            aiString texPath;

            if (aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS || 
                aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                modelMat.AlbedoTexPath = ResolveTexturePath(scene, texPath.C_Str(), modelDir);
                modelMat.AlbedoFactor = glm::vec4(1.0f); 
            }

            if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
                aiMat->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS)
            {
                modelMat.NormalTexPath = ResolveTexturePath(scene, texPath.C_Str(), modelDir);
                AE_ENGINE_INFO("Mat[{0}]: albedo='{1}' normal='{2}'", m, modelMat.AlbedoTexPath, modelMat.NormalTexPath);
            }

            float metallic { 0.0f }, roughness { 0.85f };
            aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
            aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
            modelMat.MetallicRoughnessFactors.x = metallic;
            modelMat.MetallicRoughnessFactors.y = roughness;
                        
            if (aiMat->GetTexture(aiTextureType_UNKNOWN, 0, &texPath) == AI_SUCCESS || 
                aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS ||
                aiMat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS)
            {
                modelMat.MetRoughAOTexPath = ResolveTexturePath(scene, texPath.C_Str(), modelDir);
            }

            aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 1.0f);

            if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS)
            {
                modelMat.MetallicRoughnessFactors.w = (emissiveColor.r + emissiveColor.g + emissiveColor.b) / 3.0f; 
            }

            if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS)
            {
                modelMat.EmissiveTexPath = ResolveTexturePath(scene, texPath.C_Str(), modelDir);
            }

            model.Materials.push_back(modelMat);
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