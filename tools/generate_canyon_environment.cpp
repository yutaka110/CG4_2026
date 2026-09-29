// Original low-frequency canyon reflection field, evaluated in cube directions.
// No source photographs, random noise, buildings, hard sun disc or face seams.
// Run from the repository root. See Resources/environment/README.md for build steps.
#define NOMINMAX
#include "DirectXTex.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace {
struct Color { float r, g, b; };
struct Direction { float x, y, z; };
void Check(HRESULT hr) {
    if(FAILED(hr)) throw std::runtime_error("DirectXTex operation failed");
}
float Smooth(float a, float b, float x) {
    const float t=std::clamp((x-a)/(b-a),0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}
Color Mix(Color a, Color b, float t) {
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
}
Direction CubeDirection(size_t face,float u,float v) {
    // D3D cube order: +X, -X, +Y, -Y, +Z, -Z; texture v points down.
    const Direction directions[]{{1,-v,-u},{-1,-v,u},{u,1,v},
        {u,-1,-v},{u,-v,1},{-u,-v,-1}};
    auto d=directions[face];
    const float inv=1.0f/std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    return {d.x*inv,d.y*inv,d.z*inv};
}
Color Canyon(Direction d) {
    // Linear radiance, intentionally broad and soft rather than photographic.
    const Color earth{0.16f,0.115f,0.075f};
    const Color sandstone{0.34f,0.28f,0.205f};
    const Color haze{0.46f,0.45f,0.40f};
    const Color sky{0.29f,0.38f,0.46f};
    Color c=Mix(earth,sandstone,Smooth(-0.85f,-0.05f,d.y));
    c=Mix(c,haze,Smooth(-0.10f,0.25f,d.y));
    c=Mix(c,sky,Smooth(0.22f,0.95f,d.y));
    const float light=std::pow(std::max(0.0f,d.x*0.4f+d.y*0.8f+d.z*0.4472136f),4.0f)*0.06f;
    return {c.r+light,c.g+light*0.88f,c.b+light*0.68f};
}
}

int wmain(int argc,wchar_t** argv) {
    try {
        const std::filesystem::path output=argc>1?argv[1]:L"Resources/environment/canyon_soft_256.dds";
        if(!output.parent_path().empty()) std::filesystem::create_directories(output.parent_path());
        DirectX::ScratchImage base,mips,compressed,loaded,decoded;
        Check(base.InitializeCube(DXGI_FORMAT_R32G32B32A32_FLOAT,256,256,1,1));
        for(size_t face=0;face<6;++face) {
            const auto* image=base.GetImage(0,face,0);
            for(size_t y=0;y<256;++y) {
                auto* row=reinterpret_cast<float*>(image->pixels+y*image->rowPitch);
                for(size_t x=0;x<256;++x) {
                    const auto c=Canyon(CubeDirection(face,2*(x+0.5f)/256-1,2*(y+0.5f)/256-1));
                    row[x*4]=c.r; row[x*4+1]=c.g; row[x*4+2]=c.b; row[x*4+3]=1;
                }
            }
        }
        Check(DirectX::GenerateMipMaps(base.GetImages(),base.GetImageCount(),base.GetMetadata(),
            DirectX::TEX_FILTER_BOX,0,mips));
        Check(DirectX::Compress(mips.GetImages(),mips.GetImageCount(),mips.GetMetadata(),
            DXGI_FORMAT_BC6H_UF16,DirectX::TEX_COMPRESS_DEFAULT,DirectX::TEX_THRESHOLD_DEFAULT,compressed));
        Check(DirectX::SaveToDDSFile(compressed.GetImages(),compressed.GetImageCount(),
            compressed.GetMetadata(),DirectX::DDS_FLAGS_NONE,output.c_str()));
        // Verify the exact on-disk asset and compression error, not only the source.
        Check(DirectX::LoadFromDDSFile(output.c_str(),DirectX::DDS_FLAGS_NONE,nullptr,loaded));
        const auto& metadata=loaded.GetMetadata();
        if(!metadata.IsCubemap() || metadata.width!=256 || metadata.height!=256 ||
           metadata.arraySize!=6 || metadata.mipLevels!=9 || metadata.format!=DXGI_FORMAT_BC6H_UF16)
            throw std::runtime_error("Incorrect cubemap metadata");
        Check(DirectX::Decompress(loaded.GetImages(),loaded.GetImageCount(),metadata,
            DXGI_FORMAT_R32G32B32A32_FLOAT,decoded));
        float maxError=0;
        for(size_t face=0;face<6;++face) {
            const auto* source=base.GetImage(0,face,0);
            const auto* actual=decoded.GetImage(0,face,0);
            for(size_t y=0;y<256;++y) {
                const auto* s=reinterpret_cast<const float*>(source->pixels+y*source->rowPitch);
                const auto* a=reinterpret_cast<const float*>(actual->pixels+y*actual->rowPitch);
                for(size_t x=0;x<256;++x) for(size_t c=0;c<3;++c) {
                    if(!std::isfinite(a[x*4+c])) throw std::runtime_error("Non-finite radiance");
                    maxError=std::max(maxError,std::abs(s[x*4+c]-a[x*4+c]));
                }
            }
        }
        if(maxError>0.03f) throw std::runtime_error("Excessive compression error");
        std::printf("Verified 256x256 cube, 6 faces, 9 mips, BC6H; bytes=%llu, max error=%.6f\n",
            static_cast<unsigned long long>(std::filesystem::file_size(output)),maxError);
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"%s\n",e.what()); return 1;
    }
}
