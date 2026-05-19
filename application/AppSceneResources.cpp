#include "AppSceneResources.h"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <utility>
#include <vector>

#include "AppRenderResources.h"
#include "AppRuntimeUtils.h"
#include "ModelLoaderAssimp.h"

using Microsoft::WRL::ComPtr;

namespace {

    struct SphereVertex {
        float position[4];
        float texcoord[2];
        float normal[3];
    };

    struct SkyboxVertex {
        Vector4 position;
    };

    struct RingVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct CylinderVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    constexpr uint32_t kSkinningDescriptorBaseIndex = 20;
    constexpr uint32_t kSkinningDescriptorStride = 4;

    uint32_t GetSkinningDescriptorBaseIndex(uint32_t modelIndex) {
        return kSkinningDescriptorBaseIndex + modelIndex * kSkinningDescriptorStride;
    }

    std::vector<SphereVertex> BuildSphereVertices(uint32_t stackCount, uint32_t sliceCount) {
        std::vector<SphereVertex> v;
        v.reserve(stackCount * sliceCount * 6);

        for (uint32_t y = 0; y < stackCount; ++y) {
            float v0 = (float)y / (float)stackCount;
            float v1 = (float)(y + 1) / (float)stackCount;
            float phi0 = v0 * std::numbers::pi_v<float>;
            float phi1 = v1 * std::numbers::pi_v<float>;

            for (uint32_t x = 0; x < sliceCount; ++x) {
                float u0 = (float)x / (float)sliceCount;
                float u1 = (float)(x + 1) / (float)sliceCount;
                float theta0 = u0 * (std::numbers::pi_v<float> *2.0f);
                float theta1 = u1 * (std::numbers::pi_v<float> *2.0f);

                auto MakeV = [](float phi, float theta, float u, float vTex) {
                    SphereVertex sv{};
                    float sx = std::sin(phi) * std::cos(theta);
                    float sy = std::cos(phi);
                    float sz = std::sin(phi) * std::sin(theta);

                    sv.position[0] = sx;
                    sv.position[1] = sy;
                    sv.position[2] = sz;
                    sv.position[3] = 1.0f;

                    sv.texcoord[0] = u;
                    sv.texcoord[1] = vTex;

                    sv.normal[0] = sx;
                    sv.normal[1] = sy;
                    sv.normal[2] = sz;
                    return sv;
                    };

                SphereVertex a = MakeV(phi0, theta0, u0, v0);
                SphereVertex b = MakeV(phi0, theta1, u1, v0);
                SphereVertex c = MakeV(phi1, theta0, u0, v1);
                SphereVertex d = MakeV(phi1, theta1, u1, v1);

                v.push_back(a);
                v.push_back(c);
                v.push_back(b);
                v.push_back(b);
                v.push_back(c);
                v.push_back(d);
            }
        }

        return v;
    }

    std::vector<SkyboxVertex> BuildSkyboxVertices() {
        constexpr float k = 1.0f;
        const Vector4 p000{-k, -k, -k, 1.0f};
        const Vector4 p001{-k, -k,  k, 1.0f};
        const Vector4 p010{-k,  k, -k, 1.0f};
        const Vector4 p011{-k,  k,  k, 1.0f};
        const Vector4 p100{ k, -k, -k, 1.0f};
        const Vector4 p101{ k, -k,  k, 1.0f};
        const Vector4 p110{ k,  k, -k, 1.0f};
        const Vector4 p111{ k,  k,  k, 1.0f};

        return {
            {p100}, {p110}, {p111}, {p100}, {p111}, {p101},
            {p000}, {p001}, {p011}, {p000}, {p011}, {p010},
            {p010}, {p011}, {p111}, {p010}, {p111}, {p110},
            {p000}, {p100}, {p101}, {p000}, {p101}, {p001},
            {p001}, {p101}, {p111}, {p001}, {p111}, {p011},
            {p000}, {p010}, {p110}, {p000}, {p110}, {p100},
        };
    }

    std::vector<RingVertex> BuildRingVertices(uint32_t divide) {
        std::vector<RingVertex> vertices;
        divide = (std::max)(uint32_t{3}, divide);
        vertices.reserve(static_cast<size_t>(divide) * 6);
        const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divide);

        auto makeVertex = [](float sinValue, float cosValue, float v, float u) {
            RingVertex vertex{};
            vertex.position = {sinValue, cosValue, 0.0f, 1.0f};
            vertex.texcoord = {u, v};
            vertex.normal = {0.0f, 0.0f, -1.0f};
            return vertex;
        };

