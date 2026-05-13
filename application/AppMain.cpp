#define _WINSOCKAPI_
#define _WIN32_WINNT 0x0602 // Windows 8
#define ANCHOR_TEST

#include "camera/debugCamera.h"
#include "utils/math/MathUtils.h"
#include"utils/math/Vector.h"
#include "../../externals/DirectXTex/DirectXTex.h"
#include "../../externals/DirectXTex/d3dx12.h"
#include "../../externals/imgui/imgui.h"
#include "../../externals/imgui/imgui_impl_dx12.h"
#include "../../externals/imgui/imgui_impl_win32.h"
#include "wrl.h"
#include <Windows.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <codecvt>
#include <cstdint>
#include <cstdio>
#include <d3d12.h>
#include <dbghelp.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <format>
#include <fstream>
#include <locale>
#include <mutex>
#include <numbers>
#include <queue>
#include <sstream>
#include <string>
#include <strsafe.h>
#include <unordered_map>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <mferror.h>
#include <mfobjects.h>
#include <mftransform.h>
#include <comdef.h>
#include <map> 

#include"../TextureHelper.h"
#include "core/Device.h"
#include "graphics/SwapChain.h"
#include "core/CommandListPool.h"
#include "core/DescriptorHeap.h"
#include "graphics/RenderContext.h"
#include"utils/dx12/BufferHelper.h"
#include <random>
#include "ModelLoaderAssimp.h"
#include "AppAudio.h"
#include "AppBootstrap.h"
#include "AppImGuiLayer.h"
#include "AppFrameRenderer.h"
#include "AppPipelines.h"
#include "AppParticleSystem.h"
#include "AppRenderResources.h"
#include "AppRunLoop.h"
#include "AppRuntimeConfig.h"
#include "AppRuntimeState.h"
#include "AppRuntimeUtils.h"
#include "AppSceneResources.h"
#include "EngineContext.h"

using namespace DirectX;

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
	const std::string& filename);

ModelData LoadObjFile(const std::string& directoryPath,
	const std::string& filename) {
	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);

		s >> identifier;


		if (identifier == "v") {
			Vector4 position;

			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= -1.0f;
			positions.push_back(position);

		}
		else if (identifier == "vt") {
			Vector2 texcoord;

			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);

		}
		else if (identifier == "vn") {
			Vector3 normal;

			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);

		}
		else if (identifier == "f") {

			VertexData triangle[3];

			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;

				std::istringstream w(vertexDefinition);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; ++element) {
					std::string index;

					std::getline(w, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				triangle[faceVertex] = { position, texcoord, normal };
			}

			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);

		}
		else if (identifier == "mtllib") {
			std::string materialFilename;
			s >> materialFilename;

			modelData.material =
				LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	return modelData;
}

//uint32_t numInstance = 0;








MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
	const std::string& filename) {

	MaterialData materialData;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);

	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;

			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

//ComPtr<ID3D12Resource> CreateBufferResource(ComPtr<ID3D12Device> device,
//	size_t sizeInBytes);

//void Log(const std::wstring& message) { OutputDebugStringW(message.c_str()); }
//
//void Log(std::ostream& os, const std::string& message) {
//	os << message << std::endl;
//	OutputDebugStringA(message.c_str());
//}
//
//void Log(const char* message) { OutputDebugStringA(message); }
//
//void Log(const std::string& message) { OutputDebugStringA(message.c_str()); }

//std::string ConvertString(const std::wstring& wstr) {
//
//	if (wstr.empty())
//		return "";
//	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
//		nullptr, nullptr);
//	std::string result(sizeNeeded, 0);
//	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], sizeNeeded,
//		nullptr, nullptr);
//
//	result.pop_back();
//	return result;
//}

//std::wstring ConvertString(const std::string& str) {
//
//	if (str.empty()) {
//		return L"";
//	}
//
//	int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
//	std::wstring result(sizeNeeded, 0);
//	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], sizeNeeded);
//	return result;
//}

