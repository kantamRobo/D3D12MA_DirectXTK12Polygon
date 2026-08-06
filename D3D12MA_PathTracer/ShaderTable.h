#include "D3D12MemAlloc.h" // D3D12MAのヘッダーをインクルード

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable : public GpuUploadBuffer
{
    uint8_t* m_mappedShaderRecords;
    UINT m_shaderRecordSize;

    // Debug support
    std::wstring m_name;
    std::vector<ShaderRecord> m_shaderRecords;
    ShaderTable() {}
public:
    // 変更点: ID3D12Device* から D3D12MA::Allocator* に変更
    ShaderTable(D3D12MA::Allocator* allocator, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr)
        : m_name(resourceName)
    {
        m_shaderRecordSize = Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
        m_shaderRecords.reserve(numShaderRecords);
        UINT bufferSize = numShaderRecords * m_shaderRecordSize;
        Allocate(allocator, bufferSize, resourceName);
        m_mappedShaderRecords = MapCpuWriteOnly();
    }

    void push_back(const ShaderRecord& shaderRecord)
    {
        ThrowIfFalse(m_shaderRecords.size() < m_shaderRecords.capacity());
        m_shaderRecords.push_back(shaderRecord);
        shaderRecord.CopyTo(m_mappedShaderRecords);
        m_mappedShaderRecords += m_shaderRecordSize;
    }

    UINT GetShaderRecordSize() { return m_shaderRecordSize; }

    // Pretty-print the shader records.
    void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
    {
        std::wstringstream wstr;
        wstr << L"|--------------------------------------------------------------------\n";
        wstr << L"|Shader table - " << m_name.c_str() << L": "
            << m_shaderRecordSize << L" | "
            << m_shaderRecords.size() * m_shaderRecordSize << L" bytes\n";
        for (UINT i = 0; i < m_shaderRecords.size(); i++)
        {
            wstr << L"| [" << i << L"]: ";
            wstr << shaderIdToStringMap[m_shaderRecords[i].shaderIdentifier.ptr] << L", ";
            wstr << m_shaderRecords[i].shaderIdentifier.size << L" + " << m_shaderRecords[i].localRootArguments.size << L" bytes \n";
        }
        wstr << L"|--------------------------------------------------------------------\n";
        wstr << L"\n";
        OutputDebugStringW(wstr.str().c_str());
    }
};

class GpuUploadBuffer
{
public:
    ComPtr<ID3D12Resource> GetResource() { return m_resource; }
    // 追加: Allocationオブジェクトのゲッター
    ComPtr<D3D12MA::Allocation> GetAllocation() { return m_allocation; }

protected:
    ComPtr<ID3D12Resource> m_resource;
    // 追加: D3D12MAのAllocationオブジェクトを保持
    ComPtr<D3D12MA::Allocation> m_allocation;

    GpuUploadBuffer() {}
    ~GpuUploadBuffer()
    {
        if (m_resource.Get())
        {
            m_resource->Unmap(0, nullptr);
        }
        // m_resourceとm_allocationはComPtrなので自動的にReleaseされます
    }

    // 変更点: ID3D12Device* から D3D12MA::Allocator* に変更
    void Allocate(D3D12MA::Allocator* allocator, UINT bufferSize, LPCWSTR resourceName = nullptr)
    {
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

        // 変更点: CreateCommittedResourceの代わりにD3D12MAのALLOCATION_DESCを設定
        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

        ThrowIfFailed(allocator->CreateResource(
            &allocDesc,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            &m_allocation,
            IID_PPV_ARGS(&m_resource)));

        if (resourceName)
        {
            m_resource->SetName(resourceName);
            m_allocation->SetName(resourceName); // D3D12MA内部のデバッグ用にも名前を設定
        }
    }

    uint8_t* MapCpuWriteOnly()
    {
        uint8_t* mappedData;
        // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
        return mappedData;
    }
};