        for (uint32_t index = 0; index < divide; ++index) {
            const float angle = static_cast<float>(index) * radianPerDivide;
            const float nextAngle = static_cast<float>(index + 1) * radianPerDivide;
            const float sin0 = std::sin(angle);
            const float cos0 = std::cos(angle);
            const float sin1 = std::sin(nextAngle);
            const float cos1 = std::cos(nextAngle);
            const float u0 = static_cast<float>(index) / static_cast<float>(divide);
            const float u1 = static_cast<float>(index + 1) / static_cast<float>(divide);

            const RingVertex outer0 = makeVertex(sin0, cos0, 0.0f, u0);
            const RingVertex outer1 = makeVertex(sin1, cos1, 0.0f, u1);
            const RingVertex inner0 = makeVertex(sin0, cos0, 1.0f, u0);
            const RingVertex inner1 = makeVertex(sin1, cos1, 1.0f, u1);

            vertices.push_back(outer0);
            vertices.push_back(outer1);
            vertices.push_back(inner0);
            vertices.push_back(inner0);
            vertices.push_back(outer1);
            vertices.push_back(inner1);
        }
        return vertices;
    }

    std::vector<CylinderVertex> BuildCylinderVertices(uint32_t divide) {
        std::vector<CylinderVertex> vertices;
        divide = (std::max)(uint32_t{3}, divide);
        vertices.reserve(static_cast<size_t>(divide) * 6);
        const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divide);

        auto makeVertex = [](float sinValue, float cosValue, float y, float u, float v) {
            CylinderVertex vertex{};
            vertex.position = {-sinValue, y, cosValue, 1.0f};
            vertex.texcoord = {u, v};
            vertex.normal = {-sinValue, 0.0f, cosValue};
            return vertex;
        };

        for (uint32_t index = 0; index < divide; ++index) {
            const float angle = static_cast<float>(index) * radianPerDivide;
            const float nextAngle = static_cast<float>(index + 1) * radianPerDivide;
            const float sin0 = std::sin(angle);
            const float cos0 = std::cos(angle);
            const float sin1 = std::sin(nextAngle);
            const float cos1 = std::cos(nextAngle);
            const float u0 = static_cast<float>(index) / static_cast<float>(divide);
            const float u1 = static_cast<float>(index + 1) / static_cast<float>(divide);

            const CylinderVertex top0 = makeVertex(sin0, cos0, 1.0f, u0, 0.0f);
            const CylinderVertex top1 = makeVertex(sin1, cos1, 1.0f, u1, 0.0f);
            const CylinderVertex bottom0 = makeVertex(sin0, cos0, 0.0f, u0, 1.0f);
            const CylinderVertex bottom1 = makeVertex(sin1, cos1, 0.0f, u1, 1.0f);

            vertices.push_back(top0);
            vertices.push_back(top1);
            vertices.push_back(bottom0);
            vertices.push_back(bottom0);
            vertices.push_back(top1);
            vertices.push_back(bottom1);
        }
        return vertices;
    }

    const NodeAnimation* FindNodeAnimationRecursive(
        const AnimationClip& animation,
        const Node& node) {
        auto found = animation.nodeAnimations.find(node.name);
        if (found != animation.nodeAnimations.end()) {
            return &found->second;
        }

        for (const Node& child : node.children) {
            if (const NodeAnimation* childAnimation =
                    FindNodeAnimationRecursive(animation, child)) {
                return childAnimation;
            }
        }
        return nullptr;
    }

    const NodeAnimation* FindAnimatedCubeNodeAnimation(
        const AnimationClip& animation,
        const Node& rootNode) {
        if (const NodeAnimation* nodeAnimation =
                FindNodeAnimationRecursive(animation, rootNode)) {
            return nodeAnimation;
        }
        if (!animation.nodeAnimations.empty()) {
            return &animation.nodeAnimations.begin()->second;
        }
        return nullptr;
    }

    Vector3 ExtractTranslation(const Matrix4x4& matrix) {
        return { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };
    }

    Matrix4x4 InverseCopy(Matrix4x4 matrix) {
        return Inverse(matrix);
    }

    ComPtr<ID3D12Resource> CreateUavBufferResource(
        ComPtr<ID3D12Device> device,
        size_t sizeInBytes,
        D3D12_RESOURCE_STATES initialState) {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = (sizeInBytes + 0xFF) & ~static_cast<size_t>(0xFF);
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        ComPtr<ID3D12Resource> resource;
        const HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&resource));
        assert(SUCCEEDED(hr));
        return resource;
    }

    ComPtr<ID3D12Resource> CreateDefaultBufferResource(
        ComPtr<ID3D12Device> device,
        size_t sizeInBytes,
        D3D12_RESOURCE_STATES initialState) {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = (sizeInBytes + 0xFF) & ~static_cast<size_t>(0xFF);
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> resource;
        const HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&resource));
        assert(SUCCEEDED(hr));
        return resource;
    }

    void UploadStaticBufferData(
        ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* destination,
        const void* source,
        size_t sizeInBytes,
        D3D12_RESOURCE_STATES finalState,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources) {
        assert(commandList != nullptr);
        assert(destination != nullptr);
        assert(source != nullptr);
        assert(sizeInBytes > 0);

        ComPtr<ID3D12Resource> uploadResource = CreateBufferResource(device, sizeInBytes);
        void* mappedData = nullptr;
        uploadResource->Map(0, nullptr, &mappedData);
        std::memcpy(mappedData, source, sizeInBytes);
        uploadResource->Unmap(0, nullptr);

        commandList->CopyBufferRegion(
            destination,
            0,
            uploadResource.Get(),
            0,
            sizeInBytes);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = destination;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = finalState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        retainedUploadResources.push_back(uploadResource);
    }

    GpuMeshResource CreateGpuMeshResource(
        ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList* uploadCommandList,
        const ModelData& modelData,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources) {
        GpuMeshResource mesh{};
        mesh.vertexCount = UINT(modelData.vertices.size());
        mesh.indexCount = UINT(modelData.indices.size());
        if (mesh.vertexCount == 0 || mesh.indexCount == 0) {
            return mesh;
        }

        constexpr D3D12_RESOURCE_STATES kStaticVertexReadState =
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        mesh.vertexResource =
            CreateDefaultBufferResource(
                device,
                sizeof(VertexData) * modelData.vertices.size(),
                D3D12_RESOURCE_STATE_COPY_DEST);
        UploadStaticBufferData(
            device,
            uploadCommandList,
            mesh.vertexResource.Get(),
            modelData.vertices.data(),
            sizeof(VertexData) * modelData.vertices.size(),
            kStaticVertexReadState,
            retainedUploadResources);

        mesh.vbv.BufferLocation = mesh.vertexResource->GetGPUVirtualAddress();
        mesh.vbv.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
        mesh.vbv.StrideInBytes = sizeof(VertexData);

        mesh.indexResource =
            CreateDefaultBufferResource(
                device,
                sizeof(uint32_t) * modelData.indices.size(),
                D3D12_RESOURCE_STATE_COPY_DEST);
        UploadStaticBufferData(
            device,
            uploadCommandList,
            mesh.indexResource.Get(),
            modelData.indices.data(),
            sizeof(uint32_t) * modelData.indices.size(),
            D3D12_RESOURCE_STATE_INDEX_BUFFER,
            retainedUploadResources);

        mesh.ibv.BufferLocation = mesh.indexResource->GetGPUVirtualAddress();
        mesh.ibv.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
        mesh.ibv.Format = DXGI_FORMAT_R32_UINT;
        return mesh;
    }

    SkinCluster CreateSkinCluster(
        ComPtr<ID3D12Device> device,
        const Skeleton& skeleton,
        const ModelData& modelData,
        const GpuMeshResource& mesh,
        ID3D12GraphicsCommandList* uploadCommandList,
        ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        uint32_t skinningDescriptorBaseIndex,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources) {
        SkinCluster skinCluster{};
        const size_t jointCount = skeleton.joints.size();
        const size_t vertexCount = modelData.vertices.size();
        if (jointCount == 0 || vertexCount == 0 || !mesh.vertexResource) {
            return skinCluster;
        }

        const uint32_t vertexSrvIndex = skinningDescriptorBaseIndex + 0;
        const uint32_t influenceSrvIndex = skinningDescriptorBaseIndex + 1;
        const uint32_t paletteSrvIndex = skinningDescriptorBaseIndex + 2;
        const uint32_t skinnedVertexUavIndex = skinningDescriptorBaseIndex + 3;

        skinCluster.inverseBindPoseMatrices.resize(jointCount);
        std::fill(
            skinCluster.inverseBindPoseMatrices.begin(),
            skinCluster.inverseBindPoseMatrices.end(),
            MakeIdentity4x4());

        skinCluster.paletteEntries.resize(jointCount);
        skinCluster.paletteResource =
            CreateDefaultBufferResource(
                device,
                sizeof(JointPaletteEntry) * jointCount,
                D3D12_RESOURCE_STATE_COPY_DEST);
        skinCluster.paletteState = D3D12_RESOURCE_STATE_COPY_DEST;
        skinCluster.paletteUploadResource =
            CreateBufferResource(device, sizeof(JointPaletteEntry) * jointCount);
        skinCluster.paletteUploadResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skinCluster.mappedPaletteUpload));

        skinCluster.paletteSrvCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                paletteSrvIndex);
        skinCluster.paletteSrvGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                paletteSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
        paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        paletteSrvDesc.Buffer.FirstElement = 0;
        paletteSrvDesc.Buffer.NumElements = UINT(jointCount);
        paletteSrvDesc.Buffer.StructureByteStride = sizeof(JointPaletteEntry);
        paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(
            skinCluster.paletteResource.Get(),
            &paletteSrvDesc,
            skinCluster.paletteSrvCpu);

        skinCluster.vertexSrvCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                vertexSrvIndex);
        skinCluster.vertexSrvGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                vertexSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc{};
        vertexSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        vertexSrvDesc.Buffer.FirstElement = 0;
        vertexSrvDesc.Buffer.NumElements = UINT(vertexCount);
        vertexSrvDesc.Buffer.StructureByteStride = sizeof(VertexData);
        vertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(
            mesh.vertexResource.Get(),
            &vertexSrvDesc,
            skinCluster.vertexSrvCpu);

        std::vector<VertexInfluence> influences(vertexCount);

        constexpr D3D12_RESOURCE_STATES kStaticInfluenceReadState =
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        skinCluster.influenceResource =
            CreateDefaultBufferResource(
                device,
                sizeof(VertexInfluence) * vertexCount,
                D3D12_RESOURCE_STATE_COPY_DEST);
        skinCluster.influenceBufferView.BufferLocation =
            skinCluster.influenceResource->GetGPUVirtualAddress();
        skinCluster.influenceBufferView.SizeInBytes =
            UINT(sizeof(VertexInfluence) * vertexCount);
        skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

        skinCluster.influenceSrvCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                influenceSrvIndex);
        skinCluster.influenceSrvGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                influenceSrvIndex);

        D3D12_SHADER_RESOURCE_VIEW_DESC influenceSrvDesc{};
        influenceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        influenceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        influenceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        influenceSrvDesc.Buffer.FirstElement = 0;
        influenceSrvDesc.Buffer.NumElements = UINT(vertexCount);
        influenceSrvDesc.Buffer.StructureByteStride = sizeof(VertexInfluence);
        influenceSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        device->CreateShaderResourceView(
            skinCluster.influenceResource.Get(),
            &influenceSrvDesc,
            skinCluster.influenceSrvCpu);

        skinCluster.skinnedVertexResource = CreateUavBufferResource(
            device,
            sizeof(VertexData) * vertexCount,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        skinCluster.skinnedVertexState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        skinCluster.skinnedVertexBufferView.BufferLocation =
            skinCluster.skinnedVertexResource->GetGPUVirtualAddress();
        skinCluster.skinnedVertexBufferView.SizeInBytes =
            UINT(sizeof(VertexData) * vertexCount);
        skinCluster.skinnedVertexBufferView.StrideInBytes = sizeof(VertexData);

        skinCluster.skinnedVertexUavCpu =
            AppRenderResources::GetCPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                skinnedVertexUavIndex);
        skinCluster.skinnedVertexUavGpu =
            AppRenderResources::GetGPUDescriptorHandle(
                srvDescriptorHeap,
                descriptorSizeSRV,
                skinnedVertexUavIndex);

        D3D12_UNORDERED_ACCESS_VIEW_DESC skinnedVertexUavDesc{};
        skinnedVertexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
        skinnedVertexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        skinnedVertexUavDesc.Buffer.FirstElement = 0;
        skinnedVertexUavDesc.Buffer.NumElements = UINT(vertexCount);
        skinnedVertexUavDesc.Buffer.StructureByteStride = sizeof(VertexData);
        skinnedVertexUavDesc.Buffer.CounterOffsetInBytes = 0;
        skinnedVertexUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        device->CreateUnorderedAccessView(
            skinCluster.skinnedVertexResource.Get(),
            nullptr,
            &skinnedVertexUavDesc,
            skinCluster.skinnedVertexUavCpu);

        skinCluster.skinningInfoResource =
            CreateBufferResource(device, sizeof(SkinningInformation));
        skinCluster.skinningInfoResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skinCluster.mappedSkinningInfo));
        skinCluster.mappedSkinningInfo->numVertices = UINT(vertexCount);

        for (const auto& [jointName, jointWeight] : modelData.skinClusterData) {
            const auto jointIt = skeleton.jointMap.find(jointName);
            if (jointIt == skeleton.jointMap.end()) {
                continue;
            }

            const uint32_t jointIndex = static_cast<uint32_t>(jointIt->second);
            if (jointIndex >= skinCluster.inverseBindPoseMatrices.size()) {
                continue;
            }
            skinCluster.inverseBindPoseMatrices[jointIndex] =
                jointWeight.inverseBindPoseMatrix;

            for (const VertexWeightData& vertexWeight : jointWeight.vertexWeights) {
                if (vertexWeight.vertexIndex >= influences.size()) {
                    continue;
                }

                VertexInfluence& influence = influences[vertexWeight.vertexIndex];
                for (uint32_t influenceIndex = 0;
                     influenceIndex < kNumMaxInfluence;
                     ++influenceIndex) {
                    if (influence.weights[influenceIndex] == 0.0f) {
                        influence.weights[influenceIndex] = vertexWeight.weight;
                        influence.jointIndices[influenceIndex] =
                            static_cast<int32_t>(jointIndex);
                        break;
                    }
                }
            }
        }

        for (VertexInfluence& influence : influences) {
            float sum = 0.0f;
            for (float weight : influence.weights) {
                sum += weight;
            }
            if (sum <= 0.0f) {
                influence.weights[0] = 1.0f;
                influence.jointIndices[0] = 0;
                continue;
            }
            for (float& weight : influence.weights) {
                weight /= sum;
            }
        }

        UploadStaticBufferData(
            device,
            uploadCommandList,
            skinCluster.influenceResource.Get(),
            influences.data(),
            sizeof(VertexInfluence) * influences.size(),
            kStaticInfluenceReadState,
            retainedUploadResources);

        return skinCluster;
    }

    void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton) {
        if (skinCluster.paletteEntries.empty()) {
            return;
        }

        for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
            if (jointIndex >= skinCluster.inverseBindPoseMatrices.size() ||
                jointIndex >= skinCluster.paletteEntries.size()) {
                break;
            }

            Matrix4x4 skinMatrix = Multiply(
                skinCluster.inverseBindPoseMatrices[jointIndex],
                skeleton.joints[jointIndex].skeletonSpaceMatrix);
            skinCluster.paletteEntries[jointIndex].skeletonSpaceMatrix = skinMatrix;
            skinCluster.paletteEntries[jointIndex].skeletonSpaceInverseTransposeMatrix =
                Transpose(InverseCopy(skinMatrix));
        }
        skinCluster.paletteDirty = true;
    }

    bool LoadSkinnedModelInstance(
        SkinnedModelInstance& instance,
        ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList* uploadCommandList,
        ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        uint32_t descriptorSizeSRV,
        uint32_t skinningDescriptorBaseIndex,
        std::vector<ComPtr<ID3D12Resource>>& retainedUploadResources,
        const std::string& name,
        const std::string& directory,
        const std::string& filename,
        const Transform& transform) {
        instance = {};
        instance.name = name;
        instance.directory = directory;
        instance.filename = filename;
        instance.transform = transform;
        instance.animator.loop = true;
        instance.animator.playing = true;
        instance.animator.speed = 1.0f;

        instance.model = LoadObjFile_Assimp(directory, filename);
        instance.animation = LoadAnimationFile(directory, filename);
        if (instance.model.vertices.empty() ||
            instance.model.indices.empty() ||
            instance.model.rootNode.name.empty()) {
            OutputDebugStringA(("[AppSceneResources] Skinned model could not be loaded: " +
                directory + "/" + filename + "\n").c_str());
            return false;
        }

        instance.mesh = CreateGpuMeshResource(
            device,
            uploadCommandList,
            instance.model,
            retainedUploadResources);
        instance.skeleton = CreateSkeleton(instance.model.rootNode);
        ApplyAnimation(instance.skeleton, instance.animation, 0.0f);
        UpdateSkeleton(instance.skeleton);
        instance.skinCluster = CreateSkinCluster(
            device,
            instance.skeleton,
            instance.model,
            instance.mesh,
            uploadCommandList,
            srvDescriptorHeap,
            descriptorSizeSRV,
            skinningDescriptorBaseIndex,
            retainedUploadResources);
        UpdateSkinCluster(instance.skinCluster, instance.skeleton);

        instance.transformResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        instance.transformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&instance.transformData));
        instance.transformData->WVP = MakeIdentity4x4();
        instance.transformData->World = MakeIdentity4x4();
        instance.transformData->WorldInverseTranspose = MakeIdentity4x4();
        instance.loaded = instance.mesh.indexCount > 0 &&
            instance.skinCluster.paletteSrvGpu.ptr != 0 &&
            instance.transformData != nullptr;
        return instance.loaded;
    }

    void UpdateSkinnedModelInstance(
        SkinnedModelInstance& instance,
        float deltaTime,
        bool play,
        bool loop,
        float speed,
        float& animationTime) {
        if (!instance.loaded) {
            return;
        }

        const bool animationTimeChanged =
            std::fabs(instance.animator.time - animationTime) > 0.00001f;
        if (!play && !animationTimeChanged && !instance.skinCluster.paletteDirty) {
            instance.animator.playing = false;
            instance.animator.loop = loop;
            instance.animator.speed = speed;
            return;
        }

        instance.animator.time = animationTime;
        instance.animator.playing = play;
        instance.animator.loop = loop;
        instance.animator.speed = speed;
        if (play) {
            instance.animator.Update(deltaTime, instance.animation.duration);
            animationTime = instance.animator.time;
        }

        ApplyAnimation(instance.skeleton, instance.animation, animationTime);
        UpdateSkeleton(instance.skeleton);
        UpdateSkinCluster(instance.skinCluster, instance.skeleton);
    }

    void UpdateSkinnedModelInstanceTransform(
        SkinnedModelInstance& instance,
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projMatrix) {
        if (!instance.loaded || instance.transformData == nullptr) {
            return;
        }

        Matrix4x4 baseWorld = MakeAffineMatrix(
            instance.transform.scale,
            instance.transform.rotate,
            instance.transform.translate);
        instance.transformData->World = baseWorld;
        instance.transformData->WVP = Multiply(baseWorld, Multiply(viewMatrix, projMatrix));
        instance.transformData->WorldInverseTranspose = Transpose(Inverse(baseWorld));
    }

} // namespace