//IDxcBlob* CompileShader(
//	const std::wstring& filePath,
//	const wchar_t* profile,
//	IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler,
//	IDxcIncludeHandler* includeHandler) {
//	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n",
//		filePath, profile)));
//
//	IDxcBlobEncoding* shaderSource = nullptr;
//	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
//
//	assert(SUCCEEDED(hr));
//
//	DxcBuffer shaderSourceBuffer;
//	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
//	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
//
//	LPCWSTR arguments[] = {
//		L"-E",
//		L"-T",
//		L"-Zi",
//	};
//
//	IDxcResult* shaderResult = nullptr;
//	hr = dxcCompiler->Compile(
//	);
//
//	assert(SUCCEEDED(hr));
//
//	IDxcBlobUtf8* shaderError = nullptr;
//	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
//
//	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
//		Log(shaderError->GetStringPointer());
//		assert(false);
//	}
//
//	IDxcBlob* shaderBlob = nullptr;
//	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob),
//		nullptr);
//	assert(SUCCEEDED(hr));
//
//	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n",
//		filePath, profile)));
//
//	shaderSource->Release();
//	shaderResult->Release();
//
//	return shaderBlob;
//}


//**************************
////**************************
//static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
//	SYSTEMTIME time;
//	GetLocalTime(&time);
//	wchar_t filePath[MAX_PATH] = { 0 };
//	CreateDirectory(L"./Dumps", nullptr);
//	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
//		time.wYear, time.wMonth, time.wDay, time.wHour,
//		time.wMinute);
//	HANDLE dumpFileHandle =
//		CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
//			FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
//
//	DWORD processId = GetCurrentProcessId();
//	DWORD threadId = GetCurrentThreadId();
//
//	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
//	minidumpInformation.ThreadId = threadId;
//	minidumpInformation.ExceptionPointers = exception;
//	minidumpInformation.ClientPointers = TRUE;
//
//	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
//		MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
//
//	return EXCEPTION_EXECUTE_HANDLER;
//}

/// <summary>
/// </summary>
/// <param name="device">ID3D12Device*</param>
/// <returns>ID3D12Resource*</returns>
//ComPtr<ID3D12Resource> CreateBufferResource(ComPtr<ID3D12Device> device,
//	size_t sizeInBytes) {
//	D3D12_HEAP_PROPERTIES heapProps{};
//	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
//
//	D3D12_RESOURCE_DESC resourceDesc{};
//	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
//	resourceDesc.Width = sizeInBytes;
//	resourceDesc.Height = 1;
//	resourceDesc.DepthOrArraySize = 1;
//	resourceDesc.MipLevels = 1;
//	resourceDesc.SampleDesc.Count = 1;
//	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
//
//	ComPtr<ID3D12Resource> resource = nullptr;
//	HRESULT hr = device->CreateCommittedResource(
//		&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
//		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
//	assert(SUCCEEDED(hr));
//
//	return resource;
//}


//
struct CpuTransferCtx {
	ComPtr<ID3D11Texture2D> stagingTex;
	D3D11_TEXTURE2D_DESC    stagingDesc{};

	D3D12_RESOURCE_DESC     lastDstDesc{};

	static constexpr UINT   kRing = 3;
	ComPtr<ID3D12Resource>  upload[kRing];
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{}; // CopyableFootprints
	UINT                    numRows = 0;
	UINT64                  rowSize = 0;
	UINT64                  totalBytes = 0;
	UINT64                  fenceValue[kRing]{};
	ComPtr<ID3D12Fence>     fence;
	HANDLE                  fenceEvent = nullptr;
	UINT64                  nextFence = 1;
	UINT                    ringIndex = 0;

	ComPtr<ID3D12CommandAllocator> cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> cmdList;
};


struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() {

		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};




#include "AppMain.h"


bool AppMain::Initialize(HINSTANCE hInstance) {
	hInstance_ = hInstance;
	return true;
}

