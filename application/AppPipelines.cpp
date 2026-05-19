#include "AppPipelines.h"

#include <cassert>
#include <cstdio>
#include <filesystem>

using Microsoft::WRL::ComPtr;

namespace {

std::filesystem::path GetModuleDirectory() {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path ResolveShaderPath(const std::wstring& filePath) {
    const std::filesystem::path requested(filePath);
    if (std::filesystem::exists(requested)) {
        return requested;
    }

    const std::filesystem::path bases[] = {
        std::filesystem::current_path(),
        GetModuleDirectory(),
    };

    for (const auto& base : bases) {
        std::filesystem::path probe = base;
        for (int depth = 0; depth < 6; ++depth) {
            const std::filesystem::path direct = probe / requested;
            if (std::filesystem::exists(direct)) {
                return direct;
            }

            const std::filesystem::path projectRelative = probe / L"project" / requested;
            if (std::filesystem::exists(projectRelative)) {
                return projectRelative;
            }

            if (!probe.has_parent_path()) {
                break;
            }
            probe = probe.parent_path();
        }
    }

    return requested;
}

bool FailHr(const char* stage, HRESULT hr) {
    char message[256]{};
    std::snprintf(message, sizeof(message), "[AppPipelines] %s failed. HRESULT=0x%08X\n",
                  stage, static_cast<unsigned int>(hr));
    OutputDebugStringA(message);
    return false;
}

} // namespace

ComPtr<IDxcBlob> AppPipelines::Compile_(const std::wstring& filePath, const wchar_t* profile) {
    if (!shaderCompiler_.Initialize()) {
        OutputDebugStringA("[Error] ShaderCompiler.Initialize failed\n");
        return nullptr;
    }

    const std::wstring entryPoint = L"main";
    const std::filesystem::path resolvedPath = ResolveShaderPath(filePath);
    if (!std::filesystem::exists(resolvedPath)) {
        OutputDebugStringW(L"[AppPipelines] Shader file not found: ");
        OutputDebugStringW(filePath.c_str());
        OutputDebugStringW(L"\n");
    }

    auto blob = shaderCompiler_.CompileFromFile(resolvedPath.wstring(), entryPoint, profile);
    if (!blob) {
        OutputDebugStringW(L"[AppPipelines] Shader compile failed: ");
        OutputDebugStringW(resolvedPath.wstring().c_str());
        OutputDebugStringW(L"\n");
        return nullptr;
    }
    return blob;
}

void AppPipelines::TrackShader_(const std::wstring& filePath) {
    const std::filesystem::path resolvedPath = ResolveShaderPath(filePath);
    if (std::filesystem::exists(resolvedPath)) {
        shaderWriteTimes_[filePath] = std::filesystem::last_write_time(resolvedPath);
    }
}

bool AppPipelines::ShaderChanged_(const std::wstring& filePath) const {
    const std::filesystem::path resolvedPath = ResolveShaderPath(filePath);
    if (!std::filesystem::exists(resolvedPath)) {
        return false;
    }

    const auto found = shaderWriteTimes_.find(filePath);
    if (found == shaderWriteTimes_.end()) {
        return true;
    }
    return std::filesystem::last_write_time(resolvedPath) != found->second;
}

bool AppPipelines::HotReloadIfNeeded(ID3D12Device* device) {
    if (device == nullptr) {
        return false;
    }

    const std::wstring shaders[] = {
        L"resources/Object3D.VS.hlsl",
        L"resources/SkinningObject3D.VS.hlsl",
        L"resources/Skinning.CS.hlsl",
        L"resources/Object3D.PS.hlsl",
        L"resources/Sprite.VS.hlsl",
        L"resources/Sprite.PS.hlsl",
        L"resources/Skybox.VS.hlsl",
        L"resources/Skybox.PS.hlsl",
        L"MotionDetect.CS.hlsl",
        L"resources/Particle.VS.hlsl",
        L"resources/Particle.PS.hlsl",
        L"resources/TrailMesh.VS.hlsl",
        L"resources/TrailMeshStream.VS.hlsl",
        L"resources/TrailMesh.PS.hlsl",
        L"resources/DistortionSprite.VS.hlsl",
        L"resources/DistortionSprite.PS.hlsl",
        L"resources/Ring.VS.hlsl",
        L"resources/Ring.PS.hlsl",
        L"resources/Cylinder.VS.hlsl",
        L"resources/Cylinder.PS.hlsl",
        L"resources/SkeletonDebug.VS.hlsl",
        L"resources/SkeletonDebug.PS.hlsl",
        L"resources/ParticleSim.CS.hlsl",
        L"resources/ParticleReset.CS.hlsl",
        L"resources/ParticlePoolReset.CS.hlsl",
        L"resources/ParticlePoolBegin.CS.hlsl",
        L"resources/ParticlePoolUpdate.CS.hlsl",
        L"resources/ParticleEmitterUpdate.CS.hlsl",
        L"resources/ParticleEmitterReset.CS.hlsl",
        L"resources/ParticlePoolSpawnPrepare.CS.hlsl",
        L"resources/ParticlePoolSpawn.CS.hlsl",
        L"resources/ParticlePoolArgs.CS.hlsl",
        L"resources/TrailMeshStream.CS.hlsl",
        L"resources/TrailMeshBuild.CS.hlsl",
        L"resources/FullscreenComposite.VS.hlsl",
        L"resources/FullscreenComposite.PS.hlsl",
        L"resources/BloomExtract.PS.hlsl",
        L"resources/BloomDownsample.PS.hlsl",
        L"resources/BloomUpsample.PS.hlsl",
        L"resources/BlurHorizontal.PS.hlsl",
        L"resources/BlurVertical.PS.hlsl",
        L"resources/BoxBlurHorizontal.PS.hlsl",
        L"resources/BoxBlurVertical.PS.hlsl",
        L"resources/GaussianBlurHorizontal.PS.hlsl",
        L"resources/GaussianBlurVertical.PS.hlsl",
        L"resources/DistortionComposite.PS.hlsl",
        L"resources/ToneMapping.PS.hlsl",
        L"resources/GlowComposite.PS.hlsl",
        L"resources/PrewittOutline.PS.hlsl",
        L"resources/Grayscale.PS.hlsl",
        L"resources/Vignette.PS.hlsl",
        L"resources/DebugDepthPreview.PS.hlsl",
        L"resources/DebugEmissivePreview.PS.hlsl",
    };

    for (const std::wstring& shader : shaders) {
        if (ShaderChanged_(shader)) {
            OutputDebugStringA("[AppPipelines] Shader change detected. Rebuilding pipelines.\n");
            return Initialize(device);
        }
    }
    return true;
}

bool AppPipelines::Initialize(ID3D12Device* device) {
    if (!device) return false;

    HRESULT hr = S_OK;

    // ------------------------------
    // Main RootSignature (Object3D)
    // ------------------------------
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE environmentRange = {};
    environmentRange.BaseShaderRegister = 1;
    environmentRange.NumDescriptors = 1;
    environmentRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    environmentRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE receivedRange = {};
    receivedRange.BaseShaderRegister = 4;
    receivedRange.NumDescriptors = 1;
    receivedRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    receivedRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE motionMaskRange = {};
    motionMaskRange.BaseShaderRegister = 2;
    motionMaskRange.NumDescriptors = 1;
    motionMaskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    motionMaskRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER rootParameters[10] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].DescriptorTable.pDescriptorRanges = &receivedRange;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[5].DescriptorTable.pDescriptorRanges = &motionMaskRange;

    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 2;

    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[7].Descriptor.ShaderRegister = 3;

    rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[8].Descriptor.ShaderRegister = 4;

    rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[9].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[9].DescriptorTable.pDescriptorRanges = &environmentRange;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&mainRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Main)", hr);

    D3D12_DESCRIPTOR_RANGE matrixPaletteRange = {};
    matrixPaletteRange.BaseShaderRegister = 10;
    matrixPaletteRange.NumDescriptors = 1;
    matrixPaletteRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    matrixPaletteRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER skinnedRootParameters[11] = {};
    for (uint32_t index = 0; index < _countof(rootParameters); ++index) {
        skinnedRootParameters[index] = rootParameters[index];
    }
    skinnedRootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    skinnedRootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    skinnedRootParameters[10].DescriptorTable.NumDescriptorRanges = 1;
    skinnedRootParameters[10].DescriptorTable.pDescriptorRanges = &matrixPaletteRange;

    D3D12_ROOT_SIGNATURE_DESC skinnedRootSignatureDesc = descriptionRootSignature;
    skinnedRootSignatureDesc.pParameters = skinnedRootParameters;
    skinnedRootSignatureDesc.NumParameters = _countof(skinnedRootParameters);

    ComPtr<ID3DBlob> skinnedSignatureBlob;
    ComPtr<ID3DBlob> skinnedErrorBlob;
    hr = D3D12SerializeRootSignature(
        &skinnedRootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &skinnedSignatureBlob,
        &skinnedErrorBlob);
    if (FAILED(hr)) {
        if (skinnedErrorBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(skinnedErrorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(
        0,
        skinnedSignatureBlob->GetBufferPointer(),
        skinnedSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&skinnedRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Skinned)", hr);

    // ------------------------------
    // Sprite RootSignature
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE spriteTextureRange{};
    spriteTextureRange.BaseShaderRegister = 0;
    spriteTextureRange.NumDescriptors = 1;
    spriteTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    spriteTextureRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER spriteRootParams[3] = {};
    spriteRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    spriteRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    spriteRootParams[0].Descriptor.ShaderRegister = 0;

    spriteRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    spriteRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    spriteRootParams[1].Descriptor.ShaderRegister = 0;

    spriteRootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    spriteRootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    spriteRootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    spriteRootParams[2].DescriptorTable.pDescriptorRanges = &spriteTextureRange;

    D3D12_ROOT_SIGNATURE_DESC spriteRsDesc{};
    spriteRsDesc.NumParameters = _countof(spriteRootParams);
    spriteRsDesc.pParameters = spriteRootParams;
    spriteRsDesc.NumStaticSamplers = _countof(staticSamplers);
    spriteRsDesc.pStaticSamplers = staticSamplers;
    spriteRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> spriteSigBlob;
    ComPtr<ID3DBlob> spriteErrBlob;
    hr = D3D12SerializeRootSignature(&spriteRsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &spriteSigBlob, &spriteErrBlob);
    if (FAILED(hr)) {
        if (spriteErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(spriteErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, spriteSigBlob->GetBufferPointer(), spriteSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&spriteRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Sprite)", hr);

    // ------------------------------
    // Skybox RootSignature
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE skyboxTextureRange{};
    skyboxTextureRange.BaseShaderRegister = 0;
    skyboxTextureRange.NumDescriptors = 1;
    skyboxTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    skyboxTextureRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER skyboxRootParams[2] = {};
    skyboxRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    skyboxRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    skyboxRootParams[0].Descriptor.ShaderRegister = 0;

    skyboxRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    skyboxRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    skyboxRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    skyboxRootParams[1].DescriptorTable.pDescriptorRanges = &skyboxTextureRange;

    D3D12_ROOT_SIGNATURE_DESC skyboxRsDesc{};
    skyboxRsDesc.NumParameters = _countof(skyboxRootParams);
    skyboxRsDesc.pParameters = skyboxRootParams;
    skyboxRsDesc.NumStaticSamplers = _countof(staticSamplers);
    skyboxRsDesc.pStaticSamplers = staticSamplers;
    skyboxRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> skyboxSigBlob;
    ComPtr<ID3DBlob> skyboxErrBlob;
    hr = D3D12SerializeRootSignature(&skyboxRsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &skyboxSigBlob, &skyboxErrBlob);
    if (FAILED(hr)) {
        if (skyboxErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(skyboxErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, skyboxSigBlob->GetBufferPointer(), skyboxSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&skyboxRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Skybox)", hr);

    // ------------------------------
    // Particle RootSignature
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE particleInstancingRange{};
    particleInstancingRange.BaseShaderRegister = 0;
    particleInstancingRange.NumDescriptors = 1;
    particleInstancingRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleInstancingRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleAliveListRange{};
    particleAliveListRange.BaseShaderRegister = 1;
    particleAliveListRange.NumDescriptors = 1;
    particleAliveListRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleAliveListRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleTextureRange{};
    particleTextureRange.BaseShaderRegister = 0;
    particleTextureRange.NumDescriptors = 160;
    particleTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleTextureRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleDepthRange{};
    particleDepthRange.BaseShaderRegister = 160;
    particleDepthRange.NumDescriptors = 1;
    particleDepthRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleDepthRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER particleRootParams[7] = {};
    particleRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    particleRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    particleRootParams[0].Descriptor.ShaderRegister = 0;

    particleRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    particleRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    particleRootParams[1].DescriptorTable.pDescriptorRanges = &particleInstancingRange;

    particleRootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    particleRootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    particleRootParams[2].DescriptorTable.pDescriptorRanges = &particleTextureRange;

    particleRootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    particleRootParams[3].DescriptorTable.NumDescriptorRanges = 1;
    particleRootParams[3].DescriptorTable.pDescriptorRanges = &particleDepthRange;

    particleRootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    particleRootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    particleRootParams[4].Constants.ShaderRegister = 0;
    particleRootParams[4].Constants.Num32BitValues = 4;

    particleRootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    particleRootParams[5].DescriptorTable.NumDescriptorRanges = 1;
    particleRootParams[5].DescriptorTable.pDescriptorRanges = &particleAliveListRange;

    particleRootParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    particleRootParams[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    particleRootParams[6].Constants.ShaderRegister = 1;
    particleRootParams[6].Constants.Num32BitValues = 4;

    D3D12_ROOT_SIGNATURE_DESC particleRsDesc{};
    particleRsDesc.NumParameters = _countof(particleRootParams);
    particleRsDesc.pParameters = particleRootParams;
    particleRsDesc.NumStaticSamplers = _countof(staticSamplers);
    particleRsDesc.pStaticSamplers = staticSamplers;
    particleRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> particleSigBlob;
    ComPtr<ID3DBlob> particleErrBlob;
    hr = D3D12SerializeRootSignature(&particleRsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &particleSigBlob, &particleErrBlob);
    if (FAILED(hr)) {
        if (particleErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(particleErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, particleSigBlob->GetBufferPointer(), particleSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&particleRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Particle)", hr);

    // ------------------------------
    // Ring RootSignature
    // b0: draw constants, t0: ring texture
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE ringTextureRange{};
    ringTextureRange.BaseShaderRegister = 0;
    ringTextureRange.NumDescriptors = 1;
    ringTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ringTextureRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER ringRootParams[2] = {};
    ringRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    ringRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    ringRootParams[0].Constants.ShaderRegister = 0;
    ringRootParams[0].Constants.Num32BitValues = 24;

    ringRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    ringRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    ringRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    ringRootParams[1].DescriptorTable.pDescriptorRanges = &ringTextureRange;

    D3D12_STATIC_SAMPLER_DESC ringSampler = staticSamplers[0];
    ringSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    ringSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    ringSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    D3D12_ROOT_SIGNATURE_DESC ringRsDesc{};
    ringRsDesc.NumParameters = _countof(ringRootParams);
    ringRsDesc.pParameters = ringRootParams;
    ringRsDesc.NumStaticSamplers = 1;
    ringRsDesc.pStaticSamplers = &ringSampler;
    ringRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> ringSigBlob;
    ComPtr<ID3DBlob> ringErrBlob;
    hr = D3D12SerializeRootSignature(&ringRsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &ringSigBlob, &ringErrBlob);
    if (FAILED(hr)) {
        if (ringErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(ringErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, ringSigBlob->GetBufferPointer(), ringSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&ringRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Ring)", hr);

    // ------------------------------
    // Cylinder RootSignature
    // b0: draw constants, t0: cylinder texture
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE cylinderTextureRange{};
    cylinderTextureRange.BaseShaderRegister = 0;
    cylinderTextureRange.NumDescriptors = 1;
    cylinderTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    cylinderTextureRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER cylinderRootParams[2] = {};
    cylinderRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    cylinderRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    cylinderRootParams[0].Constants.ShaderRegister = 0;
    cylinderRootParams[0].Constants.Num32BitValues = 28;

    cylinderRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    cylinderRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    cylinderRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    cylinderRootParams[1].DescriptorTable.pDescriptorRanges = &cylinderTextureRange;

    D3D12_STATIC_SAMPLER_DESC cylinderSampler = staticSamplers[0];
    cylinderSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    cylinderSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    cylinderSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    D3D12_ROOT_SIGNATURE_DESC cylinderRsDesc{};
    cylinderRsDesc.NumParameters = _countof(cylinderRootParams);
    cylinderRsDesc.pParameters = cylinderRootParams;
    cylinderRsDesc.NumStaticSamplers = 1;
    cylinderRsDesc.pStaticSamplers = &cylinderSampler;
    cylinderRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> cylinderSigBlob;
    ComPtr<ID3DBlob> cylinderErrBlob;
    hr = D3D12SerializeRootSignature(&cylinderRsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &cylinderSigBlob, &cylinderErrBlob);
    if (FAILED(hr)) {
        if (cylinderErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(cylinderErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, cylinderSigBlob->GetBufferPointer(), cylinderSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&cylinderRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Cylinder)", hr);

    // ------------------------------
    // Skeleton debug line RootSignature
    // ------------------------------
    D3D12_ROOT_PARAMETER skeletonDebugRootParams[1] = {};
    skeletonDebugRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    skeletonDebugRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    skeletonDebugRootParams[0].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC skeletonDebugRsDesc{};
    skeletonDebugRsDesc.NumParameters = _countof(skeletonDebugRootParams);
    skeletonDebugRsDesc.pParameters = skeletonDebugRootParams;
    skeletonDebugRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> skeletonDebugSigBlob;
    ComPtr<ID3DBlob> skeletonDebugErrBlob;
    hr = D3D12SerializeRootSignature(
        &skeletonDebugRsDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &skeletonDebugSigBlob,
        &skeletonDebugErrBlob);
    if (FAILED(hr)) {
        if (skeletonDebugErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(skeletonDebugErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(
        0,
        skeletonDebugSigBlob->GetBufferPointer(),
        skeletonDebugSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&skeletonDebugRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(SkeletonDebug)", hr);

    // ------------------------------
    // Compute RootSignature (MotionDetect)
    // ------------------------------
    // t4: Y, t5: UV, u0: output
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 2;
    ranges[0].BaseShaderRegister = 4;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER csParams[2] = {};
    csParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    csParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    csParams[0].DescriptorTable.NumDescriptorRanges = 1;
    csParams[0].DescriptorTable.pDescriptorRanges = &ranges[0];

    csParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    csParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    csParams[1].DescriptorTable.NumDescriptorRanges = 1;
    csParams[1].DescriptorTable.pDescriptorRanges = &ranges[1];

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.NumParameters = _countof(csParams);
    rootSigDesc.pParameters = csParams;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> csSigBlob;
    ComPtr<ID3DBlob> csErrBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &csSigBlob, &csErrBlob);
    if (FAILED(hr)) {
        if (csErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(csErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, csSigBlob->GetBufferPointer(), csSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&computeRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Compute)", hr);

    // ------------------------------
    // Skinning Compute RootSignature
    // t0: input vertices, t1: influences, t2: palette, u0: skinned vertices, b0: dispatch constants
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE skinningComputeRanges[4] = {};
    for (uint32_t i = 0; i < 3; ++i) {
        skinningComputeRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        skinningComputeRanges[i].NumDescriptors = 1;
        skinningComputeRanges[i].BaseShaderRegister = i;
        skinningComputeRanges[i].OffsetInDescriptorsFromTableStart = 0;
    }
    skinningComputeRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    skinningComputeRanges[3].NumDescriptors = 1;
    skinningComputeRanges[3].BaseShaderRegister = 0;
    skinningComputeRanges[3].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER skinningComputeParams[5] = {};
    for (uint32_t i = 0; i < 4; ++i) {
        skinningComputeParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        skinningComputeParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        skinningComputeParams[i].DescriptorTable.NumDescriptorRanges = 1;
        skinningComputeParams[i].DescriptorTable.pDescriptorRanges = &skinningComputeRanges[i];
    }
    skinningComputeParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    skinningComputeParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    skinningComputeParams[4].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC skinningComputeRootDesc{};
    skinningComputeRootDesc.NumParameters = _countof(skinningComputeParams);
    skinningComputeRootDesc.pParameters = skinningComputeParams;
    skinningComputeRootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> skinningComputeSigBlob;
    ComPtr<ID3DBlob> skinningComputeErrBlob;
    hr = D3D12SerializeRootSignature(
        &skinningComputeRootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &skinningComputeSigBlob,
        &skinningComputeErrBlob);
    if (FAILED(hr)) {
        if (skinningComputeErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(skinningComputeErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(
        0,
        skinningComputeSigBlob->GetBufferPointer(),
        skinningComputeSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&skinningComputeRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(SkinningCompute)", hr);

    // ------------------------------
    // GPU Particle Compute RootSignature
    // b0: simulation constants, u0: render particle output, u1: simulation state,
    // u2: alive list, u3: dead list, u4: pool counters, u5: indirect draw args, u6: emitter state,
    // u7: emitter spawn requests, u8: emitter spawn prefix offsets, u9: spawn dispatch args
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE particleOutputUavRange{};
    particleOutputUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleOutputUavRange.NumDescriptors = 1;
    particleOutputUavRange.BaseShaderRegister = 0;
    particleOutputUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleStateUavRange{};
    particleStateUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleStateUavRange.NumDescriptors = 1;
    particleStateUavRange.BaseShaderRegister = 1;
    particleStateUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleAliveListUavRange{};
    particleAliveListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleAliveListUavRange.NumDescriptors = 1;
    particleAliveListUavRange.BaseShaderRegister = 2;
    particleAliveListUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleDeadListUavRange{};
    particleDeadListUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleDeadListUavRange.NumDescriptors = 1;
    particleDeadListUavRange.BaseShaderRegister = 3;
    particleDeadListUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleCounterUavRange{};
    particleCounterUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleCounterUavRange.NumDescriptors = 1;
    particleCounterUavRange.BaseShaderRegister = 4;
    particleCounterUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleDrawArgsUavRange{};
    particleDrawArgsUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleDrawArgsUavRange.NumDescriptors = 1;
    particleDrawArgsUavRange.BaseShaderRegister = 5;
    particleDrawArgsUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleEmitterStateUavRange{};
    particleEmitterStateUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleEmitterStateUavRange.NumDescriptors = 1;
    particleEmitterStateUavRange.BaseShaderRegister = 6;
    particleEmitterStateUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleEmitterSpawnRequestUavRange{};
    particleEmitterSpawnRequestUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleEmitterSpawnRequestUavRange.NumDescriptors = 1;
    particleEmitterSpawnRequestUavRange.BaseShaderRegister = 7;
    particleEmitterSpawnRequestUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleEmitterSpawnOffsetUavRange{};
    particleEmitterSpawnOffsetUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleEmitterSpawnOffsetUavRange.NumDescriptors = 1;
    particleEmitterSpawnOffsetUavRange.BaseShaderRegister = 8;
    particleEmitterSpawnOffsetUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleSpawnDispatchArgsUavRange{};
    particleSpawnDispatchArgsUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleSpawnDispatchArgsUavRange.NumDescriptors = 1;
    particleSpawnDispatchArgsUavRange.BaseShaderRegister = 9;
    particleSpawnDispatchArgsUavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER gpuParticleParams[11] = {};
    gpuParticleParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    gpuParticleParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[0].Constants.ShaderRegister = 0;
    gpuParticleParams[0].Constants.Num32BitValues = 48;

    gpuParticleParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[1].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[1].DescriptorTable.pDescriptorRanges = &particleOutputUavRange;

    gpuParticleParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[2].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[2].DescriptorTable.pDescriptorRanges = &particleStateUavRange;

    gpuParticleParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[3].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[3].DescriptorTable.pDescriptorRanges = &particleAliveListUavRange;

    gpuParticleParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[4].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[4].DescriptorTable.pDescriptorRanges = &particleDeadListUavRange;

    gpuParticleParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[5].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[5].DescriptorTable.pDescriptorRanges = &particleCounterUavRange;

    gpuParticleParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[6].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[6].DescriptorTable.pDescriptorRanges = &particleDrawArgsUavRange;

    gpuParticleParams[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[7].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[7].DescriptorTable.pDescriptorRanges = &particleEmitterStateUavRange;

    gpuParticleParams[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[8].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[8].DescriptorTable.pDescriptorRanges = &particleEmitterSpawnRequestUavRange;

    gpuParticleParams[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[9].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[9].DescriptorTable.pDescriptorRanges = &particleEmitterSpawnOffsetUavRange;

    gpuParticleParams[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    gpuParticleParams[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    gpuParticleParams[10].DescriptorTable.NumDescriptorRanges = 1;
    gpuParticleParams[10].DescriptorTable.pDescriptorRanges = &particleSpawnDispatchArgsUavRange;

    D3D12_ROOT_SIGNATURE_DESC gpuParticleRootDesc{};
    gpuParticleRootDesc.NumParameters = _countof(gpuParticleParams);
    gpuParticleRootDesc.pParameters = gpuParticleParams;
    gpuParticleRootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> gpuParticleSigBlob;
    ComPtr<ID3DBlob> gpuParticleErrBlob;
    hr = D3D12SerializeRootSignature(
        &gpuParticleRootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &gpuParticleSigBlob,
        &gpuParticleErrBlob);
    if (FAILED(hr)) {
        if (gpuParticleErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(gpuParticleErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(
        0,
        gpuParticleSigBlob->GetBufferPointer(),
        gpuParticleSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&gpuParticleComputeRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(GpuParticleCompute)", hr);

    // ------------------------------
    // Trail mesh stream compute RootSignature
    // b0: stream constants, t0: position history, u0: control points, u1: segments
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE trailStreamRanges[3] = {};
    trailStreamRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    trailStreamRanges[0].NumDescriptors = 1;
    trailStreamRanges[0].BaseShaderRegister = 0;
    trailStreamRanges[0].OffsetInDescriptorsFromTableStart = 0;
    trailStreamRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    trailStreamRanges[1].NumDescriptors = 1;
    trailStreamRanges[1].BaseShaderRegister = 0;
    trailStreamRanges[1].OffsetInDescriptorsFromTableStart = 0;
    trailStreamRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    trailStreamRanges[2].NumDescriptors = 1;
    trailStreamRanges[2].BaseShaderRegister = 1;
    trailStreamRanges[2].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER trailStreamParams[4] = {};
    trailStreamParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    trailStreamParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    trailStreamParams[0].Constants.ShaderRegister = 0;
    trailStreamParams[0].Constants.Num32BitValues = 32;
    for (uint32_t i = 1; i < _countof(trailStreamParams); ++i) {
        trailStreamParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        trailStreamParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        trailStreamParams[i].DescriptorTable.NumDescriptorRanges = 1;
        trailStreamParams[i].DescriptorTable.pDescriptorRanges = &trailStreamRanges[i - 1];
    }

    D3D12_ROOT_SIGNATURE_DESC trailStreamRootDesc{};
    trailStreamRootDesc.NumParameters = _countof(trailStreamParams);
    trailStreamRootDesc.pParameters = trailStreamParams;
    trailStreamRootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> trailStreamSigBlob;
    ComPtr<ID3DBlob> trailStreamErrBlob;
    hr = D3D12SerializeRootSignature(
        &trailStreamRootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &trailStreamSigBlob,
        &trailStreamErrBlob);
    if (FAILED(hr)) return FailHr("D3D12SerializeRootSignature(TrailMeshStream)", hr);

    hr = device->CreateRootSignature(
        0,
        trailStreamSigBlob->GetBufferPointer(),
        trailStreamSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&trailMeshStreamComputeRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(TrailMeshStream)", hr);

    // ------------------------------
    // Trail mesh build compute RootSignature
    // b0: build constants, t0: control points, t1: segments, u0: vertices, u1: indices, u2: draw args
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE trailBuildRanges[5] = {};
    trailBuildRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    trailBuildRanges[0].NumDescriptors = 1;
    trailBuildRanges[0].BaseShaderRegister = 0;
    trailBuildRanges[0].OffsetInDescriptorsFromTableStart = 0;
    trailBuildRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    trailBuildRanges[1].NumDescriptors = 1;
    trailBuildRanges[1].BaseShaderRegister = 1;
    trailBuildRanges[1].OffsetInDescriptorsFromTableStart = 0;
    trailBuildRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    trailBuildRanges[2].NumDescriptors = 1;
    trailBuildRanges[2].BaseShaderRegister = 0;
    trailBuildRanges[2].OffsetInDescriptorsFromTableStart = 0;
    trailBuildRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    trailBuildRanges[3].NumDescriptors = 1;
    trailBuildRanges[3].BaseShaderRegister = 1;
    trailBuildRanges[3].OffsetInDescriptorsFromTableStart = 0;
    trailBuildRanges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    trailBuildRanges[4].NumDescriptors = 1;
    trailBuildRanges[4].BaseShaderRegister = 2;
    trailBuildRanges[4].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER trailBuildParams[6] = {};
    trailBuildParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    trailBuildParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    trailBuildParams[0].Constants.ShaderRegister = 0;
    trailBuildParams[0].Constants.Num32BitValues = 16;
    for (uint32_t i = 1; i < _countof(trailBuildParams); ++i) {
        trailBuildParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        trailBuildParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        trailBuildParams[i].DescriptorTable.NumDescriptorRanges = 1;
        trailBuildParams[i].DescriptorTable.pDescriptorRanges = &trailBuildRanges[i - 1];
    }

    D3D12_ROOT_SIGNATURE_DESC trailBuildRootDesc{};
    trailBuildRootDesc.NumParameters = _countof(trailBuildParams);
    trailBuildRootDesc.pParameters = trailBuildParams;
    trailBuildRootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> trailBuildSigBlob;
    ComPtr<ID3DBlob> trailBuildErrBlob;
    hr = D3D12SerializeRootSignature(
        &trailBuildRootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &trailBuildSigBlob,
        &trailBuildErrBlob);
    if (FAILED(hr)) return FailHr("D3D12SerializeRootSignature(TrailMeshBuild)", hr);

    hr = device->CreateRootSignature(
        0,
        trailBuildSigBlob->GetBufferPointer(),
        trailBuildSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&trailMeshBuildComputeRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(TrailMeshBuild)", hr);

    // ------------------------------
    // Full-screen Composite RootSignature
    // t0: SceneColor, t1: VfxAccumulation, t2: PostColor
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE compositeRanges[3] = {};
    for (uint32_t i = 0; i < _countof(compositeRanges); ++i) {
        compositeRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        compositeRanges[i].NumDescriptors = 1;
        compositeRanges[i].BaseShaderRegister = i;
        compositeRanges[i].OffsetInDescriptorsFromTableStart = 0;
    }

    D3D12_ROOT_PARAMETER compositeParams[4] = {};
    for (uint32_t i = 0; i < 3; ++i) {
        compositeParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        compositeParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        compositeParams[i].DescriptorTable.NumDescriptorRanges = 1;
        compositeParams[i].DescriptorTable.pDescriptorRanges = &compositeRanges[i];
    }
    compositeParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    compositeParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    compositeParams[3].Constants.ShaderRegister = 0;
    compositeParams[3].Constants.Num32BitValues = 8;

    D3D12_ROOT_SIGNATURE_DESC compositeRootDesc{};
    compositeRootDesc.NumParameters = _countof(compositeParams);
    compositeRootDesc.pParameters = compositeParams;
    compositeRootDesc.NumStaticSamplers = _countof(staticSamplers);
    compositeRootDesc.pStaticSamplers = staticSamplers;
    compositeRootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> compositeSigBlob;
    ComPtr<ID3DBlob> compositeErrBlob;
    hr = D3D12SerializeRootSignature(
        &compositeRootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &compositeSigBlob,
        &compositeErrBlob);
    if (FAILED(hr)) {
        if (compositeErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(compositeErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(
        0,
        compositeSigBlob->GetBufferPointer(),
        compositeSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&compositeRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Composite)", hr);

    // ------------------------------
    // Compile shaders
    // ------------------------------
    vs_ = Compile_(L"resources/Object3D.VS.hlsl", L"vs_6_0");
    skinnedVs_ = Compile_(L"resources/SkinningObject3D.VS.hlsl", L"vs_6_0");
    skinningCs_ = Compile_(L"resources/Skinning.CS.hlsl", L"cs_6_0");
    ps_ = Compile_(L"resources/Object3D.PS.hlsl", L"ps_6_0");
    spriteVs_ = Compile_(L"resources/Sprite.VS.hlsl", L"vs_6_0");
    spritePs_ = Compile_(L"resources/Sprite.PS.hlsl", L"ps_6_0");
    skyboxVs_ = Compile_(L"resources/Skybox.VS.hlsl", L"vs_6_0");
    skyboxPs_ = Compile_(L"resources/Skybox.PS.hlsl", L"ps_6_0");
    cs_ = Compile_(L"MotionDetect.CS.hlsl", L"cs_6_0");
    particleVs_ = Compile_(L"resources/Particle.VS.hlsl", L"vs_6_0");
    particlePs_ = Compile_(L"resources/Particle.PS.hlsl", L"ps_6_0");
    trailMeshVs_ = Compile_(L"resources/TrailMesh.VS.hlsl", L"vs_6_0");
    trailMeshStreamVs_ = Compile_(L"resources/TrailMeshStream.VS.hlsl", L"vs_6_0");
    trailMeshPs_ = Compile_(L"resources/TrailMesh.PS.hlsl", L"ps_6_0");
    distortionSpriteVs_ = Compile_(L"resources/DistortionSprite.VS.hlsl", L"vs_6_0");
    distortionSpritePs_ = Compile_(L"resources/DistortionSprite.PS.hlsl", L"ps_6_0");
    ringVs_ = Compile_(L"resources/Ring.VS.hlsl", L"vs_6_0");
    ringPs_ = Compile_(L"resources/Ring.PS.hlsl", L"ps_6_0");
    cylinderVs_ = Compile_(L"resources/Cylinder.VS.hlsl", L"vs_6_0");
    cylinderPs_ = Compile_(L"resources/Cylinder.PS.hlsl", L"ps_6_0");
    skeletonDebugVs_ = Compile_(L"resources/SkeletonDebug.VS.hlsl", L"vs_6_0");
    skeletonDebugPs_ = Compile_(L"resources/SkeletonDebug.PS.hlsl", L"ps_6_0");
    gpuParticleCs_ = Compile_(L"resources/ParticleSim.CS.hlsl", L"cs_6_0");
    gpuParticleResetCs_ = Compile_(L"resources/ParticleReset.CS.hlsl", L"cs_6_0");
    gpuParticlePoolResetCs_ = Compile_(L"resources/ParticlePoolReset.CS.hlsl", L"cs_6_0");
    gpuParticlePoolBeginCs_ = Compile_(L"resources/ParticlePoolBegin.CS.hlsl", L"cs_6_0");
    gpuParticlePoolUpdateCs_ = Compile_(L"resources/ParticlePoolUpdate.CS.hlsl", L"cs_6_0");
    gpuParticleEmitterUpdateCs_ = Compile_(L"resources/ParticleEmitterUpdate.CS.hlsl", L"cs_6_0");
    gpuParticleEmitterResetCs_ = Compile_(L"resources/ParticleEmitterReset.CS.hlsl", L"cs_6_0");
    gpuParticlePoolSpawnPrepareCs_ = Compile_(L"resources/ParticlePoolSpawnPrepare.CS.hlsl", L"cs_6_0");
    gpuParticlePoolSpawnCs_ = Compile_(L"resources/ParticlePoolSpawn.CS.hlsl", L"cs_6_0");
    gpuParticlePoolArgsCs_ = Compile_(L"resources/ParticlePoolArgs.CS.hlsl", L"cs_6_0");
    trailMeshStreamCs_ = Compile_(L"resources/TrailMeshStream.CS.hlsl", L"cs_6_0");
    trailMeshBuildCs_ = Compile_(L"resources/TrailMeshBuild.CS.hlsl", L"cs_6_0");
    compositeVs_ = Compile_(L"resources/FullscreenComposite.VS.hlsl", L"vs_6_0");
    compositePs_ = Compile_(L"resources/FullscreenComposite.PS.hlsl", L"ps_6_0");
    bloomExtractPs_ = Compile_(L"resources/BloomExtract.PS.hlsl", L"ps_6_0");
    bloomDownsamplePs_ = Compile_(L"resources/BloomDownsample.PS.hlsl", L"ps_6_0");
    bloomUpsamplePs_ = Compile_(L"resources/BloomUpsample.PS.hlsl", L"ps_6_0");
    blurHorizontalPs_ = Compile_(L"resources/BlurHorizontal.PS.hlsl", L"ps_6_0");
    blurVerticalPs_ = Compile_(L"resources/BlurVertical.PS.hlsl", L"ps_6_0");
    boxBlurHorizontalPs_ = Compile_(L"resources/BoxBlurHorizontal.PS.hlsl", L"ps_6_0");
    boxBlurVerticalPs_ = Compile_(L"resources/BoxBlurVertical.PS.hlsl", L"ps_6_0");
    gaussianBlurHorizontalPs_ = Compile_(L"resources/GaussianBlurHorizontal.PS.hlsl", L"ps_6_0");
    gaussianBlurVerticalPs_ = Compile_(L"resources/GaussianBlurVertical.PS.hlsl", L"ps_6_0");
    distortionCompositePs_ = Compile_(L"resources/DistortionComposite.PS.hlsl", L"ps_6_0");
    toneMappingPs_ = Compile_(L"resources/ToneMapping.PS.hlsl", L"ps_6_0");
    glowCompositePs_ = Compile_(L"resources/GlowComposite.PS.hlsl", L"ps_6_0");
    prewittOutlinePs_ = Compile_(L"resources/PrewittOutline.PS.hlsl", L"ps_6_0");
    grayscalePs_ = Compile_(L"resources/Grayscale.PS.hlsl", L"ps_6_0");
    vignettePs_ = Compile_(L"resources/Vignette.PS.hlsl", L"ps_6_0");
    debugDepthPreviewPs_ = Compile_(L"resources/DebugDepthPreview.PS.hlsl", L"ps_6_0");
    debugEmissivePreviewPs_ = Compile_(L"resources/DebugEmissivePreview.PS.hlsl", L"ps_6_0");

    if (!vs_ || !skinnedVs_ || !skinningCs_ || !ps_ || !spriteVs_ || !spritePs_ || !skyboxVs_ || !skyboxPs_ || !cs_ || !particleVs_ || !particlePs_ ||
        !trailMeshVs_ || !trailMeshStreamVs_ || !trailMeshPs_ || !distortionSpriteVs_ || !distortionSpritePs_ ||
        !ringVs_ || !ringPs_ || !cylinderVs_ || !cylinderPs_ || !skeletonDebugVs_ || !skeletonDebugPs_ ||
        !gpuParticleCs_ || !gpuParticleResetCs_ || !gpuParticlePoolResetCs_ || !gpuParticlePoolBeginCs_ ||
        !gpuParticlePoolUpdateCs_ || !gpuParticleEmitterUpdateCs_ || !gpuParticleEmitterResetCs_ ||
        !gpuParticlePoolSpawnPrepareCs_ ||
        !gpuParticlePoolSpawnCs_ || !gpuParticlePoolArgsCs_ ||
        !trailMeshStreamCs_ || !trailMeshBuildCs_ || !compositeVs_ || !compositePs_ || !bloomExtractPs_ ||
        !bloomDownsamplePs_ || !bloomUpsamplePs_ || !blurHorizontalPs_ || !blurVerticalPs_ ||
        !boxBlurHorizontalPs_ || !boxBlurVerticalPs_ || !gaussianBlurHorizontalPs_ || !gaussianBlurVerticalPs_ ||
        !distortionCompositePs_ ||
        !toneMappingPs_ || !glowCompositePs_ || !prewittOutlinePs_ || !grayscalePs_ || !vignettePs_ ||
        !debugDepthPreviewPs_ || !debugEmissivePreviewPs_) {
        OutputDebugStringA("[AppPipelines] Shader compilation failed.\n");
        return false;
    }

    // ------------------------------
    // InputLayout
    // ------------------------------
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendState
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Rasterizer
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Depth
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // ------------------------------
    // Main PSO (graphicsPipelineState)
    // ------------------------------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = mainRootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vs_->GetBufferPointer(), vs_->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { ps_->GetBufferPointer(), ps_->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&mainPso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Main)", hr);

    D3D12_INPUT_ELEMENT_DESC skinnedInputElements[5] = {};
    skinnedInputElements[0] = inputElementDescs[0];
    skinnedInputElements[1] = inputElementDescs[1];
    skinnedInputElements[2] = inputElementDescs[2];

    skinnedInputElements[3].SemanticName = "WEIGHT";
    skinnedInputElements[3].SemanticIndex = 0;
    skinnedInputElements[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    skinnedInputElements[3].InputSlot = 1;
    skinnedInputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    skinnedInputElements[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    skinnedInputElements[4].SemanticName = "INDEX";
    skinnedInputElements[4].SemanticIndex = 0;
    skinnedInputElements[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;
    skinnedInputElements[4].InputSlot = 1;
    skinnedInputElements[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    skinnedInputElements[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_INPUT_LAYOUT_DESC skinnedInputLayoutDesc{};
    skinnedInputLayoutDesc.pInputElementDescs = skinnedInputElements;
    skinnedInputLayoutDesc.NumElements = _countof(skinnedInputElements);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedPsoDesc = graphicsPipelineStateDesc;
    skinnedPsoDesc.pRootSignature = skinnedRootSignature_.Get();
    skinnedPsoDesc.InputLayout = skinnedInputLayoutDesc;
    skinnedPsoDesc.VS = { skinnedVs_->GetBufferPointer(), skinnedVs_->GetBufferSize() };
    skinnedPsoDesc.PS = { ps_->GetBufferPointer(), ps_->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&skinnedPsoDesc, IID_PPV_ARGS(&skinnedPso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Skinned)", hr);

    // ------------------------------
    // Skybox PSO
    // ------------------------------
    D3D12_INPUT_ELEMENT_DESC skyboxElement{};
    skyboxElement.SemanticName = "POSITION";
    skyboxElement.SemanticIndex = 0;
    skyboxElement.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    skyboxElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC skyboxInputLayout{};
    skyboxInputLayout.pInputElementDescs = &skyboxElement;
    skyboxInputLayout.NumElements = 1;

    D3D12_RASTERIZER_DESC skyboxRasterizer{};
    skyboxRasterizer.CullMode = D3D12_CULL_MODE_NONE;
    skyboxRasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    skyboxRasterizer.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC skyboxDepth{};
    skyboxDepth.DepthEnable = FALSE;
    skyboxDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    skyboxDepth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyboxDesc{};
    skyboxDesc.pRootSignature = skyboxRootSignature_.Get();
    skyboxDesc.InputLayout = skyboxInputLayout;
    skyboxDesc.VS = { skyboxVs_->GetBufferPointer(), skyboxVs_->GetBufferSize() };
    skyboxDesc.PS = { skyboxPs_->GetBufferPointer(), skyboxPs_->GetBufferSize() };
    skyboxDesc.BlendState = blendDesc;
    skyboxDesc.RasterizerState = skyboxRasterizer;
    skyboxDesc.DepthStencilState = skyboxDepth;
    skyboxDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    skyboxDesc.NumRenderTargets = 1;
    skyboxDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    skyboxDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    skyboxDesc.SampleDesc.Count = 1;
    skyboxDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device->CreateGraphicsPipelineState(&skyboxDesc, IID_PPV_ARGS(&skyboxPso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Skybox)", hr);

    // ------------------------------
    // Sprite PSO
    // ------------------------------
    D3D12_INPUT_ELEMENT_DESC spriteElements[2] = {};
    spriteElements[0].SemanticName = "POSITION";
    spriteElements[0].SemanticIndex = 0;
    spriteElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    spriteElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    spriteElements[1].SemanticName = "TEXCOORD";
    spriteElements[1].SemanticIndex = 0;
    spriteElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    spriteElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC spriteInputLayout{};
    spriteInputLayout.pInputElementDescs = spriteElements;
    spriteInputLayout.NumElements = _countof(spriteElements);

    D3D12_BLEND_DESC spriteBlend{};
    spriteBlend.RenderTarget[0].BlendEnable = TRUE;
    spriteBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    spriteBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    spriteBlend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    spriteBlend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    spriteBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    spriteBlend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    spriteBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC spriteDepth{};
    spriteDepth.DepthEnable = TRUE;
    spriteDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    spriteDepth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC spriteDesc{};
    spriteDesc.pRootSignature = spriteRootSignature_.Get();
    spriteDesc.InputLayout = spriteInputLayout;
    spriteDesc.VS = { spriteVs_->GetBufferPointer(), spriteVs_->GetBufferSize() };
    spriteDesc.PS = { spritePs_->GetBufferPointer(), spritePs_->GetBufferSize() };
    spriteDesc.BlendState = spriteBlend;
    spriteDesc.RasterizerState = rasterizerDesc;
    spriteDesc.DepthStencilState = spriteDepth;
    spriteDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    spriteDesc.NumRenderTargets = 1;
    spriteDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    spriteDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    spriteDesc.SampleDesc.Count = 1;
    spriteDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device->CreateGraphicsPipelineState(&spriteDesc, IID_PPV_ARGS(&spritePso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Sprite)", hr);

    // ------------------------------
    // Compute PSO
    // ------------------------------
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc{};
    computePsoDesc.pRootSignature = computeRootSignature_.Get();
    computePsoDesc.CS = { cs_->GetBufferPointer(), cs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&computePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(MotionDetect)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC skinningComputeDesc{};
    skinningComputeDesc.pRootSignature = skinningComputeRootSignature_.Get();
    skinningComputeDesc.CS = { skinningCs_->GetBufferPointer(), skinningCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&skinningComputeDesc, IID_PPV_ARGS(&skinningComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(SkinningCompute)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticleComputeDesc{};
    gpuParticleComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticleComputeDesc.CS = { gpuParticleCs_->GetBufferPointer(), gpuParticleCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticleComputeDesc, IID_PPV_ARGS(&gpuParticleComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticle)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticleResetComputeDesc{};
    gpuParticleResetComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticleResetComputeDesc.CS = { gpuParticleResetCs_->GetBufferPointer(), gpuParticleResetCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticleResetComputeDesc, IID_PPV_ARGS(&gpuParticleResetComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticleReset)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticlePoolResetComputeDesc{};
    gpuParticlePoolResetComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticlePoolResetComputeDesc.CS = { gpuParticlePoolResetCs_->GetBufferPointer(), gpuParticlePoolResetCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticlePoolResetComputeDesc, IID_PPV_ARGS(&gpuParticlePoolResetComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticlePoolReset)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticlePoolBeginComputeDesc{};
    gpuParticlePoolBeginComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticlePoolBeginComputeDesc.CS = { gpuParticlePoolBeginCs_->GetBufferPointer(), gpuParticlePoolBeginCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticlePoolBeginComputeDesc, IID_PPV_ARGS(&gpuParticlePoolBeginComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticlePoolBegin)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticlePoolUpdateComputeDesc{};
    gpuParticlePoolUpdateComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticlePoolUpdateComputeDesc.CS = { gpuParticlePoolUpdateCs_->GetBufferPointer(), gpuParticlePoolUpdateCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticlePoolUpdateComputeDesc, IID_PPV_ARGS(&gpuParticlePoolUpdateComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticlePoolUpdate)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticleEmitterUpdateComputeDesc{};
    gpuParticleEmitterUpdateComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticleEmitterUpdateComputeDesc.CS = { gpuParticleEmitterUpdateCs_->GetBufferPointer(), gpuParticleEmitterUpdateCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticleEmitterUpdateComputeDesc, IID_PPV_ARGS(&gpuParticleEmitterUpdateComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticleEmitterUpdate)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticleEmitterResetComputeDesc{};
    gpuParticleEmitterResetComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticleEmitterResetComputeDesc.CS = { gpuParticleEmitterResetCs_->GetBufferPointer(), gpuParticleEmitterResetCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticleEmitterResetComputeDesc, IID_PPV_ARGS(&gpuParticleEmitterResetComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticleEmitterReset)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticlePoolSpawnPrepareComputeDesc{};
    gpuParticlePoolSpawnPrepareComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticlePoolSpawnPrepareComputeDesc.CS = { gpuParticlePoolSpawnPrepareCs_->GetBufferPointer(), gpuParticlePoolSpawnPrepareCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticlePoolSpawnPrepareComputeDesc, IID_PPV_ARGS(&gpuParticlePoolSpawnPrepareComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticlePoolSpawnPrepare)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticlePoolSpawnComputeDesc{};
    gpuParticlePoolSpawnComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticlePoolSpawnComputeDesc.CS = { gpuParticlePoolSpawnCs_->GetBufferPointer(), gpuParticlePoolSpawnCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticlePoolSpawnComputeDesc, IID_PPV_ARGS(&gpuParticlePoolSpawnComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticlePoolSpawn)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC gpuParticlePoolArgsComputeDesc{};
    gpuParticlePoolArgsComputeDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    gpuParticlePoolArgsComputeDesc.CS = { gpuParticlePoolArgsCs_->GetBufferPointer(), gpuParticlePoolArgsCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&gpuParticlePoolArgsComputeDesc, IID_PPV_ARGS(&gpuParticlePoolArgsComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(GpuParticlePoolArgs)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC trailMeshStreamComputeDesc{};
    trailMeshStreamComputeDesc.pRootSignature = trailMeshStreamComputeRootSignature_.Get();
    trailMeshStreamComputeDesc.CS = { trailMeshStreamCs_->GetBufferPointer(), trailMeshStreamCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&trailMeshStreamComputeDesc, IID_PPV_ARGS(&trailMeshStreamComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(TrailMeshStream)", hr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC trailMeshBuildComputeDesc{};
    trailMeshBuildComputeDesc.pRootSignature = trailMeshBuildComputeRootSignature_.Get();
    trailMeshBuildComputeDesc.CS = { trailMeshBuildCs_->GetBufferPointer(), trailMeshBuildCs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&trailMeshBuildComputeDesc, IID_PPV_ARGS(&trailMeshBuildComputePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(TrailMeshBuild)", hr);

    D3D12_DEPTH_STENCIL_DESC compositeDepth{};
    compositeDepth.DepthEnable = FALSE;

    D3D12_RASTERIZER_DESC compositeRaster{};
    compositeRaster.CullMode = D3D12_CULL_MODE_NONE;
    compositeRaster.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_BLEND_DESC compositeBlend{};
    compositeBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC compositeDesc{};
    compositeDesc.pRootSignature = compositeRootSignature_.Get();
    compositeDesc.VS = { compositeVs_->GetBufferPointer(), compositeVs_->GetBufferSize() };
    compositeDesc.PS = { compositePs_->GetBufferPointer(), compositePs_->GetBufferSize() };
    compositeDesc.BlendState = compositeBlend;
    compositeDesc.RasterizerState = compositeRaster;
    compositeDesc.DepthStencilState = compositeDepth;
    compositeDesc.NumRenderTargets = 1;
    compositeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    compositeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    compositeDesc.SampleDesc.Count = 1;
    compositeDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    hr = device->CreateGraphicsPipelineState(&compositeDesc, IID_PPV_ARGS(&compositePso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Composite)", hr);

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { bloomExtractPs_->GetBufferPointer(), bloomExtractPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&bloomExtractPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(BloomExtract)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { bloomDownsamplePs_->GetBufferPointer(), bloomDownsamplePs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&bloomDownsamplePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(BloomDownsample)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { bloomUpsamplePs_->GetBufferPointer(), bloomUpsamplePs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&bloomUpsamplePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(BloomUpsample)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { blurHorizontalPs_->GetBufferPointer(), blurHorizontalPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&blurHorizontalPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(BlurHorizontal)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { blurVerticalPs_->GetBufferPointer(), blurVerticalPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&blurVerticalPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(BlurVertical)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { boxBlurHorizontalPs_->GetBufferPointer(), boxBlurHorizontalPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&boxBlurHorizontalPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(BoxBlurHorizontal)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { boxBlurVerticalPs_->GetBufferPointer(), boxBlurVerticalPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&boxBlurVerticalPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(BoxBlurVertical)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { gaussianBlurHorizontalPs_->GetBufferPointer(), gaussianBlurHorizontalPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&gaussianBlurHorizontalPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(GaussianBlurHorizontal)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { gaussianBlurVerticalPs_->GetBufferPointer(), gaussianBlurVerticalPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&gaussianBlurVerticalPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(GaussianBlurVertical)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { distortionCompositePs_->GetBufferPointer(), distortionCompositePs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&distortionCompositePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(DistortionComposite)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { toneMappingPs_->GetBufferPointer(), toneMappingPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&toneMappingPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(ToneMapping)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { glowCompositePs_->GetBufferPointer(), glowCompositePs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&glowCompositePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(GlowComposite)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { prewittOutlinePs_->GetBufferPointer(), prewittOutlinePs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&prewittOutlinePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(PrewittOutline)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { grayscalePs_->GetBufferPointer(), grayscalePs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&grayscalePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Grayscale)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { vignettePs_->GetBufferPointer(), vignettePs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&vignettePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Vignette)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { debugDepthPreviewPs_->GetBufferPointer(), debugDepthPreviewPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&debugDepthPreviewPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(DebugDepthPreview)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = compositeDesc;
        d.PS = { debugEmissivePreviewPs_->GetBufferPointer(), debugEmissivePreviewPs_->GetBufferSize() };
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&debugEmissivePreviewPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(DebugEmissivePreview)", hr);
    }

    // ------------------------------
    // Particle PSOs
    // ------------------------------
    auto MakeParticleOpaqueBlend = []() {
        D3D12_BLEND_DESC d{};
        d.AlphaToCoverageEnable = FALSE;
        d.IndependentBlendEnable = FALSE;
        auto& rt = d.RenderTarget[0];
        rt.BlendEnable = FALSE;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        return d;
    };

    auto MakeParticleAlphaBlend = []() {
        D3D12_BLEND_DESC d{};
        d.AlphaToCoverageEnable = FALSE;
        d.IndependentBlendEnable = FALSE;
        auto& rt = d.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        return d;
    };

    // Particle quad uses VertexData: position(float4), texcoord(float2), normal(float3).
    D3D12_INPUT_ELEMENT_DESC particleElements[3] = {};
    particleElements[0].SemanticName = "POSITION";
    particleElements[0].SemanticIndex = 0;
    particleElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    particleElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    particleElements[1].SemanticName = "TEXCOORD";
    particleElements[1].SemanticIndex = 0;
    particleElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    particleElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    particleElements[2].SemanticName = "NORMAL";
    particleElements[2].SemanticIndex = 0;
    particleElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    particleElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC particleInputLayout{};
    particleInputLayout.pInputElementDescs = particleElements;
    particleInputLayout.NumElements = _countof(particleElements);

    // Particle rasterizer: no cull is common for billboard
    D3D12_RASTERIZER_DESC particleRaster{};
    particleRaster.CullMode = D3D12_CULL_MODE_NONE;
    particleRaster.FillMode = D3D12_FILL_MODE_SOLID;

    // Particle depth: enable but no write for transparent
    D3D12_DEPTH_STENCIL_DESC particleDepth{};
    particleDepth.DepthEnable = TRUE;
    particleDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    particleDepth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // Base particle desc
    D3D12_GRAPHICS_PIPELINE_STATE_DESC particleDesc{};
    particleDesc.pRootSignature = particleRootSignature_.Get();
    particleDesc.InputLayout = particleInputLayout;
    particleDesc.VS = { particleVs_->GetBufferPointer(), particleVs_->GetBufferSize() };
    particleDesc.PS = { particlePs_->GetBufferPointer(), particlePs_->GetBufferSize() };
    particleDesc.RasterizerState = particleRaster;
    particleDesc.DepthStencilState = particleDepth;
    particleDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    particleDesc.NumRenderTargets = 1;
    particleDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    particleDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    particleDesc.SampleDesc.Count = 1;
    particleDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // Legacy: particlePso_ (equivalent to particlePipelineState)
    particleDesc.BlendState = MakeParticleAlphaBlend();
    hr = device->CreateGraphicsPipelineState(&particleDesc, IID_PPV_ARGS(&particlePso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Particle)", hr);

    // Opaque
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.BlendState = MakeParticleOpaqueBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&particleOpaquePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(ParticleOpaque)", hr);
    }

    // Alpha
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.BlendState = MakeParticleAlphaBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&particleAlphaPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(ParticleAlpha)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.VS = { trailMeshVs_->GetBufferPointer(), trailMeshVs_->GetBufferSize() };
        d.PS = { trailMeshPs_->GetBufferPointer(), trailMeshPs_->GetBufferSize() };
        d.BlendState = MakeParticleAlphaBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&trailMeshPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(TrailMesh)", hr);
    }

    {
        D3D12_INPUT_ELEMENT_DESC trailStreamElements[3] = {};
        trailStreamElements[0].SemanticName = "POSITION";
        trailStreamElements[0].SemanticIndex = 0;
        trailStreamElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        trailStreamElements[0].AlignedByteOffset = 0;
        trailStreamElements[1].SemanticName = "COLOR";
        trailStreamElements[1].SemanticIndex = 0;
        trailStreamElements[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        trailStreamElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        trailStreamElements[2].SemanticName = "TEXCOORD";
        trailStreamElements[2].SemanticIndex = 0;
        trailStreamElements[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        trailStreamElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

        D3D12_INPUT_LAYOUT_DESC trailStreamInputLayout{};
        trailStreamInputLayout.pInputElementDescs = trailStreamElements;
        trailStreamInputLayout.NumElements = _countof(trailStreamElements);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.InputLayout = trailStreamInputLayout;
        d.VS = { trailMeshStreamVs_->GetBufferPointer(), trailMeshStreamVs_->GetBufferSize() };
        d.PS = { trailMeshPs_->GetBufferPointer(), trailMeshPs_->GetBufferSize() };
        d.BlendState = MakeParticleAlphaBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&trailMeshStreamPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(TrailMeshStream)", hr);
    }

    {
        D3D12_BLEND_DESC distortionBlend{};
        distortionBlend.AlphaToCoverageEnable = FALSE;
        distortionBlend.IndependentBlendEnable = FALSE;
        auto& rt = distortionBlend.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.VS = { distortionSpriteVs_->GetBufferPointer(), distortionSpriteVs_->GetBufferSize() };
        d.PS = { distortionSpritePs_->GetBufferPointer(), distortionSpritePs_->GetBufferSize() };
        d.BlendState = distortionBlend;
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&distortionSpritePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(DistortionSprite)", hr);
    }

    {
        D3D12_BLEND_DESC ringBlend{};
        ringBlend.AlphaToCoverageEnable = FALSE;
        ringBlend.IndependentBlendEnable = FALSE;
        auto& rt = ringBlend.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_ONE;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ONE;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.pRootSignature = ringRootSignature_.Get();
        d.VS = { ringVs_->GetBufferPointer(), ringVs_->GetBufferSize() };
        d.PS = { ringPs_->GetBufferPointer(), ringPs_->GetBufferSize() };
        d.BlendState = ringBlend;
        d.RasterizerState = particleRaster;
        d.DepthStencilState = particleDepth;
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&ringPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Ring)", hr);
    }

    {
        D3D12_BLEND_DESC cylinderBlend{};
        cylinderBlend.AlphaToCoverageEnable = FALSE;
        cylinderBlend.IndependentBlendEnable = FALSE;
        auto& rt = cylinderBlend.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_ONE;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ONE;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.pRootSignature = cylinderRootSignature_.Get();
        d.VS = { cylinderVs_->GetBufferPointer(), cylinderVs_->GetBufferSize() };
        d.PS = { cylinderPs_->GetBufferPointer(), cylinderPs_->GetBufferSize() };
        d.BlendState = cylinderBlend;
        d.RasterizerState = particleRaster;
        d.DepthStencilState = particleDepth;
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&cylinderPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Cylinder)", hr);
    }

    {
        D3D12_INPUT_ELEMENT_DESC skeletonLineElements[2] = {};
        skeletonLineElements[0].SemanticName = "POSITION";
        skeletonLineElements[0].SemanticIndex = 0;
        skeletonLineElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        skeletonLineElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        skeletonLineElements[1].SemanticName = "COLOR";
        skeletonLineElements[1].SemanticIndex = 0;
        skeletonLineElements[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        skeletonLineElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

        D3D12_INPUT_LAYOUT_DESC skeletonLineLayout{};
        skeletonLineLayout.pInputElementDescs = skeletonLineElements;
        skeletonLineLayout.NumElements = _countof(skeletonLineElements);

        D3D12_DEPTH_STENCIL_DESC skeletonLineDepth{};
        skeletonLineDepth.DepthEnable = FALSE;
        skeletonLineDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        skeletonLineDepth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        D3D12_RASTERIZER_DESC skeletonLineRaster{};
        skeletonLineRaster.CullMode = D3D12_CULL_MODE_NONE;
        skeletonLineRaster.FillMode = D3D12_FILL_MODE_SOLID;
        skeletonLineRaster.DepthClipEnable = TRUE;

        D3D12_BLEND_DESC skeletonLineBlend{};
        skeletonLineBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
        d.pRootSignature = skeletonDebugRootSignature_.Get();
        d.InputLayout = skeletonLineLayout;
        d.VS = { skeletonDebugVs_->GetBufferPointer(), skeletonDebugVs_->GetBufferSize() };
        d.PS = { skeletonDebugPs_->GetBufferPointer(), skeletonDebugPs_->GetBufferSize() };
        d.BlendState = skeletonLineBlend;
        d.RasterizerState = skeletonLineRaster;
        d.DepthStencilState = skeletonLineDepth;
        d.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        d.NumRenderTargets = 1;
        d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        d.SampleDesc.Count = 1;
        d.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&skeletonDebugPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(SkeletonDebug)", hr);
    }

    // ------------------------------
    // Main Opaque/Alpha variants (for sprite or UI etc)
    // These were in AppMain as psoOpaque/psoAlpha using mainRootSignature.
    // We create them as variants of main PSO by only changing BlendState.
    // ------------------------------
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = graphicsPipelineStateDesc;
        auto MakeOpaqueBlend = []() {
            D3D12_BLEND_DESC bd{};
            bd.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return bd;
        };
        d.BlendState = MakeOpaqueBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&mainOpaquePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(MainOpaque)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = graphicsPipelineStateDesc;
        D3D12_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        d.BlendState = bd;
        d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&mainAlphaPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(MainAlpha)", hr);
    }

    const std::wstring shaders[] = {
        L"resources/Object3D.VS.hlsl",
        L"resources/SkinningObject3D.VS.hlsl",
        L"resources/Skinning.CS.hlsl",
        L"resources/Object3D.PS.hlsl",
        L"resources/Sprite.VS.hlsl",
        L"resources/Sprite.PS.hlsl",
        L"resources/Skybox.VS.hlsl",
        L"resources/Skybox.PS.hlsl",
        L"MotionDetect.CS.hlsl",
        L"resources/Particle.VS.hlsl",
        L"resources/Particle.PS.hlsl",
        L"resources/TrailMesh.VS.hlsl",
        L"resources/TrailMeshStream.VS.hlsl",
        L"resources/TrailMesh.PS.hlsl",
        L"resources/DistortionSprite.VS.hlsl",
        L"resources/DistortionSprite.PS.hlsl",
        L"resources/Ring.VS.hlsl",
        L"resources/Ring.PS.hlsl",
        L"resources/Cylinder.VS.hlsl",
        L"resources/Cylinder.PS.hlsl",
        L"resources/SkeletonDebug.VS.hlsl",
        L"resources/SkeletonDebug.PS.hlsl",
        L"resources/ParticleSim.CS.hlsl",
        L"resources/ParticleReset.CS.hlsl",
        L"resources/ParticlePoolReset.CS.hlsl",
        L"resources/ParticlePoolBegin.CS.hlsl",
        L"resources/ParticlePoolUpdate.CS.hlsl",
        L"resources/ParticleEmitterUpdate.CS.hlsl",
        L"resources/ParticleEmitterReset.CS.hlsl",
        L"resources/ParticlePoolSpawnPrepare.CS.hlsl",
        L"resources/ParticlePoolSpawn.CS.hlsl",
        L"resources/ParticlePoolArgs.CS.hlsl",
        L"resources/TrailMeshStream.CS.hlsl",
        L"resources/TrailMeshBuild.CS.hlsl",
        L"resources/FullscreenComposite.VS.hlsl",
        L"resources/FullscreenComposite.PS.hlsl",
        L"resources/BloomExtract.PS.hlsl",
        L"resources/BloomDownsample.PS.hlsl",
        L"resources/BloomUpsample.PS.hlsl",
        L"resources/BlurHorizontal.PS.hlsl",
        L"resources/BlurVertical.PS.hlsl",
        L"resources/BoxBlurHorizontal.PS.hlsl",
        L"resources/BoxBlurVertical.PS.hlsl",
        L"resources/GaussianBlurHorizontal.PS.hlsl",
        L"resources/GaussianBlurVertical.PS.hlsl",
        L"resources/DistortionComposite.PS.hlsl",
        L"resources/ToneMapping.PS.hlsl",
        L"resources/GlowComposite.PS.hlsl",
        L"resources/PrewittOutline.PS.hlsl",
        L"resources/Grayscale.PS.hlsl",
        L"resources/Vignette.PS.hlsl",
        L"resources/DebugDepthPreview.PS.hlsl",
        L"resources/DebugEmissivePreview.PS.hlsl",
    };
    shaderWriteTimes_.clear();
    for (const std::wstring& shader : shaders) {
        TrackShader_(shader);
    }

    return true;
}