bool AppSceneResources::Initialize(
    ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList* uploadCommandList,
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
    uint32_t descriptorSizeSRV) {
    assert(uploadCommandList != nullptr);

    // =========================================================
    // Sprite geometry
    // =========================================================
    indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
    vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

    transformationMatrixResourceSprite =
        CreateBufferResource(device, sizeof(TransformationMatrix));
    transformationMatrixResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&transformationMatrixDataSprite));
    transformationMatrixDataSprite->WVP = MakeIdentity4x4();
    transformationMatrixDataSprite->World = MakeIdentity4x4();
    transformationMatrixDataSprite->WorldInverseTranspose = MakeIdentity4x4();

    indexBufferViewSprite.BufferLocation =
        indexResourceSprite->GetGPUVirtualAddress();
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

    vertexBufferViewSprite.BufferLocation =
        vertexResourceSprite->GetGPUVirtualAddress();
    vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

    uint32_t* indexDataSprite = nullptr;
    indexResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&indexDataSprite));
    indexDataSprite[0] = 0;
    indexDataSprite[1] = 1;
    indexDataSprite[2] = 2;
    indexDataSprite[3] = 3;
    indexDataSprite[4] = 4;
    indexDataSprite[5] = 5;

    VertexData* vertexDataSprite = nullptr;
    vertexResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&vertexDataSprite));

    vertexDataSprite[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[0].texcoord = { 0.0f, 1.0f };

    vertexDataSprite[1].position = { -0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[1].texcoord = { 0.0f, 0.0f };

    vertexDataSprite[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

    vertexDataSprite[3].position = { -0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[3].texcoord = { 0.0f, 0.0f };

    vertexDataSprite[4].position = { 0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[4].texcoord = { 1.0f, 0.0f };

    vertexDataSprite[5].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[5].texcoord = { 1.0f, 1.0f };

    // =========================================================
    // Materials
    // =========================================================
    materialResource = CreateBufferResource(device, sizeof(Material));
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->enableLighting = true;
    materialData->shininess = 3.0f;
    materialData->environmentCoefficient = 0.3f;
    materialData->specularMode = 1;
    materialData->uvTransform = MakeIdentity4x4();

    materialResourceSprite = CreateBufferResource(device, sizeof(Material));
    materialResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&materialDataSprite));
    materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialDataSprite->enableLighting = false;
    materialDataSprite->shininess = 1.0f;
    materialDataSprite->environmentCoefficient = 0.0f;
    materialDataSprite->specularMode = 1;
    materialDataSprite->uvTransform = MakeIdentity4x4();

    // =========================================================
    // Lights
    // =========================================================
    directionalLightData.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 dir = { 0.0f, -1.0f, 0.0f };
    dir = Normalize(dir);
    directionalLightData.direction = dir;
    directionalLightData.intensity = 1.5f;

    directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    directionalLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedLight));
    *mappedLight = directionalLightData;

    pointLightData.color = { 1,1,1,1 };
    pointLightData.position = { 0.0f, 2.0f, 0.0f };
    pointLightData.intensity = 1.0f;
    pointLightData.radius = 10.0f;
    pointLightData.decay = 2.0f;

    pointLightResource = CreateBufferResource(device, sizeof(PointLight));
    pointLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedPointLight));
    *mappedPointLight = pointLightData;

    spotLight.color = { 1,1,1,1 };
    spotLight.position = { 2.0f, 1.25f, 0.0f };
    spotLight.direction = Normalize(Vector3{ -1.0f, -1.0f, 0.0f });
    spotLight.distance = 7.0f;
    spotLight.intensity = 5.0f;
    spotLight.decay = 2.0f;
    spotLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);

    spotLightResource = CreateBufferResource(device, sizeof(SpotLight));
    spotLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedSpotLight));
    *mappedSpotLight = spotLight;

    // =========================================================
    // Camera
    // =========================================================
    cameraResource = CreateBufferResource(device, sizeof(CameraForGPU));
    cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedCamera));
    mappedCamera->worldPosition = Vector3{ 0.0f, 0.0f, -5.0f };
    mappedCamera->padding = 0.0f;

    // =========================================================
    // Texture 2枚
    // slot 1, 2 を使用（slot 0 は ImGui 用の前提）
    // =========================================================
    DirectX::ScratchImage mipImages = AppRenderResources::LoadTexture("resources/monsterBall.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    textureResource = AppRenderResources::CreateTextureResource(device, metadata);
    AppRenderResources::UploadTextureData(textureResource, mipImages);

    DirectX::ScratchImage mipImages2 = AppRenderResources::LoadTexture("resources/monsterBall.png");
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    textureResource2 = AppRenderResources::CreateTextureResource(device, metadata2);
    AppRenderResources::UploadTextureData(textureResource2, mipImages2);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    textureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    textureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    textureSrvHandleCPU2 = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    textureSrvHandleGPU2 = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

    const std::string circle2TexturePath =
        std::filesystem::exists("Resources/circle2.png") ? "Resources/circle2.png" : "resources/monsterBall.png";
    DirectX::ScratchImage circle2Images = AppRenderResources::LoadTexture(circle2TexturePath);
    const DirectX::TexMetadata& circle2Metadata = circle2Images.GetMetadata();
    circle2TextureResource = AppRenderResources::CreateTextureResource(device, circle2Metadata);
    AppRenderResources::UploadTextureData(circle2TextureResource, circle2Images);

    D3D12_SHADER_RESOURCE_VIEW_DESC circle2SrvDesc{};
    circle2SrvDesc.Format = circle2Metadata.format;
    circle2SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    circle2SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    circle2SrvDesc.Texture2D.MipLevels = UINT(circle2Metadata.mipLevels);

    circle2TextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);
    circle2TextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);
    device->CreateShaderResourceView(circle2TextureResource.Get(), &circle2SrvDesc, circle2TextureSrvHandleCPU);

    const std::string gradationLineTexturePath =
        std::filesystem::exists("Resources/gradationLine.png") ? "Resources/gradationLine.png" : circle2TexturePath;
    DirectX::ScratchImage gradationLineImages = AppRenderResources::LoadTexture(gradationLineTexturePath);
    const DirectX::TexMetadata& gradationLineMetadata = gradationLineImages.GetMetadata();
    gradationLineTextureResource = AppRenderResources::CreateTextureResource(device, gradationLineMetadata);
    AppRenderResources::UploadTextureData(gradationLineTextureResource, gradationLineImages);

    D3D12_SHADER_RESOURCE_VIEW_DESC gradationLineSrvDesc{};
    gradationLineSrvDesc.Format = gradationLineMetadata.format;
    gradationLineSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    gradationLineSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    gradationLineSrvDesc.Texture2D.MipLevels = UINT(gradationLineMetadata.mipLevels);

    gradationLineTextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);
    gradationLineTextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);
    device->CreateShaderResourceView(
        gradationLineTextureResource.Get(),
        &gradationLineSrvDesc,
        gradationLineTextureSrvHandleCPU);

    const std::string skyboxTexturePath = "Resources/rostock_laage_airport_4k.dds";
    if (std::filesystem::exists(skyboxTexturePath)) {
        DirectX::ScratchImage skyboxImages = AppRenderResources::LoadTexture(skyboxTexturePath);
        const DirectX::TexMetadata& skyboxMetadata = skyboxImages.GetMetadata();
        if (skyboxMetadata.IsCubemap()) {
            skyboxTextureResource = AppRenderResources::CreateTextureResource(device, skyboxMetadata);
            AppRenderResources::UploadTextureData(skyboxTextureResource, skyboxImages);

            D3D12_SHADER_RESOURCE_VIEW_DESC skyboxSrvDesc{};
            skyboxSrvDesc.Format = skyboxMetadata.format;
            skyboxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            skyboxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            skyboxSrvDesc.TextureCube.MostDetailedMip = 0;
            skyboxSrvDesc.TextureCube.MipLevels = UINT(skyboxMetadata.mipLevels);
            skyboxSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

            skyboxTextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
            skyboxTextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
            device->CreateShaderResourceView(skyboxTextureResource.Get(), &skyboxSrvDesc, skyboxTextureSrvHandleCPU);
        } else {
            OutputDebugStringA("[AppSceneResources] Skybox texture exists but is not a cubemap DDS.\n");
        }
    } else {
        OutputDebugStringA("[AppSceneResources] Skybox DDS not found. Skybox pass will be skipped.\n");
    }

    const std::string animatedCubeTexturePath =
        std::filesystem::exists("Resources/AnimatedCube/AnimatedCube_BaseColor.png")
            ? "Resources/AnimatedCube/AnimatedCube_BaseColor.png"
            : "resources/monsterBall.png";
    DirectX::ScratchImage animatedCubeImages =
        AppRenderResources::LoadTexture(animatedCubeTexturePath);
    const DirectX::TexMetadata& animatedCubeMetadata = animatedCubeImages.GetMetadata();
    animatedCubeTextureResource =
        AppRenderResources::CreateTextureResource(device, animatedCubeMetadata);
    AppRenderResources::UploadTextureData(animatedCubeTextureResource, animatedCubeImages);

    D3D12_SHADER_RESOURCE_VIEW_DESC animatedCubeSrvDesc{};
    animatedCubeSrvDesc.Format = animatedCubeMetadata.format;
    animatedCubeSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    animatedCubeSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    animatedCubeSrvDesc.Texture2D.MipLevels = UINT(animatedCubeMetadata.mipLevels);

    animatedCubeTextureSrvHandleCPU =
        AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 7);
    animatedCubeTextureSrvHandleGPU =
        AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 7);
    device->CreateShaderResourceView(
        animatedCubeTextureResource.Get(),
        &animatedCubeSrvDesc,
        animatedCubeTextureSrvHandleCPU);

    vfxTextureLibrary.clear();
    auto registerExistingVfxTexture = [this](
        std::string name,
        std::string path,
        ComPtr<ID3D12Resource> resource,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu,
        uint32_t descriptorIndex,
        const DirectX::TexMetadata& textureMetadata) {
        if (resource == nullptr || gpu.ptr == 0) {
            return;
        }
        vfxTextureLibrary.push_back({
            std::move(name),
            std::move(path),
            resource,
            cpu,
            gpu,
            descriptorIndex,
            static_cast<uint32_t>(textureMetadata.width),
            static_cast<uint32_t>(textureMetadata.height)
        });
    };

    registerExistingVfxTexture(
        "default",
        "Resources/monsterBall.png",
        textureResource,
        textureSrvHandleCPU,
        textureSrvHandleGPU,
        1,
        metadata);
    registerExistingVfxTexture(
        "monsterBall",
        "Resources/monsterBall.png",
        textureResource2,
        textureSrvHandleCPU2,
        textureSrvHandleGPU2,
        2,
        metadata2);
    registerExistingVfxTexture(
        "circle2",
        circle2TexturePath,
        circle2TextureResource,
        circle2TextureSrvHandleCPU,
        circle2TextureSrvHandleGPU,
        5,
        circle2Metadata);
    registerExistingVfxTexture(
        "gradationLine",
        gradationLineTexturePath,
        gradationLineTextureResource,
        gradationLineTextureSrvHandleCPU,
        gradationLineTextureSrvHandleGPU,
        6,
        gradationLineMetadata);

    struct VfxTextureLoadSpec {
        const char* name;
        const char* path;
    };
    constexpr uint32_t kVfxTextureDescriptorBaseIndex = 128;
    const VfxTextureLoadSpec vfxTextureLoadSpecs[] = {
        {"streakNoise", "Resources/streakNoise.png"},
        {"circle", "Resources/circle.png"},
        {"beamRamp_lightning", "Resources/beamRamp_lightning.png"},
        {"uvChecker", "Resources/uvChecker.png"},
        {"fence", "Resources/fence/fence.png"},
    };

    for (uint32_t index = 0; index < _countof(vfxTextureLoadSpecs); ++index) {
        const VfxTextureLoadSpec& spec = vfxTextureLoadSpecs[index];
        if (!std::filesystem::exists(spec.path)) {
            continue;
        }

        DirectX::ScratchImage vfxTextureImages = AppRenderResources::LoadTexture(spec.path);
        const DirectX::TexMetadata& vfxTextureMetadata = vfxTextureImages.GetMetadata();
        ComPtr<ID3D12Resource> vfxTextureResource =
            AppRenderResources::CreateTextureResource(device, vfxTextureMetadata);
        AppRenderResources::UploadTextureData(vfxTextureResource, vfxTextureImages);

        D3D12_SHADER_RESOURCE_VIEW_DESC vfxTextureSrvDesc{};
        vfxTextureSrvDesc.Format = vfxTextureMetadata.format;
        vfxTextureSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        vfxTextureSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        vfxTextureSrvDesc.Texture2D.MipLevels = UINT(vfxTextureMetadata.mipLevels);

        const uint32_t descriptorIndex = kVfxTextureDescriptorBaseIndex + index;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu =
            AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpu =
            AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, descriptorIndex);
        device->CreateShaderResourceView(vfxTextureResource.Get(), &vfxTextureSrvDesc, cpu);

        vfxTextureLibrary.push_back({
            spec.name,
            spec.path,
            vfxTextureResource,
            cpu,
            gpu,
            descriptorIndex,
            static_cast<uint32_t>(vfxTextureMetadata.width),
            static_cast<uint32_t>(vfxTextureMetadata.height)
        });
    }

    // =========================================================
    // Sphere mesh
    // =========================================================
    {
        const uint32_t sphereStacks = 32;
        const uint32_t sphereSlices = 64;
        std::vector<SphereVertex> sphereVerts =
            BuildSphereVertices(sphereStacks, sphereSlices);

        sphere.vertexCount = (UINT)sphereVerts.size();
        sphere.vertexResource =
            CreateBufferResource(device, sizeof(SphereVertex) * sphere.vertexCount);

        SphereVertex* mappedVB = nullptr;
        sphere.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, sphereVerts.data(), sizeof(SphereVertex) * sphere.vertexCount);

        sphere.vbv.BufferLocation = sphere.vertexResource->GetGPUVirtualAddress();
        sphere.vbv.SizeInBytes = (UINT)(sizeof(SphereVertex) * sphere.vertexCount);
        sphere.vbv.StrideInBytes = sizeof(SphereVertex);

        sphere.cbvResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        sphere.cbvResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&sphere.mappedCBV));
        sphere.mappedCBV->WVP = MakeIdentity4x4();
        sphere.mappedCBV->World = MakeIdentity4x4();
        sphere.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }

    // =========================================================
    // Skybox mesh
    // =========================================================
    {
        const std::vector<SkyboxVertex> skyboxVerts = BuildSkyboxVertices();
        skybox.vertexCount = UINT(skyboxVerts.size());
        skybox.vertexResource =
            CreateBufferResource(device, sizeof(SkyboxVertex) * skybox.vertexCount);

        SkyboxVertex* mappedVB = nullptr;
        skybox.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, skyboxVerts.data(), sizeof(SkyboxVertex) * skybox.vertexCount);

        skybox.vbv.BufferLocation = skybox.vertexResource->GetGPUVirtualAddress();
        skybox.vbv.SizeInBytes = UINT(sizeof(SkyboxVertex) * skybox.vertexCount);
        skybox.vbv.StrideInBytes = sizeof(SkyboxVertex);

        skybox.cbvResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        skybox.cbvResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skybox.mappedCBV));
        skybox.mappedCBV->WVP = MakeIdentity4x4();
        skybox.mappedCBV->World = MakeIdentity4x4();
        skybox.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }

    // =========================================================
    // VFX Ring mesh
    // =========================================================
    {
        const std::vector<RingVertex> ringVerts = BuildRingVertices(128);
        ring.vertexCount = UINT(ringVerts.size());
        ring.vertexResource =
            CreateBufferResource(device, sizeof(RingVertex) * ring.vertexCount);

        RingVertex* mappedVB = nullptr;
        ring.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, ringVerts.data(), sizeof(RingVertex) * ring.vertexCount);

        ring.vbv.BufferLocation = ring.vertexResource->GetGPUVirtualAddress();
        ring.vbv.SizeInBytes = UINT(sizeof(RingVertex) * ring.vertexCount);
        ring.vbv.StrideInBytes = sizeof(RingVertex);
    }

    // =========================================================
    // VFX Cylinder mesh
    // =========================================================
    {
        const std::vector<CylinderVertex> cylinderVerts = BuildCylinderVertices(128);
        cylinder.vertexCount = UINT(cylinderVerts.size());
        cylinder.vertexResource =
            CreateBufferResource(device, sizeof(CylinderVertex) * cylinder.vertexCount);

        CylinderVertex* mappedVB = nullptr;
        cylinder.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, cylinderVerts.data(), sizeof(CylinderVertex) * cylinder.vertexCount);

        cylinder.vbv.BufferLocation = cylinder.vertexResource->GetGPUVirtualAddress();
        cylinder.vbv.SizeInBytes = UINT(sizeof(CylinderVertex) * cylinder.vertexCount);
        cylinder.vbv.StrideInBytes = sizeof(CylinderVertex);
    }

    // =========================================================
    // Assimp model mesh
    // =========================================================
    modelData = LoadObjFile_Assimp("Resources/ball", "ball.obj");
    assert(!modelData.vertices.empty());
    assert(!modelData.indices.empty());
    modelMesh = CreateGpuMeshResource(
        device,
        uploadCommandList,
        modelData,
        initialUploadResources_);

    // =========================================================
    // AnimatedCube model and animation
    // =========================================================
    animatedCubeData = LoadObjFile_Assimp("Resources/AnimatedCube", "AnimatedCube.gltf");
    animatedCubeAnimation = LoadAnimationFile("Resources/AnimatedCube", "AnimatedCube.gltf");
    if (!animatedCubeData.vertices.empty() && !animatedCubeData.indices.empty()) {
        animatedCubeMesh = CreateGpuMeshResource(
            device,
            uploadCommandList,
            animatedCubeData,
            initialUploadResources_);

        animatedCubeTransformResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        animatedCubeTransformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&animatedCubeTransformData));
        animatedCubeTransformData->WVP = MakeIdentity4x4();
        animatedCubeTransformData->World = MakeIdentity4x4();
        animatedCubeTransformData->WorldInverseTranspose = MakeIdentity4x4();

    } else {
        OutputDebugStringA("[AppSceneResources] AnimatedCube model has no indexed mesh data.\n");
    }

    // =========================================================
    // Managed VFX model library and object instances
    // =========================================================
    vfxModelLibrary.clear();
    if (modelMesh.indexCount > 0) {
        vfxModelLibrary.push_back({
            "ball",
            "Resources/ball",
            "ball.obj",
            modelData,
            modelMesh,
            textureSrvHandleGPU2,
            true,
        });
    }
    if (animatedCubeMesh.indexCount > 0) {
        vfxModelLibrary.push_back({
            "animated_cube",
            "Resources/AnimatedCube",
            "AnimatedCube.gltf",
            animatedCubeData,
            animatedCubeMesh,
            animatedCubeTextureSrvHandleGPU,
            animatedCubeTextureSrvHandleGPU.ptr != 0,
        });
    }

    vfxModelObjects.clear();
    vfxModelObjects.resize(kRuntimeVfxModelObjectCount);
    for (size_t index = 0; index < vfxModelObjects.size(); ++index) {
        AppModelObjectInstance& object = vfxModelObjects[index];
        object.name = "vfx_model_object_" + std::to_string(index);
        object.modelIndex = vfxModelLibrary.empty()
            ? 0
            : static_cast<uint32_t>(index % vfxModelLibrary.size());
        object.visible = true;
        object.transformResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        object.transformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&object.transformData));
        object.transformData->WVP = MakeIdentity4x4();
        object.transformData->World = MakeIdentity4x4();
        object.transformData->WorldInverseTranspose = MakeIdentity4x4();
    }

    // =========================================================
    // Skinned model instances
    // =========================================================
    const Transform skinnedDefaultTransform{
        { 0.45f, 0.45f, 0.45f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, -0.4f, -6.3f },
    };
    LoadSkinnedModelInstance(
        skinnedModels[0],
        device,
        uploadCommandList,
        srvDescriptorHeap,
        descriptorSizeSRV,
        GetSkinningDescriptorBaseIndex(0),
        initialUploadResources_,
        "simpleSkin",
        "Resources/simpleSkin",
        "simpleSkin.gltf",
        skinnedDefaultTransform);
    LoadSkinnedModelInstance(
        skinnedModels[1],
        device,
        uploadCommandList,
        srvDescriptorHeap,
        descriptorSizeSRV,
        GetSkinningDescriptorBaseIndex(1),
        initialUploadResources_,
        "human walk",
        "Resources/human",
        "walk.gltf",
        skinnedDefaultTransform);
    LoadSkinnedModelInstance(
        skinnedModels[2],
        device,
        uploadCommandList,
        srvDescriptorHeap,
        descriptorSizeSRV,
        GetSkinningDescriptorBaseIndex(2),
        initialUploadResources_,
        "human sneakWalk",
        "Resources/human",
        "sneakWalk.gltf",
        skinnedDefaultTransform);

    size_t maxJointCount = 0;
    for (const SkinnedModelInstance& instance : skinnedModels) {
        if (instance.loaded) {
            maxJointCount = (std::max)(maxJointCount, instance.skeleton.joints.size());
        }
    }
    if (maxJointCount > 0) {
        const size_t hierarchyLineVertices = maxJointCount > 0 ? (maxJointCount - 1) * 2 : 0;
        const size_t fallbackAnimatedJointLineVertices = maxJointCount > 0 ? (maxJointCount - 1) * 2 : 0;
        const size_t jointMarkerVertices = maxJointCount * 6;
        skeletonDebugVertexCapacity =
            UINT(hierarchyLineVertices + fallbackAnimatedJointLineVertices + jointMarkerVertices);
        skeletonDebugVertexResource = CreateBufferResource(
            device,
            sizeof(SkeletonDebugLineVertex) * skeletonDebugVertexCapacity);
        skeletonDebugVertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedSkeletonDebugLines));
        skeletonDebugVBV.BufferLocation = skeletonDebugVertexResource->GetGPUVirtualAddress();
        skeletonDebugVBV.SizeInBytes =
            UINT(sizeof(SkeletonDebugLineVertex) * skeletonDebugVertexCapacity);
        skeletonDebugVBV.StrideInBytes = sizeof(SkeletonDebugLineVertex);

        skeletonDebugTransformResource =
            CreateBufferResource(device, sizeof(TransformationMatrix));
        skeletonDebugTransformResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skeletonDebugTransformData));
        skeletonDebugTransformData->WVP = MakeIdentity4x4();
        skeletonDebugTransformData->World = MakeIdentity4x4();
        skeletonDebugTransformData->WorldInverseTranspose = MakeIdentity4x4();
    }

    return true;
}