void AppMain::Finalize() {
}

int AppMain::Run() {
	D3DResourceLeakChecker leakCheck;

	AppBootstrap bootstrap;
	if (!bootstrap.Initialize(hInstance_)) {
		return -1;
	}

	const HWND hwnd = bootstrap.Handle();
	const uint32_t windowWidth = bootstrap.Width();
	const uint32_t windowHeight = bootstrap.Height();

	HRESULT hr = S_OK;

	////**************************
	////**************************

	EngineContext engineContext;
	if (!engineContext.Initialize(hwnd, windowWidth, windowHeight, /*enableDebugLayer=*/true)) {
		return -1;
	}

	core::Device& dev = engineContext.GetDevice();

	////**************************
	////**************************

	Microsoft::WRL::ComPtr < ID3D12Device> device = dev.GetDevice();
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = dev.GetCommandQueue();
	Microsoft::WRL::ComPtr<ID3D12Fence>        fence = dev.GetFence();

	//***************************
	//***************************
	// RunSenderTest();

	//***************************
	//***************************

	auto& swapChain = engineContext.GetSwapChain();
	auto& clPool = engineContext.GetCommandListPool();



	//ComPtr<IDXGISwapChain4> swapChain = nullptr;
	//DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	//swapChainDesc.Width = wd.width;
	//swapChainDesc.Height = wd.height;
	//swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//swapChainDesc.SampleDesc.Count = 1;
	//swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//swapChainDesc.BufferCount = 2;

	//swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	//hr = dxgiFactory->CreateSwapChainForHwnd(
	//	commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr,
	//	reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));

	//assert(SUCCEEDED(hr));
	// ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	// D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	// rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	// rtvDescriptorHeapDesc.NumDescriptors = 2;
	// hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc,
	// IID_PPV_ARGS(&rtvDescriptorHeap));

	auto& heaps = engineContext.GetHeaps();
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = heaps.srv.GetHeap();
	heaps.srv.Reserve(32);




	//device->CreateDepthStencilView(
	//	depthStencilResource.Get(), &dsvDesc,
	//	dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	//**************************
	//**************************
	uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	assert(SUCCEEDED(hr));


	constexpr DXGI_FORMAT kRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	uint64_t fenceValue = engineContext.GetFenceValue();
	HANDLE fenceEvent = engineContext.GetFenceEvent();

	//IDxcUtils* dxcUtils = nullptr;
	//IDxcCompiler3* dxcCompiler = nullptr;
	//hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	//assert(SUCCEEDED(hr));

	//hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	//assert(SUCCEEDED(hr));

	//IDxcIncludeHandler* includeHandler = nullptr;
	//hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);

	//assert(SUCCEEDED(hr));


	ComPtr<ID3D12Resource> wvpResource =
		CreateBufferResource(device, sizeof(Matrix4x4));

	Matrix4x4* wvpData = nullptr;

	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

	*wvpData = MakeIdentity4x4();





	//-------------------------------------------------------
	//-------------------------------------------------------



	//-------------------------------------------------------
	//-------------------------------------------------------


	//for (uint32_t i = 0; i < kNumInstance; ++i) {

	//	

	//	particle[i].transform.scale = { 0.6f, 0.6f, 0.6f };

	//	particle[i].transform.rotate = { 0.0f, 0.0f, 0.0f };

	//	particle[i].velocity = { 0.0f, 1.0f, 0.0f };

	//	for (uint32_t i = 0; i < kNumInstance; ++i) {
	//		particle[i] = MakeNewParticle(randomEngine);
	//	}


	//	particle[i].transform.translate = {
	//	0.0f-0.01f*i,
	//	0.0f+0.01f*i,
	//	};
	//}


	//**************************
	//**************************

	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC vertexResourceDesc{};

	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width =
		sizeof(VertexData) * 3;

	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> vertexResource =
		CreateBufferResource(device, sizeof(VertexData) * 6);
	hr = device->CreateCommittedResource(
		&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));

	ComPtr<ID3D12Resource> indexResourceSprite =
		CreateBufferResource(device, sizeof(uint32_t) * 6);

	const UINT stackCount = 16;
	const UINT sliceCount = 32;

	ComPtr<ID3D12Resource> vertexResourceSprite =
		CreateBufferResource(device, sizeof(VertexData) * 6);

