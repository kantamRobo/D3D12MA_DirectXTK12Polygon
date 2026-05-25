#pragma once
#include <BufferHelpers.h>
#include <vector>
#include <DirectXMath.h>
#include <VertexTypes.h>
using namespace DirectX;

class Model
{
public:
	std::vector<VertexPositionNormalColorTexture> vertices;
    std::vector<uint16_t> indices;
    bool LoadModel(const char* path);
	std::vector<DirectX::VertexPositionNormalColorTexture> GenerateVertices();
};