void AppSceneResources::ReleaseInitialUploadResources() {
    initialUploadResources_.clear();
}

void AppSceneResources::UpdateCameraWorldPosition(const Vector3& worldPosition) {
    if (mappedCamera == nullptr) {
        return;
    }

    mappedCamera->worldPosition = worldPosition;
}

SkinnedModelInstance* AppSceneResources::GetActiveSkinnedModel() {
    if (activeSkinnedModelIndex >= skinnedModels.size()) {
        return nullptr;
    }
    SkinnedModelInstance& instance = skinnedModels[activeSkinnedModelIndex];
    return instance.loaded ? &instance : nullptr;
}

const SkinnedModelInstance* AppSceneResources::GetActiveSkinnedModel() const {
    if (activeSkinnedModelIndex >= skinnedModels.size()) {
        return nullptr;
    }
    const SkinnedModelInstance& instance = skinnedModels[activeSkinnedModelIndex];
    return instance.loaded ? &instance : nullptr;
}

const AppManagedModelResource* AppSceneResources::FindManagedModel(uint32_t modelIndex) const {
    if (modelIndex >= vfxModelLibrary.size()) {
        return nullptr;
    }
    const AppManagedModelResource& model = vfxModelLibrary[modelIndex];
    return model.loaded ? &model : nullptr;
}