//	DirectionalLight directionalLightData;
//
//
//		Vector3 dir = { 0.068f, -0.057f, -0.996f };
//
//	float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
//	dir.x /= length;
//	dir.y /= length;
//	dir.z /= length;
//	directionalLightData.direction = dir;
//
//		directionalLightData.intensity = 1.5f;
//	ComPtr<ID3D12Resource> directionalLightResource =
//		CreateBufferResource(device, sizeof(DirectionalLight));
//	DirectionalLight* mappedLight = nullptr;
//	directionalLightResource->Map(0, nullptr,
//		reinterpret_cast<void**>(&mappedLight));
//
//	//================================
//    //================================
//	PointLight pointLightData{};
//	pointLightData.color = { 1,1,1,1 };
//	pointLightData.position = { 0.0f, 2.0f, 0.0f };
//	pointLightData.intensity = 1.0f;
//
//	ComPtr<ID3D12Resource> pointLightResource =
//		CreateBufferResource(device, sizeof(PointLight));
//
//	PointLight* mappedPointLight = nullptr;
//	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPointLight));
//	*mappedPointLight = pointLightData;
//
//	//================================
//	//================================
//	SpotLight spotLight{};
//	spotLight.color = { 1,1,1,1 };
//	spotLight.position = { 2.0f, 1.25f, 0.0f };
//	spotLight.direction = Normalize(Vector3{ -1.0f, -1.0f, 0.0f });
//	spotLight.distance = 7.0f;
//	spotLight.intensity = 5.0f;
//	spotLight.decay = 2.0f;
//	spotLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
//
//	ComPtr<ID3D12Resource> spotLightResource = CreateBufferResource(device, sizeof(SpotLight));
//	SpotLight* mappedSpotLight = nullptr;
//	spotLightResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedSpotLight));
//	*mappedSpotLight = spotLight;
//
//	// -----------------------------
//	// -----------------------------
//	struct CameraForGPU {
//		Vector3 worldPosition;
//	};
//	ComPtr<ID3D12Resource> cameraResource =
//		CreateBufferResource(device, sizeof(CameraForGPU));
//	CameraForGPU* mappedCamera = nullptr;
//	cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedCamera));
//	mappedCamera->worldPosition = Vector3{ 0.0f, 0.0f, -5.0f };
//	mappedCamera->padding = 0.0f;
//
//	ComPtr<ID3D12Resource> transformationMatrixResourceSprite =
//		CreateBufferResource(device, sizeof(Matrix4x4));
//
//	Matrix4x4* transformationMatrixDataSprite = nullptr;
//
//	transformationMatrixResourceSprite->Map(
//		0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite)
//
//	);
//
//	*transformationMatrixDataSprite = MakeIdentity4x4();
//
//	Transform transformSprite{
//		{1.0f, 1.0f, 1.0f}, // scale
//		{0.0f, 0.0f, 0.0f}, // rotation
//		{0.0f, 0.0f, 0.0f}  // translation
//	};
//
//	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
//
//	vertexBufferViewSprite.BufferLocation =
//		vertexResourceSprite->GetGPUVirtualAddress();
//
//	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
//
//	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);
//
//	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
//
//	indexBufferViewSprite.BufferLocation =
//		indexResourceSprite->GetGPUVirtualAddress();
//
//	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
//
//	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
//
//	//**************************
//	//**************************
//	Vector4* vertexData = nullptr;
//
//	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
//
AppSceneResources scene;
AppPipelines appPipelines;
if (!appPipelines.Initialize(device.Get())) {
	OutputDebugStringA("[AppMain] AppPipelines initialization failed.\n");
	return 1;
}
if (!scene.Initialize(device, srvDescriptorHeap, descriptorSizeSRV)) {
	OutputDebugStringA("[AppMain] AppSceneResources initialization failed.\n");
	return 1;
}

