#include "pch.h"
#include "Model.h"



bool Model::LoadModel(const char* path)
{


    vertices = GenerateVertices();
    // スケール値を設定
    float scaleFactor = 10.0f;


    for (int i = 0; i < vertices.size(); i++)
    {
        //乗算　vertices[i].position = vertices[i].position,100.0f;
    }


    return true;
}




std::vector<DirectX::VertexPositionNormalColorTexture> Model::GenerateVertices()
{




    std::vector< DirectX::VertexPositionNormalColorTexture> outvertices;
    outvertices.clear();

    for (unsigned int i = 0; i < m_scene->mNumMeshes; i++)
    {
        aiMesh* mesh = m_scene->mMeshes[i];



        for (unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            DirectX::VertexPositionNormalColorTexture vertex = {};
            aiVector3D pos = mesh->mVertices[j];


            vertex.position = { pos.x , pos.y , pos.z };
            vertex.normal = { mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z };

            if (mesh->mTextureCoords[0])
            {
                vertex.textureCoordinate.x = mesh->mTextureCoords[0][j].x;
                vertex.textureCoordinate.y = mesh->mTextureCoords[0][j].y;
            }
            else
            {
                vertex.textureCoordinate.x = 0.0f;
                vertex.textureCoordinate.y = 0.0f;
            }

            outvertices.push_back(vertex);
        }

        // インデックスの設定
        for (unsigned int j = 0; j < mesh->mNumFaces; j++)
        {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++)
            {
                indices.push_back(face.mIndices[k]);
            }
        }
    }



    return outvertices;
}


void Model::BuildGeometry(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();


    m_vertexCount = static_cast<UINT>(vertices.size());
    m_indexCount = static_cast<UINT>(indices.size());


    // ----- Vertex buffer (D3D12MA UPLOAD heap) -----
    {
        UINT64 vbSize = sizeof(DirectX::VertexPositionNormalColorTexture) * m_vertexCount;
        D3D12MA::CALLOCATION_DESC allocDesc(D3D12_HEAP_TYPE_UPLOAD,
            D3D12MA::ALLOCATION_FLAG_NONE);
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        DX::ThrowIfFailed(m_allocator->CreateResource(
            &allocDesc, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            &m_vertexAllocation, IID_PPV_ARGS(&m_vertexBuffer)));
        m_vertexBuffer->SetName(L"SphereVertexBuffer");

        void* pData;
        CD3DX12_RANGE readRange(0, 0);
        DX::ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, &pData));
        memcpy(pData, vertices.data(), static_cast<size_t>(vbSize));
        m_vertexBuffer->Unmap(0, nullptr);
    }

    // ----- Index buffer (D3D12MA UPLOAD heap) -----
    {
        UINT64 ibSize = sizeof(uint16_t) * m_indexCount;
        D3D12MA::CALLOCATION_DESC allocDesc(D3D12_HEAP_TYPE_UPLOAD,
            D3D12MA::ALLOCATION_FLAG_NONE);
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
        DX::ThrowIfFailed(m_allocator->CreateResource(
            &allocDesc, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            &m_indexAllocation, IID_PPV_ARGS(&m_indexBuffer)));
        m_indexBuffer->SetName(L"SphereIndexBuffer");

        void* pData;
        CD3DX12_RANGE readRange(0, 0);
        DX::ThrowIfFailed(m_indexBuffer->Map(0, &readRange, &pData));
        memcpy(pData, indices.data(), static_cast<size_t>(ibSize));
        m_indexBuffer->Unmap(0, nu)llptr);
    }
}

// Compile the shader library.
std::vector<BYTE> dxilBlob;
void* pBytecode = nullptr;
SIZE_T bytecodeSize = 0;
CompileDXRShaderLibrary(L"RaytracingSphere.hlsl", &pBytecode, &bytecodeSize, dxilBlob);

// Build the RTPSO with 5 subobjects.
CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

// 1. DXIL library
{
    auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = { pBytecode, bytecodeSize };
    lib->SetDXILLibrary(&libdxil);
    lib->DefineExport(c_raygenShaderName);
    lib->DefineExport(c_closestHitShaderName);
    lib->DefineExport(c_missShaderName);
}

// 2. Triangle hit group (closest-hit only)
{
    auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
    hitGroup->SetHitGroupExport(c_hitGroupName);
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
}

// 3. Shader config
{
    auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    UINT payloadSize = 4 * sizeof(float); // float4 color
    UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
    shaderConfig->Config(payloadSize, attributeSize);
}

// 4. Global root signature
{
    auto globalRootSig = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSig->SetRootSignature(m_globalRootSignature.Get());
}

// 5. Pipeline config (no secondary rays → max recursion = 1)
{
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipelineConfig->Config(1);
}

DX::ThrowIfFailed(m_dxrDevice->CreateStateObject(
    raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)));