void AppSceneResources::UpdateTransforms(
    const AppRuntimeState& runtimeState,
    Matrix4x4* wvpData,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projMatrix,
    uint32_t windowWidth,
    uint32_t windowHeight) {
    if (wvpData != nullptr) {
        Matrix4x4 worldMatrix = MakeAffineMatrix(
            runtimeState.transform.scale,
            runtimeState.transform.rotate,
            runtimeState.transform.translate);
        *wvpData = Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
    }

    if (transformationMatrixDataSprite != nullptr) {
        Matrix4x4 worldMatrixSprite = MakeAffineMatrix(
            runtimeState.transformSprite.scale,
            runtimeState.transformSprite.rotate,
            runtimeState.transformSprite.translate);
        Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
            0.0f,
            0.0f,
            float(windowWidth),
            float(windowHeight),
            0.0f,
            100.0f);
        transformationMatrixDataSprite->World = worldMatrixSprite;
        transformationMatrixDataSprite->WVP = Multiply(
            worldMatrixSprite,
            Multiply(MakeIdentity4x4(), projectionMatrixSprite));
        transformationMatrixDataSprite->WorldInverseTranspose =
            Transpose(Inverse(worldMatrixSprite));
    }

    if (materialDataSprite != nullptr) {
        Matrix4x4 uvTransformMatrix = MakeScaleMatrix(runtimeState.uvTransformSprite.scale);
        uvTransformMatrix = Multiply(
            uvTransformMatrix,
            MakeRoateZMatrix(runtimeState.uvTransformSprite.rotate.z));
        uvTransformMatrix = Multiply(
            uvTransformMatrix,
            MakeTranslateMatrix(runtimeState.uvTransformSprite.translate));
        materialDataSprite->uvTransform = uvTransformMatrix;
    }

    if (sphere.mappedCBV != nullptr) {
        Matrix4x4 worldMatrixSphere = MakeAffineMatrix(
            runtimeState.transform.scale,
            runtimeState.transform.rotate,
            runtimeState.transform.translate);
        const Matrix4x4& rootLocal = modelData.rootNode.localMatrix;
        Matrix4x4 worldWithNode = Multiply(rootLocal, worldMatrixSphere);
        Matrix4x4 wvpWithNode = Multiply(worldWithNode, Multiply(viewMatrix, projMatrix));
        sphere.mappedCBV->World = worldWithNode;
        sphere.mappedCBV->WVP = wvpWithNode;
        sphere.mappedCBV->WorldInverseTranspose = Transpose(Inverse(worldWithNode));
    }

    if (animatedCubeTransformData != nullptr) {
        Matrix4x4 animatedLocal = animatedCubeData.rootNode.localMatrix;
        if (const NodeAnimation* nodeAnimation =
                FindAnimatedCubeNodeAnimation(animatedCubeAnimation, animatedCubeData.rootNode)) {
            animatedLocal = MakeNodeAnimationMatrix(*nodeAnimation, runtimeState.animatedCubeTime);
        }

        Matrix4x4 baseWorld = MakeAffineMatrix(
            runtimeState.animatedCubeTransform.scale,
            runtimeState.animatedCubeTransform.rotate,
            runtimeState.animatedCubeTransform.translate);
        Matrix4x4 worldMatrix = Multiply(animatedLocal, baseWorld);
        animatedCubeTransformData->World = worldMatrix;
        animatedCubeTransformData->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
        animatedCubeTransformData->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

    }

    for (size_t index = 0;
         index < vfxModelObjects.size() && index < runtimeState.vfxModelObjects.size();
         ++index) {
        AppModelObjectInstance& object = vfxModelObjects[index];
        const RuntimeVfxModelObjectState& objectState = runtimeState.vfxModelObjects[index];
        uint32_t modelIndex = objectState.modelIndex;
        if (!vfxModelLibrary.empty()) {
            modelIndex = (std::min)(
                modelIndex,
                static_cast<uint32_t>(vfxModelLibrary.size() - 1));
        }

        const AppManagedModelResource* managedModel = FindManagedModel(modelIndex);
        object.modelIndex = modelIndex;
        object.transform = objectState.transform;
        object.visible =
            runtimeState.showVfxModelObjects &&
            objectState.visible &&
            managedModel != nullptr &&
            object.transformData != nullptr;

        if (!object.visible) {
            continue;
        }

        Matrix4x4 worldMatrix = MakeAffineMatrix(
            object.transform.scale,
            object.transform.rotate,
            object.transform.translate);
        worldMatrix = Multiply(managedModel->model.rootNode.localMatrix, worldMatrix);
        object.transformData->World = worldMatrix;
        object.transformData->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
        object.transformData->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
    }

    SkinnedModelInstance* activeSkinnedModel = GetActiveSkinnedModel();
    if (activeSkinnedModel != nullptr) {
        activeSkinnedModel->visible = runtimeState.showSkinnedModel;
        activeSkinnedModel->transform = runtimeState.skinnedModelTransform;
        if (activeSkinnedModel->visible) {
            UpdateSkinnedModelInstanceTransform(*activeSkinnedModel, viewMatrix, projMatrix);
        }

        if (runtimeState.showSkeletonDebug &&
            mappedSkeletonDebugLines != nullptr &&
            skeletonDebugTransformData != nullptr) {
            skeletonDebugVertexCount = 0;
            const Vector4 rootColor = { 1.0f, 0.9f, 0.1f, 1.0f };
            const Vector4 childColor = { 0.0f, 0.95f, 1.0f, 1.0f };
            const Vector4 markerXColor = { 1.0f, 0.15f, 0.15f, 1.0f };
            const Vector4 markerYColor = { 0.1f, 1.0f, 0.25f, 1.0f };
            const Vector4 markerZColor = { 0.2f, 0.55f, 1.0f, 1.0f };
            const Vector4 fallbackLinkColor = { 1.0f, 0.15f, 1.0f, 1.0f };
            const float markerSize = 0.06f;
            std::vector<Vector3> animatedJointPositions;
            size_t animatedHierarchyLinkCount = 0;
            auto pushLine = [&](const Vector3& a, const Vector3& b, const Vector4& colorA, const Vector4& colorB) {
                if (skeletonDebugVertexCount + 2 > skeletonDebugVertexCapacity) {
                    return;
                }
                mappedSkeletonDebugLines[skeletonDebugVertexCount++] = {
                    { a.x, a.y, a.z, 1.0f },
                    colorA,
                };
                mappedSkeletonDebugLines[skeletonDebugVertexCount++] = {
                    { b.x, b.y, b.z, 1.0f },
                    colorB,
                };
            };
            auto distanceSquared = [](const Vector3& a, const Vector3& b) {
                const float dx = a.x - b.x;
                const float dy = a.y - b.y;
                const float dz = a.z - b.z;
                return dx * dx + dy * dy + dz * dz;
            };

            const Skeleton& skeleton = activeSkinnedModel->skeleton;
            const AnimationClip& animation = activeSkinnedModel->animation;
            for (const Joint& joint : skeleton.joints) {
                const Vector3 jointPosition = ExtractTranslation(joint.skeletonSpaceMatrix);
                const bool isAnimatedJoint =
                    animation.nodeAnimations.find(joint.name) != animation.nodeAnimations.end();
                if (isAnimatedJoint) {
                    animatedJointPositions.push_back(jointPosition);
                }

                pushLine(
                    { jointPosition.x - markerSize, jointPosition.y, jointPosition.z },
                    { jointPosition.x + markerSize, jointPosition.y, jointPosition.z },
                    markerXColor,
                    markerXColor);
                pushLine(
                    { jointPosition.x, jointPosition.y - markerSize, jointPosition.z },
                    { jointPosition.x, jointPosition.y + markerSize, jointPosition.z },
                    markerYColor,
                    markerYColor);
                pushLine(
                    { jointPosition.x, jointPosition.y, jointPosition.z - markerSize },
                    { jointPosition.x, jointPosition.y, jointPosition.z + markerSize },
                    markerZColor,
                    markerZColor);

                if (!joint.parent.has_value()) {
                    continue;
                }

                const Joint& parent = skeleton.joints[static_cast<size_t>(*joint.parent)];
                const Vector3 parentPosition = ExtractTranslation(parent.skeletonSpaceMatrix);
                const bool parentIsAnimatedJoint =
                    animation.nodeAnimations.find(parent.name) != animation.nodeAnimations.end();
                if (isAnimatedJoint && parentIsAnimatedJoint &&
                    distanceSquared(parentPosition, jointPosition) > 0.0001f) {
                    ++animatedHierarchyLinkCount;
                }
                pushLine(parentPosition, jointPosition, rootColor, childColor);
            }

            if (animatedHierarchyLinkCount == 0 && animatedJointPositions.size() >= 2) {
                for (size_t index = 0; index + 1 < animatedJointPositions.size(); ++index) {
                    pushLine(
                        animatedJointPositions[index],
                        animatedJointPositions[index + 1],
                        fallbackLinkColor,
                        fallbackLinkColor);
                }
            }

            Matrix4x4 baseWorld = MakeAffineMatrix(
                activeSkinnedModel->transform.scale,
                activeSkinnedModel->transform.rotate,
                activeSkinnedModel->transform.translate);
            skeletonDebugTransformData->World = baseWorld;
            skeletonDebugTransformData->WVP = Multiply(baseWorld, Multiply(viewMatrix, projMatrix));
            skeletonDebugTransformData->WorldInverseTranspose = Transpose(Inverse(baseWorld));
        } else {
            skeletonDebugVertexCount = 0;
        }
    } else {
        skeletonDebugVertexCount = 0;
    }

    if (skybox.mappedCBV != nullptr) {
        Matrix4x4 skyboxView = viewMatrix;
        skyboxView.m[3][0] = 0.0f;
        skyboxView.m[3][1] = 0.0f;
        skyboxView.m[3][2] = 0.0f;
        skybox.mappedCBV->World = MakeIdentity4x4();
        skybox.mappedCBV->WVP = Multiply(skyboxView, projMatrix);
        skybox.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }
}