AppRuntimeState runtimeState{};
ApplyEnvironmentRuntimeConfig(runtimeState.vfx);
AppParticleSystem particleSystem;
runtimeState.transform.scale = { 6.0f, 6.0f, 6.0f };
runtimeState.transform.rotate = { 0.0f, 0.0f, 0.0f };
runtimeState.transform.translate = { 0.0f, 0.0f, 0.0f };

runtimeState.transformSprite.scale = { 256.0f, 256.0f, 1.0f };
runtimeState.transformSprite.rotate = { 0.0f, 0.0f, 0.0f };
runtimeState.transformSprite.translate = { 640.0f, 360.0f, 0.0f };

runtimeState.uvTransformSprite.scale = { 1.0f, 1.0f, 1.0f };
runtimeState.uvTransformSprite.rotate = { 0.0f, 0.0f, 0.0f };
runtimeState.uvTransformSprite.translate = { 0.0f, 0.0f, 0.0f };

runtimeState.viewport.Width = 1280.0f;
runtimeState.viewport.Height = 720.0f;
runtimeState.viewport.TopLeftX = 0.0f;
runtimeState.viewport.TopLeftY = 0.0f;
runtimeState.viewport.MinDepth = 0.0f;
runtimeState.viewport.MaxDepth = 1.0f;

runtimeState.scissorRect.left = 0;
runtimeState.scissorRect.top = 0;
runtimeState.scissorRect.right = 1280;
runtimeState.scissorRect.bottom = 720;

runtimeState.directionalLightData = scene.directionalLightData;
runtimeState.pointLightData = scene.pointLightData;
runtimeState.spotLight = scene.spotLight;
runtimeState.cameraWorldPosition = scene.mappedCamera->worldPosition;
runtimeState.materialData = *scene.materialData;

AppImGuiLayer imguiLayer;
imguiLayer.Initialize(
	hwnd,
	device.Get(),
	static_cast<int>(swapChain.BufferCount()),
	kRtvFormat,
	srvDescriptorHeap.Get());