void AppSceneResources::SyncRuntimeState(AppRuntimeState& runtimeState, float deltaTime) {
    activeSkinnedModelIndex = (std::min)(
        runtimeState.selectedSkinnedModelIndex,
        uint32_t(skinnedModels.size() - 1));
    runtimeState.selectedSkinnedModelIndex = activeSkinnedModelIndex;
    runtimeState.selectedVfxModelObjectIndex = (std::min)(
        runtimeState.selectedVfxModelObjectIndex,
        uint32_t(runtimeState.vfxModelObjects.size() - 1));
    if (!vfxModelLibrary.empty()) {
        const uint32_t maxModelIndex = static_cast<uint32_t>(vfxModelLibrary.size() - 1);
        for (RuntimeVfxModelObjectState& objectState : runtimeState.vfxModelObjects) {
            objectState.modelIndex = (std::min)(objectState.modelIndex, maxModelIndex);
        }
    }

    SkinnedModelInstance* activeSkinnedModel = GetActiveSkinnedModel();
    const bool updateActiveSkinned =
        activeSkinnedModel != nullptr &&
        (runtimeState.showSkinnedModel ||
            runtimeState.showSkeletonDebug ||
            runtimeState.vfx.enableSkinnedSurfaceVfx);

    if (updateActiveSkinned) {
        activeSkinnedModel->visible = runtimeState.showSkinnedModel;
        activeSkinnedModel->transform = runtimeState.skinnedModelTransform;
        UpdateSkinnedModelInstance(
            *activeSkinnedModel,
            deltaTime,
            runtimeState.playAnimatedCube,
            runtimeState.loopAnimatedCube,
            runtimeState.animatedCubeSpeed,
            runtimeState.animatedCubeTime);
    } else if (runtimeState.playAnimatedCube) {
        Animator animator{};
        animator.time = runtimeState.animatedCubeTime;
        animator.speed = runtimeState.animatedCubeSpeed;
        animator.playing = runtimeState.playAnimatedCube;
        animator.loop = runtimeState.loopAnimatedCube;
        animator.Update(deltaTime, animatedCubeAnimation.duration);
        runtimeState.animatedCubeTime = animator.time;
    } else if (activeSkinnedModel != nullptr) {
        activeSkinnedModel->visible = false;
    }

    directionalLightData = runtimeState.directionalLightData;
    directionalLightData.direction = Normalize(directionalLightData.direction);
    runtimeState.directionalLightData.direction = directionalLightData.direction;
    if (mappedLight != nullptr) {
        *mappedLight = directionalLightData;
    }

    pointLightData = runtimeState.pointLightData;
    pointLightData.position.x = sinf(deltaTime) * 2.0f;
    runtimeState.pointLightData.position = pointLightData.position;
    if (mappedPointLight != nullptr) {
        *mappedPointLight = pointLightData;
    }

    spotLight = runtimeState.spotLight;
    spotLight.direction = Normalize(spotLight.direction);
    runtimeState.spotLight.direction = spotLight.direction;
    if (mappedSpotLight != nullptr) {
        *mappedSpotLight = spotLight;
    }

    if (materialData != nullptr) {
        runtimeState.materialData.specularMode = std::clamp(runtimeState.materialData.specularMode, 0, 1);
        *materialData = runtimeState.materialData;
    }
}