AppRenderResources renderResources;
assert(renderResources.InitializeParticleQuad(device));
AppFrameRenderer frameRenderer;



	//commandAllocator->Reset();
	//commandList->Reset(commandAllocator.Get(), appPipelines.GetMainPSO());


	//===============================================
	//==============================================
	// ========================================================
	// ========================================================

	constexpr uint32_t kInstancingSrvIndex = 10;
	ComPtr<ID3D12Resource> instancingResource =
		particleSystem.CreateInstancingResource(device.Get());
	particleSystem.InitializeInstancingBuffer(
		instancingResource.Get(),
		particleSystem.MaxInstances());

	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvCPU =

		AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, kInstancingSrvIndex);

	D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvGPU =
		AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, kInstancingSrvIndex);

	particleSystem.CreateInstancingSrv(
		device.Get(),
		instancingResource.Get(),
		instancingSrvCPU,
		instancingSrvGPU);
	const UINT texWidth = 1280;
	const UINT texHeight = 720;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ComPtr<ID3D12Resource> texture =
		CreateTextureResourceResolution(device, texWidth, texHeight, format);



	D3D12_CPU_DESCRIPTOR_HANDLE receivedSrvHandleCPU =
		AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 4);
	CreateTextureSRV(device.Get(), texture.Get(),
		receivedSrvHandleCPU);

	//**************************
	//**************************
	AppAudio audio;
	audio.Initialize();


    ::audio::SoundData soundData1 = audio.LoadWave("Resources/Alarm01.wav");

	audio.PlayWave(soundData1);
	DebugCamera debugCamera;
	debugCamera.Initialize();

	////**************************
	////**************************
	//IDirectInput8* directInput = nullptr;
	//result =
	//	DirectInput8Create(wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
	//		(void**)&directInput, nullptr);
	//assert(SUCCEEDED(result));

	//IDirectInputDevice8* keyboard = nullptr;
	//result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	//assert(SUCCEEDED(result));

	//assert(SUCCEEDED(result));

	//result = keyboard->SetCooperativeLevel(
	//	hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	//assert(SUCCEEDED(result));
	CpuTransferCtx xferCtx;

	MSG msg{};

	FrameLoopState frameState{};
	AppRunLoop runLoop(
		debugCamera,
		runtimeState,
		scene,
		particleSystem,
		imguiLayer,
		frameRenderer,
		appPipelines,
		renderResources,
		swapChain,
		clPool,
		engineContext,
		heaps,
		dev,
		srvDescriptorHeap,
		wvpData,
		windowWidth,
		windowHeight,
		frameState,
		commandQueue.Get(),
		fence.Get(),
		fenceEvent);
	runLoop.InitializeBeam(
		device.Get(),
		srvDescriptorHeap.Get(),
		descriptorSizeSRV,
		kRtvFormat,
		DXGI_FORMAT_D24_UNORM_S8_UINT);
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			runLoop.RenderFrame();
		}

	}

	MFShutdown();

	//**************************
	//**************************
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	std::string logFilePath = std::string("logs/") + dateString + "log";
	std::ofstream logStream(logFilePath);
#if defined(_DEBUG)||DEVELOP
	imguiLayer.Shutdown();
#endif
	//**************************
	//**************************

	engineContext.Shutdown();

	/*if (fence)
	{
			fence->Release();
			fence = nullptr;
	}*/

	/*if (rtvDescriptorHeap)
	{
			rtvDescriptorHeap->Release();
			rtvDescriptorHeap = nullptr;
	}*/

	/*if (commandList)
	{
			commandList->Release();
			commandList = nullptr;
	}*/

	/*if (commandAllocator)
	{
			commandAllocator->Release();
			commandAllocator = nullptr;
	}*/

	/*if (commandQueue) {
			commandQueue->Release();
			commandQueue = nullptr;
	}*/

	// if (rtvDescriptorHeap)
	//{
	//	rtvDescriptorHeap->Release();
	//	rtvDescriptorHeap = nullptr;
	// }

	/*for (int i = 0; i < 2; ++i)
	{
			if (swapChainResources[i])
			{
					swapChainResources[i]->Release();
					swapChainResources[i] = nullptr;
			}
	}*/

	/*if (swapChain)
	{
			swapChain->Release();
			swapChain = nullptr;
	}*/

	/*if (device)
	{
			device->Release();
			device = nullptr;
	}*/

	/*if (useAdapter)
	{
			useAdapter->Release();
			useAdapter = nullptr;
	}*/

	/*if (dxgiFactory)
	{
			dxgiFactory->Release();
			dxgiFactory = nullptr;
	}*/

	/*vertexResourceSphere->Release();*/

	/*if (sphere.vertexResource) {
			sphere.vertexResource->Release();
			sphere.vertexResource = nullptr;
	}
	if (sphere.cbvResource) {
			sphere.cbvResource->Release();
			sphere.cbvResource = nullptr;
	}*/

	/*includeHandler->Release();
	dxcCompiler->Release();
	dxcUtils->Release();*/

	// graphicsPipelineState->Release();

	// rootSignature->Release();
	
	

	audio.Finalize();

	runLoop.Shutdown();
	audio.Unload(&soundData1);

	// materialResource->Release();

#if defined(_DEBUG) || defined(DEVELOP)
	ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();

		debugController->SetEnableGPUBasedValidation(TRUE);
	}
	debugController.Reset();
#endif
	bootstrap.Shutdown();
	return 0;

}






