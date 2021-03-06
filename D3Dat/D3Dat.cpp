#pragma region("preprocessor")
#pragma region("imports")
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <shellapi.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <vector>
#include <complex>
#include <cmath>
//#include "d3dx12.h"
#pragma endregion("imports")
#pragma region("linker directives")

#pragma comment(linker, "/DEFAULTLIB:D3d12.lib")
#pragma comment(linker, "/DEFAULTLIB:DXGI.lib")
#pragma comment(linker, "/DEFAULTLIB:D3DCompiler.lib")

#pragma endregion("linker directives")
#pragma region("preprocessor system checks")

#ifdef _DEBUG
#ifdef NDEBUG
#error debug flag conflict
#endif
#endif

#if !_HAS_CXX20
#error C++20 is required
#endif

#ifndef __has_cpp_attribute
#error critical macro __has_cpp_attribute not defined
#endif

#if !__has_include(<Windows.h>)
#error critital header Windows.h not found
#endif

#pragma endregion("preprocessor system checks")
#pragma region("macros")

#ifdef min
#pragma push_macro("min")
#undef min
#endif

#ifdef max
#pragma push_macro("max")
#undef max
#endif

#ifdef CreateWindow
#pragma push_macro("CreateWindow")
#undef CreateWindow
#endif

#ifdef _DEBUG
#define BDEBUG 1
#else
#define BDEBUG 0
#endif

#if __has_cpp_attribute(nodiscard) < 201603L
#define _NODISCARD [[nodiscard]]
#endif

#ifndef DECLSPEC_NOINLINE
#if _MSC_VER >= 1300
#define DECLSPEC_NOINLINE  __declspec(noinline)
#else
#define DECLSPEC_NOINLINE
#endif
#endif

#ifndef _NORETURN
#define _NORETURN [[_Noreturn]]
#endif

#pragma endregion("macros")
#pragma region("function macros")

#ifdef _DEBUG
#define ASSERT_SUCCESS(x) assert(SUCCEEDED(x))
#else
#define ASSERT_SUCCESS(x) x
#endif

#define THROW_ON_FAIL(x) \
	if (FAILED(x)){ \
		std::cout << "[" << __LINE__ <<"]ERROR: " << std::hex << x << std::dec << std::endl; \
		throw std::exception(); \
	}

#pragma endregion("function macros")
#pragma endregion("preprocessor")

using Microsoft::WRL::ComPtr;

#pragma region("structures")

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;

};

// this is the structure of our constant buffer.
struct ConstantBuffer {
	DirectX::XMFLOAT4 colorMultiplier;
	DirectX::XMFLOAT4 mousePos;
};
struct ConstantBuffer_Iterations {
	int numIterations;
};
struct ConstantBuffer_Camera {
	int camera_xpos;
	int camera_ypos;
};
#pragma endregion("structures")
#pragma region("globals")
constexpr uint8_t g_NumFrames = 3;
constexpr bool showFPS = false;
constexpr wchar_t const* FENCE_NAME = L"_GlobalFence";
constexpr UINT sampleCount = 8;
constexpr UINT sampleQuality = 0;

uint32_t g_ClientWidth = 1280;
uint32_t g_ClientHeight = 720;

double temp = .2;
bool isWireframe = false;
static RECT g_WindowRect;

static ComPtr<ID3D12Device2> g_Device;
static ComPtr<ID3D12CommandQueue> g_CommandQueue;
static ComPtr<IDXGISwapChain4> g_SwapChain;
static ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
static ComPtr<ID3D12Resource> g_IntermediateBuffers[g_NumFrames];
static ComPtr<ID3D12GraphicsCommandList> g_CommandList;
static ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
static ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
static ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
static ComPtr<ID3D12Fence> g_Fence;
static ComPtr<ID3D12PipelineState> pipelineStateObject;
static ComPtr<ID3D12PipelineState> pipelineStateObject2;
static ComPtr<ID3D12PipelineState> pipelineStateObject_msaa;
static ComPtr<ID3D12RootSignature> rootSignature;

static ComPtr<ID3D12Resource> depthStencilBuffer;
static ComPtr<ID3D12DescriptorHeap> dsDescriptorHeap;

static ComPtr<ID3D12Resource> vertexBuffer;
static D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
static ComPtr<ID3D12Resource> vBufferUploadHeap;

static ComPtr<ID3D12Resource> indexBuffer;
static D3D12_INDEX_BUFFER_VIEW indexBufferView;
static ComPtr<ID3D12Resource> iBufferUploadHeap;

static D3D12_RECT scissorRect;
static D3D12_VIEWPORT viewport;
static D3D12_VIEWPORT viewport2;
D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

static uint64_t g_FenceValue = 0;
static uint64_t g_FrameFenceValues[g_NumFrames] = {};

static bool g_VSync = true;
static bool g_TearingSupported = false;
static bool g_Fullscreen = false;

static uint64_t frameCounter = 0;
static double elapsedSeconds = 0.0;
static std::chrono::high_resolution_clock fpsClock;

ComPtr<ID3D12DescriptorHeap> mainDescriptorHeap[g_NumFrames]; // this heap will store the descripor to our constant buffer
ComPtr<ID3D12Resource> constantBufferUploadHeap[g_NumFrames]; // this is the memory on the gpu where our constant buffer will be placed.

ConstantBuffer cbColorMultiplierData; // this is the constant buffer data we will send to the gpu 
										// (which will be placed in the resource we created above)
ConstantBuffer_Iterations cbIterations;
ConstantBuffer_Camera cbCamera;

UINT8* cbColorMultiplierGPUAddress[g_NumFrames]; // this is a pointer to the memory location we get when we map our constant buffer
UINT8* cbIterationsGPUAddress[g_NumFrames]; // this is a pointer to the memory location we get when we map our constant buffer
UINT8* cbCameraGPUAddress[g_NumFrames];

#pragma endregion("globals")

LRESULT CALLBACK WndProc_preinit(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc_postinit(HWND, UINT, WPARAM, LPARAM);

int main(void)
{
	bool useWarp = false;

	{
		int argc;
		wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

		for (size_t i = 0; i < argc; i++)
		{
			if (wcscmp(argv[i], L"-w") == 0 || wcscmp(argv[i], L"--width") == 0)
			{
				g_ClientWidth = wcstol(argv[++i], nullptr, 10);
			}
			if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--height") == 0)
			{
				g_ClientHeight = wcstol(argv[++i], nullptr, 10);
			}
			if (wcscmp(argv[i], L"-warp") == 0 || wcscmp(argv[i], L"--warp") == 0)
			{
				useWarp = true;
			}
		}

		LocalFree(argv);
	}

	HINSTANCE hInstance;
	GetModuleHandleExW(0, nullptr, &hInstance);

	// TODO: what is this?
	//SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Window class name. Used for registering / creating the window.
	constexpr wchar_t const* windowClassName = L"DX12WindowClass";


	ComPtr<IDXGIFactory4> factory4;

#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugInterface;
	THROW_ON_FAIL(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugInterface));
	debugInterface->EnableDebugLayer();
#endif

	{
		BOOL allowTearing = FALSE;

		constexpr UINT createFactoryFlags =
#ifdef _DEBUG
			DXGI_CREATE_FACTORY_DEBUG
#else
			0
#endif
			;
		//TODO: why create a IDXGIFactory4, then cast to IDXGIFactory5?
		if (SUCCEEDED(CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), &factory4)))
		{
			ComPtr<IDXGIFactory5> factory5;
			if (SUCCEEDED(factory4.As(&factory5)))
			{
				if (FAILED(factory5->CheckFeatureSupport(
					DXGI_FEATURE_PRESENT_ALLOW_TEARING,
					&allowTearing, sizeof(allowTearing))))
				{
					allowTearing = FALSE;
				}
			}
		}
		g_TearingSupported = (allowTearing == TRUE);
	}

	{
		const WNDCLASSEXW windowClass = {
			.cbSize = sizeof(WNDCLASSEXW),
			.style = CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = &WndProc_preinit,
			.cbClsExtra = 0,
			.cbWndExtra = 0,
			.hInstance = hInstance,
			.hIcon = LoadIconW(hInstance, IDI_APPLICATION),
			.hCursor = LoadCursorW(hInstance, IDC_ARROW),
			.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
			.lpszMenuName = nullptr,
			.lpszClassName = windowClassName,
			.hIconSm = LoadIconW(hInstance, IDI_APPLICATION)
		};

		ASSERT_SUCCESS(RegisterClassExW(&windowClass));
	}

	HWND hWnd;

	{
		const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

		g_WindowRect = {
			.left = 0,
			.top = 0,
			.right = static_cast<LONG>(g_ClientWidth),
			.bottom = static_cast<LONG>(g_ClientHeight)
		};

		AdjustWindowRect(&g_WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

		const int windowWidth = g_WindowRect.right - g_WindowRect.left;
		const int windowHeight = g_WindowRect.bottom - g_WindowRect.top;

		// Center the window within the screen. Clamp to 0, 0 for the top-left corner.

		hWnd = CreateWindowExW(
			NULL,
			windowClassName,
			L"DirectX 12",
			WS_OVERLAPPEDWINDOW,
			std::max(0, (screenWidth - windowWidth) / 2),
			std::max(0, (screenHeight - windowHeight) / 2),
			windowWidth,
			windowHeight,
			NULL,
			NULL,
			hInstance,
			nullptr
		);

		assert(hWnd && "Failed to create window");
		ShowWindow(hWnd, SW_SHOW);
	}

	{
		//TODO: no effect?
		THROW_ON_FAIL(factory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		ComPtr<IDXGIFactory6> factory6;
		THROW_ON_FAIL(factory4.As(&factory6));
		ComPtr<IDXGIAdapter4> dxgiAdapter4;

		if (useWarp)
		{
			THROW_ON_FAIL(factory4->EnumWarpAdapter(__uuidof(IDXGIAdapter4), &dxgiAdapter4));
			THROW_ON_FAIL(D3D12CreateDevice(dxgiAdapter4.Get(), D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device2), &g_Device));
		}
		else
		{
			factory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter4), &dxgiAdapter4);
			THROW_ON_FAIL(D3D12CreateDevice(dxgiAdapter4.Get(), D3D_FEATURE_LEVEL_11_1, __uuidof(ID3D12Device2), &g_Device));
		}

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
		msLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Replace with your render target format.
		msLevels.SampleCount = sampleCount; // Replace with your sample count.
		msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

		g_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels));
		std::cout << "quality levels " << msLevels.NumQualityLevels << std::endl;
		// Enable debug messages in debug mode.
#ifdef _DEBUG
		ComPtr<ID3D12InfoQueue> pInfoQueue;
		if (SUCCEEDED(g_Device.As(&pInfoQueue)))
		{
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			// Suppress whole categories of messages
			//D3D12_MESSAGE_CATEGORY Categories[] = {};

			// Suppress messages based on their severity level
			D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			// Suppress individual messages by their ID
			D3D12_MESSAGE_ID DenyIds[] = {
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
			};

			D3D12_INFO_QUEUE_FILTER NewFilter = {
				.DenyList = {
					.NumSeverities = _countof(Severities),
					.pSeverityList = Severities,
					.NumIDs = _countof(DenyIds),
					.pIDList = DenyIds
				}
			};
			THROW_ON_FAIL(pInfoQueue->PushStorageFilter(&NewFilter));
		}
#endif
	}

	{
		constexpr D3D12_COMMAND_QUEUE_DESC desc = {
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
			.NodeMask = 0
		};
		THROW_ON_FAIL(g_Device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (&g_CommandQueue)));
	}

	{
		const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
			.Width = g_ClientWidth,
			.Height = g_ClientHeight,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.Stereo = FALSE,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = g_NumFrames,
			.Scaling = DXGI_SCALING_STRETCH,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
			.Flags = (g_TearingSupported) ? (DWORD)DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : (DWORD)0
		};

		ComPtr<IDXGISwapChain1> swapChain1;//intermediate

		THROW_ON_FAIL(factory4->CreateSwapChainForHwnd(
			g_CommandQueue.Get(),
			hWnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain1));
		THROW_ON_FAIL(swapChain1.As(&g_SwapChain));
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = g_NumFrames * 2 //Swap chain render targets + intermediate render targets
		};
		THROW_ON_FAIL(g_Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &g_RTVDescriptorHeap));
	}

	{

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		ComPtr<ID3D12Resource> backBuffer;
		const INT64 descriptorHandleIncrementSize = (INT64)g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (int i = 0; i < g_NumFrames; i++)
		{
			//TODO: create intermediate render target with higher sample count

			D3D12_HEAP_PROPERTIES heapProperties =
			{
				.Type = D3D12_HEAP_TYPE_DEFAULT,
				.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
				.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
				.CreationNodeMask = 1,
				.VisibleNodeMask = 1
			};

			D3D12_RESOURCE_DESC resourceDesc = {
				.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
				.Alignment = 0,
				.Width = g_ClientWidth,
				.Height = g_ClientHeight,
				.DepthOrArraySize = 1,
				.MipLevels = 1,
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.SampleDesc = {
					.Count = sampleCount,
					.Quality = sampleQuality
				},
				.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
				.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
			};

			g_Device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&resourceDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				nullptr,
				__uuidof(ID3D12Resource),
				&g_IntermediateBuffers[i]
			);

			g_IntermediateBuffers[i]->SetName(L"Intermediate Render Target");
			g_Device->CreateRenderTargetView(g_IntermediateBuffers[i].Get(), nullptr, rtvHandle);

			rtvHandle.ptr = (SIZE_T)((INT64)rtvHandle.ptr + descriptorHandleIncrementSize);
		}

		for (int i = 0; i < g_NumFrames; i++)
		{

			THROW_ON_FAIL(g_SwapChain->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffer));

			g_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

			g_BackBuffers[i] = backBuffer;
			rtvHandle.ptr = (SIZE_T)((INT64)rtvHandle.ptr + descriptorHandleIncrementSize);

			THROW_ON_FAIL(g_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &g_CommandAllocators[i]));
		}
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};

		THROW_ON_FAIL(g_Device->CreateDescriptorHeap(&dsvHeapDesc, __uuidof(ID3D12DescriptorHeap), &m_DSVHeap));
	}

	THROW_ON_FAIL(g_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_CommandAllocators[g_SwapChain->GetCurrentBackBufferIndex()].Get(), nullptr, __uuidof(ID3D12GraphicsCommandList1), &g_CommandList));

	THROW_ON_FAIL(g_Device->CreateFence(g_FenceValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &g_Fence));

	HANDLE g_FenceEvent = CreateEventW(nullptr, FALSE, FALSE, FENCE_NAME);

	assert(g_FenceEvent && "Failed to create fence event.");

	{
		D3D12_DESCRIPTOR_RANGE  descriptorTableRanges[1] =
		{ // only one range right now
			{
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV, // this is a range of constant buffer views (descriptors)
				.NumDescriptors = 2, // we only have one constant buffer, so the range is only 1
				.BaseShaderRegister = 0, // start index of the shader registers in the range
				.RegisterSpace = 0, // space 0. can usually be zero
				.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // this appends the range to the end of the root signature descriptor tables
			}
		};

		D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable = {
			.NumDescriptorRanges = _countof(descriptorTableRanges), // we only have one range
			.pDescriptorRanges = &descriptorTableRanges[0] // the pointer to the beginning of our ranges array
		};

		// create a root parameter and fill it out
		D3D12_ROOT_PARAMETER  rootParameters[1] = { { // only one parameter right now
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, // this is a descriptor table
			.DescriptorTable = descriptorTable, // this is our descriptor table for this root parameter
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL // our vertex shader will be the only shader accessing this parameter for now
		}
		};

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc =
		{
			.NumParameters = _countof(rootParameters),
			.pParameters = rootParameters,
			.NumStaticSamplers = 0,
			.pStaticSamplers = nullptr,
			.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS // we can deny shader stages here for better performance
		};

		for (int i = 0; i < g_NumFrames; i++)
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
				.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				.NumDescriptors = 1,
				.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
			};

			THROW_ON_FAIL(g_Device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), &mainDescriptorHeap[i]));
		}

		// create the constant buffer resource heap
// We will update the constant buffer one or more times per frame, so we will use only an upload heap
// unlike previously we used an upload heap to upload the vertex and index data, and then copied over
// to a default heap. If you plan to use a resource for more than a couple frames, it is usually more
// efficient to copy to a default heap where it stays on the gpu. In this case, our constant buffer
// will be modified and uploaded at least once per frame, so we only use an upload heap

// create a resource heap, descriptor heap, and pointer to cbv for each frame
		//todo: fix
		for (int i = 0; i < g_NumFrames; i++)
		{
			D3D12_HEAP_PROPERTIES heapProperties =
			{
				.Type = D3D12_HEAP_TYPE_UPLOAD,
				.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
				.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
				.CreationNodeMask = 1,
				.VisibleNodeMask = 1
			};

			D3D12_RESOURCE_DESC resourceDesc = {
				.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
				.Alignment = 0,
				.Width = 1024 * 64,
				.Height = 1,
				.DepthOrArraySize = 1,
				.MipLevels = 1,
				.Format = DXGI_FORMAT_UNKNOWN,
				.SampleDesc = {
					.Count = 1,
					.Quality = 0
				},
				.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
				.Flags = D3D12_RESOURCE_FLAG_NONE,
			};

			g_Device->CreateCommittedResource(
				&heapProperties, // this heap will be used to upload the constant buffer data
				D3D12_HEAP_FLAG_NONE, // no flags
				&resourceDesc, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
				D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
				nullptr, // we do not have use an optimized clear value for constant buffers
				__uuidof(ID3D12Resource), &constantBufferUploadHeap[i]);
			constantBufferUploadHeap[i]->SetName(L"Constant Buffer Upload Resource Heap");
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = constantBufferUploadHeap[i]->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = (sizeof(ConstantBuffer) + sizeof(ConstantBuffer_Iterations) + sizeof(ConstantBuffer_Camera) + 255) & ~255;    // CB size is required to be 256-byte aligned.
			g_Device->CreateConstantBufferView(&cbvDesc, mainDescriptorHeap[i].Get()->GetCPUDescriptorHandleForHeapStart());
			//memset(&cbColorMultiplierData, 0, sizeof(cbColorMultiplierData));
			cbColorMultiplierData.colorMultiplier.x = 1;
			cbColorMultiplierData.colorMultiplier.y = 1;
			cbColorMultiplierData.colorMultiplier.z = 1;

			cbIterations.numIterations = 1;

			cbCamera.camera_xpos = 0;
			cbCamera.camera_ypos = 0;
			D3D12_RANGE readRange{ .Begin = 0, .End = 0 };    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
			constantBufferUploadHeap[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbColorMultiplierGPUAddress[i]));
			memcpy(cbColorMultiplierGPUAddress[i], &cbColorMultiplierData, sizeof(cbColorMultiplierData));
			memcpy(cbColorMultiplierGPUAddress[i] + sizeof(cbColorMultiplierData), &cbIterations, sizeof(cbIterations));
			memcpy(cbColorMultiplierGPUAddress[i] + sizeof(cbColorMultiplierData) + sizeof(cbIterations), &cbCamera, sizeof(cbCamera));
		}
		ComPtr<ID3DBlob> signature;
		THROW_ON_FAIL(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));

		THROW_ON_FAIL(g_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(ID3D12RootSignature), &rootSignature));

		// create vertex and pixel shaders

		// when debugging, we can compile the shader files at runtime.
		// but for release versions, we can compile the hlsl shaders
		// with fxc.exe to create .cso files, which contain the shader
		// bytecode. We can load the .cso files at runtime to get the
		// shader bytecode, which of course is faster than compiling
		// them at runtime

		// compile vertex shader
		ComPtr<ID3DBlob> vertexShader; // d3d blob for holding vertex shader bytecode
		ComPtr<ID3DBlob> errorBuff; // a buffer holding the error data if any
		THROW_ON_FAIL(D3DCompileFromFile(L"VertexShader.hlsl",
			nullptr,
			nullptr,
			"main",
			"vs_5_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3,
			0,
			&vertexShader,
			&errorBuff));

		// fill out a shader bytecode structure, which is basically just a pointer
		// to the shader bytecode and the size of the shader bytecode

		// compile pixel shader
		ComPtr<ID3DBlob> pixelShader;
		THROW_ON_FAIL(D3DCompileFromFile(L"PixelShader.hlsl",
			nullptr,
			nullptr,
			"main",
			"ps_5_0",
			//D3DCOMPILE_DEBUG | 
			D3DCOMPILE_OPTIMIZATION_LEVEL3,
			0,
			&pixelShader,
			&errorBuff));

		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{
				.SemanticName = "POSITION",
				.SemanticIndex = 0,
				.Format = DXGI_FORMAT_R32G32B32_FLOAT,
				.InputSlot = 0,
				.AlignedByteOffset = 0,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0
			},
			{
				.SemanticName = "COLOR",
				.SemanticIndex = 0,
				.Format = DXGI_FORMAT_R32G32B32_FLOAT,
				.InputSlot = 0,
				.AlignedByteOffset = 12,
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = 0
			}
		};

		psoDesc = {
			.pRootSignature = rootSignature.Get(),
			.VS = {
				.pShaderBytecode = vertexShader->GetBufferPointer(),
				.BytecodeLength = vertexShader->GetBufferSize()
			},
			.PS = {
				.pShaderBytecode = pixelShader->GetBufferPointer(),
				.BytecodeLength = pixelShader->GetBufferSize()
			},
			.BlendState = {
				.AlphaToCoverageEnable = FALSE,
				.IndependentBlendEnable = FALSE,
				.RenderTarget = {
					{
						.BlendEnable = FALSE,
						.LogicOpEnable = FALSE,
						.SrcBlend = D3D12_BLEND_ONE,
						.DestBlend = D3D12_BLEND_ZERO,
						.BlendOp = D3D12_BLEND_OP_ADD,
						.SrcBlendAlpha = D3D12_BLEND_ONE,
						.DestBlendAlpha = D3D12_BLEND_ZERO,
						.BlendOpAlpha = D3D12_BLEND_OP_ADD,
						.LogicOp = D3D12_LOGIC_OP_NOOP,
						.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
					}//TODO: X8
				}
			},
			.SampleMask = 0xFFFFFFFF,
			.RasterizerState = {
				.FillMode = D3D12_FILL_MODE_WIREFRAME,
				.CullMode = D3D12_CULL_MODE_NONE,
				.FrontCounterClockwise = FALSE,
				.DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
				.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
				.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
				.DepthClipEnable = TRUE,
				.MultisampleEnable = FALSE,
				.AntialiasedLineEnable = FALSE,
				.ForcedSampleCount = 0,
				.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
			},
			.DepthStencilState = {
				.DepthEnable = TRUE,
				.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
				.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
				.StencilEnable = FALSE,
				.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
				.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
				.FrontFace = {
					.StencilFailOp = D3D12_STENCIL_OP_KEEP,
					.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
					.StencilPassOp = D3D12_STENCIL_OP_KEEP,
					.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
				},
				.BackFace = {
					.StencilFailOp = D3D12_STENCIL_OP_KEEP,
					.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
					.StencilPassOp = D3D12_STENCIL_OP_KEEP,
					.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
				}
			},
			.InputLayout = {
				.pInputElementDescs = inputLayout,
				.NumElements = _countof(inputLayout),
			},
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1,
			.RTVFormats = {DXGI_FORMAT_R8G8B8A8_UNORM},
			.DSVFormat = DXGI_FORMAT_D32_FLOAT,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0
			},
		};
		THROW_ON_FAIL(g_Device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &pipelineStateObject));
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		THROW_ON_FAIL(g_Device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &pipelineStateObject2));
		psoDesc.DepthStencilState = {};
		psoDesc.SampleDesc.Count = sampleCount;
		THROW_ON_FAIL(g_Device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &pipelineStateObject_msaa));
	}

	{
		// create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
		};
		THROW_ON_FAIL(g_Device->CreateDescriptorHeap(&dsvHeapDesc, __uuidof(ID3D12DescriptorHeap), &dsDescriptorHeap));
	}

	{
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {
			.Format = DXGI_FORMAT_D32_FLOAT,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE
		};

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {
			.Format = DXGI_FORMAT_D32_FLOAT,
			.DepthStencil = {
				.Depth = 1.0f,
				.Stencil = 0
			}
		};
		D3D12_HEAP_PROPERTIES heapProperties = {
			.Type = D3D12_HEAP_TYPE_DEFAULT,
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask = 1,
			.VisibleNodeMask = 1
		};

		D3D12_RESOURCE_DESC resourceDesc = {
			.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment = 0,
			.Width = g_ClientWidth,
			.Height = g_ClientHeight,
			.DepthOrArraySize = 1,
			.MipLevels = 0,
			.Format = DXGI_FORMAT_D32_FLOAT,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0
			},
			.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		};

		g_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			__uuidof(ID3D12Resource),
			&depthStencilBuffer
		);

		dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

		g_Device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	}

	Vertex vList[] = {
		{.pos = {-1.f, -1.f, 0.9f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}},
		{.pos = {1.0f, -1.f, 0.6f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
		{.pos = {-1.f, 1.0f, 0.6f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
		{.pos = {1.0f, 1.0f, 0.9f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},

		{.pos = {-1.0f, -1.f, 0.1f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}},
		{.pos = {1.0f, -1.f, 0.9f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
		{.pos = {-1.0f, 1.f, 0.9f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
		{.pos = {1.0f, 1.f, 0.1f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
	};

	constexpr UINT64 vBufferSize = sizeof(vList);
	{
		D3D12_HEAP_PROPERTIES heapProperties =
		{
			.Type = D3D12_HEAP_TYPE_DEFAULT,
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask = 1,
			.VisibleNodeMask = 1
		};

		D3D12_RESOURCE_DESC resourceDesc = {
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = 0,
			.Width = vBufferSize,
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc =
			{
				.Count = 1,
				.Quality = 0
			},
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_NONE
		};

		g_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			__uuidof(ID3D12Resource),
			&vertexBuffer
		);

		vertexBuffer->SetName(L"Triangle 1 Vertex Buffer");

		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;//reuse properties struct for upload heap
		g_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			__uuidof(ID3D12Resource),
			&vBufferUploadHeap
		);
		vBufferUploadHeap->SetName(L"Triangle 1 Vertex Buffer Upload Heap");

		//todo: fix use of reinterpret_cast
		D3D12_SUBRESOURCE_DATA vertexData = {
			.pData = reinterpret_cast<BYTE*>(vList),
			.RowPitch = sizeof(vList),
			.SlicePitch = sizeof(vList)
		};

		//TODO: this section is still filled with mystery, needs to be further investigated

		{
			struct TempData {
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts;
				UINT64 RowSizesInBytes;
				UINT NumRows;
			};
			//TODO: use custom allocator from paged pool
			TempData* myData = (TempData*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(TempData));

			D3D12_RESOURCE_DESC Desc = vertexBuffer->GetDesc();
			UINT64 RequiredSize;

			g_Device->GetCopyableFootprints(&Desc, 0, 1, 0, &myData->layouts, &myData->NumRows, &myData->RowSizesInBytes, &RequiredSize);

			std::cout << "myData->layouts.Offset " << myData->layouts.Offset << std::endl;
			std::cout << "myData->layouts.Footprint.Format " << myData->layouts.Footprint.Format << std::endl;
			std::cout << "myData->layouts.Footprint.Width " << myData->layouts.Footprint.Width << std::endl;
			std::cout << "myData->layouts.Footprint.Height " << myData->layouts.Footprint.Height << std::endl;
			std::cout << "myData->layouts.Footprint.Depth " << myData->layouts.Footprint.Depth << std::endl;
			std::cout << "myData->layouts.Footprint.RowPitch " << myData->layouts.Footprint.RowPitch << std::endl;
			std::cout << "myData->NumRows " << myData->NumRows << std::endl;
			std::cout << "myData->RowSizesInBytes " << myData->RowSizesInBytes << std::endl;
			std::cout << "RequiredSize " << RequiredSize << std::endl;

			BYTE* pData = nullptr;

			THROW_ON_FAIL(vBufferUploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&pData)));

			for (int x = 0; x < myData->layouts.Footprint.Depth; x++)
			{
				BYTE* pDestSlice = static_cast<BYTE*>(pData + myData->layouts.Offset) + ((SIZE_T)myData->layouts.Footprint.RowPitch * (SIZE_T)myData->NumRows) * x;
				const BYTE* pSrcSlice = static_cast<const BYTE*>(vertexData.pData) + ((SIZE_T)myData->layouts.Footprint.RowPitch * (SIZE_T)myData->NumRows) * (LONG_PTR)x;//TODO: what is the LONG_PTR cast for?

				for (int y = 0; y < myData->NumRows; y++)
				{
					memcpy(
						pDestSlice + myData->layouts.Footprint.RowPitch * (UINT64)y,
						pSrcSlice + vertexData.RowPitch * (LONG_PTR)y, //again, the mysterious cast
						static_cast<SIZE_T>(myData->RowSizesInBytes));
				}
			}

			vBufferUploadHeap->Unmap(0, nullptr);

			g_CommandList->CopyBufferRegion(vertexBuffer.Get(), 0, vBufferUploadHeap.Get(), myData->layouts.Offset, myData->layouts.Footprint.Width);

			HeapFree(GetProcessHeap(), 0, myData);

			/*
			* unknown functions used:
			* g_Device->GetCopyableFootprints
			* vBufferUploadHeap->Map
			* vBufferUploadHeap->Unmap
			*/
		}

		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition{
				.pResource = vertexBuffer.Get(),
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
				.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			},
		};
		g_CommandList->ResourceBarrier(1, &barrier);
	}

	DWORD iList[] = {
		0, 1, 2, // first triangle
		1, 2, 3, // second triangle

		4, 5, 6,
		5, 6, 7,
	};
	constexpr UINT64 iBufferSize = sizeof(iList);

	{
		D3D12_HEAP_PROPERTIES heapProperties =
		{
			.Type = D3D12_HEAP_TYPE_DEFAULT,
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask = 1,
			.VisibleNodeMask = 1
		};

		D3D12_RESOURCE_DESC resourceDesc = {
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = 0,
			.Width = iBufferSize,
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc =
			{
				.Count = 1,
				.Quality = 0
			},
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_NONE
		};

		g_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			__uuidof(ID3D12Resource),
			&indexBuffer
		);

		vertexBuffer->SetName(L"Triangle 1 Index Buffer");

		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;//reuse properties struct for upload heap
		g_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			__uuidof(ID3D12Resource),
			&iBufferUploadHeap
		);
		iBufferUploadHeap->SetName(L"Triangle 1 Index Buffer Upload Heap");

		//TODO: fix use of reinterpret_cast
		D3D12_SUBRESOURCE_DATA IndexData = {
			.pData = reinterpret_cast<BYTE*>(iList),
			.RowPitch = sizeof(iList),
			.SlicePitch = sizeof(iList)
		};

		//TODO: this section is still filled with mystery, needs to be further investigated

		{
			struct TempData {
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts;
				UINT64 RowSizesInBytes;
				UINT NumRows;
			};
			//TODO: use custom allocator from paged pool
			TempData* myData = (TempData*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, sizeof(TempData));

			D3D12_RESOURCE_DESC Desc = indexBuffer->GetDesc();
			UINT64 RequiredSize;

			g_Device->GetCopyableFootprints(&Desc, 0, 1, 0, &myData->layouts, &myData->NumRows, &myData->RowSizesInBytes, &RequiredSize);

			std::cout << "myData->layouts.Offset " << myData->layouts.Offset << std::endl;
			std::cout << "myData->layouts.Footprint.Format " << myData->layouts.Footprint.Format << std::endl;
			std::cout << "myData->layouts.Footprint.Width " << myData->layouts.Footprint.Width << std::endl;
			std::cout << "myData->layouts.Footprint.Height " << myData->layouts.Footprint.Height << std::endl;
			std::cout << "myData->layouts.Footprint.Depth " << myData->layouts.Footprint.Depth << std::endl;
			std::cout << "myData->layouts.Footprint.RowPitch " << myData->layouts.Footprint.RowPitch << std::endl;
			std::cout << "myData->NumRows " << myData->NumRows << std::endl;
			std::cout << "myData->RowSizesInBytes " << myData->RowSizesInBytes << std::endl;
			std::cout << "RequiredSize " << RequiredSize << std::endl;

			BYTE* pData = nullptr;

			THROW_ON_FAIL(iBufferUploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&pData)));

			for (int x = 0; x < myData->layouts.Footprint.Depth; x++)
			{
				BYTE* pDestSlice = static_cast<BYTE*>(pData + myData->layouts.Offset) + ((SIZE_T)myData->layouts.Footprint.RowPitch * (SIZE_T)myData->NumRows) * x;
				const BYTE* pSrcSlice = static_cast<const BYTE*>(IndexData.pData) + ((SIZE_T)myData->layouts.Footprint.RowPitch * (SIZE_T)myData->NumRows) * (LONG_PTR)x;//TODO: what is the LONG_PTR cast for?

				for (int y = 0; y < myData->NumRows; y++)
				{
					memcpy(
						pDestSlice + myData->layouts.Footprint.RowPitch * (UINT64)y,
						pSrcSlice + IndexData.RowPitch * (LONG_PTR)y, //again, the mysterious cast
						static_cast<SIZE_T>(myData->RowSizesInBytes));
				}
			}

			iBufferUploadHeap->Unmap(0, nullptr);

			g_CommandList->CopyBufferRegion(indexBuffer.Get(), 0, iBufferUploadHeap.Get(), myData->layouts.Offset, myData->layouts.Footprint.Width);

			HeapFree(GetProcessHeap(), 0, myData);

			/*
			* unknown functions used:
			* g_Device->GetCopyableFootprints
			* vBufferUploadHeap->Map
			* vBufferUploadHeap->Unmap
			*/
		}

		D3D12_RESOURCE_BARRIER barrier = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition{
				.pResource = indexBuffer.Get(),
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
				.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			}
		};
		g_CommandList->ResourceBarrier(1, &barrier);
		g_CommandList->Close();
		ID3D12CommandList* ppCommandLists[] = { g_CommandList.Get() };
		g_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		g_FenceValue++;
		g_CommandQueue->Signal(g_Fence.Get(), g_FenceValue);

	}
	vertexBufferView = {
		.BufferLocation = vertexBuffer->GetGPUVirtualAddress(),
		.SizeInBytes = (UINT)vBufferSize,
		.StrideInBytes = sizeof(Vertex)
	};

	indexBufferView = {
		.BufferLocation = indexBuffer->GetGPUVirtualAddress(),
		.SizeInBytes = (UINT)iBufferSize,
		.Format = DXGI_FORMAT_R32_UINT
	};

	viewport = {
		.TopLeftX = 0,
		.TopLeftY = 0,
		.Width = (FLOAT)g_ClientWidth,
		.Height = (FLOAT)g_ClientHeight,
		.MinDepth = 0.0F,
		.MaxDepth = 1.0F
	};

	//viewport2 = {
	//	.TopLeftX = (FLOAT)g_ClientWidth / 2 - 100,
	//	.TopLeftY = (FLOAT)g_ClientHeight / 2 - 100,
	//	.Width = (FLOAT)200,
	//	.Height = (FLOAT)200,
	//	.MinDepth = 0.0F,
	//	.MaxDepth = 1.0F
	//};

	scissorRect = {
		.left = 0,
		.top = 0,
		.right = (LONG)g_ClientWidth,
		.bottom = (LONG)g_ClientHeight
	};


	SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)&WndProc_postinit);

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		{
			memcpy(cbColorMultiplierGPUAddress[g_SwapChain->GetCurrentBackBufferIndex()], &cbColorMultiplierData, sizeof(cbColorMultiplierData));
		}
		if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	g_FenceValue++;
	THROW_ON_FAIL(g_CommandQueue->Signal(g_Fence.Get(), g_FenceValue));

	if (g_Fence->GetCompletedValue() < g_FenceValue)
	{
		THROW_ON_FAIL(g_Fence->SetEventOnCompletion(g_FenceValue, g_FenceEvent));
		WaitForSingleObject(g_FenceEvent, INFINITE);
	}

	CloseHandle(g_FenceEvent);
	return 0;
}

static auto t0 = fpsClock.now();
auto deltaTime = t0 - t0;

LRESULT CALLBACK WndProc_preinit(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_DESTROY)
		PostQuitMessage(0);
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK WndProc_postinit(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
	{
		//keep track of fps
		//TODO: this is probably broken
		frameCounter++;
		auto t1 = fpsClock.now();
		deltaTime = t1 - t0;
		t0 = t1;
		if constexpr (showFPS)
		{
			elapsedSeconds += deltaTime.count() * 1e-9;
			if (elapsedSeconds > .25)
			{

				system("cls");
				std::cout << "FPS: " << (frameCounter / elapsedSeconds); //temporary, until I can write this to the screen
				frameCounter = 0;
				elapsedSeconds = 0.0;
			}
		}
	}

	{
		const UINT currentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

		g_CommandAllocators[currentBackBufferIndex]->Reset();

		//record and submit graphics commands
		{
			D3D12_RESOURCE_BARRIER barrier1
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
				{
					.pResource = g_BackBuffers[currentBackBufferIndex].Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
					.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
				}
			};

			temp += 0.3 * deltaTime.count() * 1e-9;
			const FLOAT clearColor1[] = { sin(temp), cos(temp), .5f, 1.0f };
			const FLOAT clearColor2[] = { cos(temp), .5f, sin(temp), 1.0f };



			D3D12_RESOURCE_BARRIER barrier2
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
				{
					.pResource = g_BackBuffers[currentBackBufferIndex].Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
					.StateAfter = D3D12_RESOURCE_STATE_PRESENT
				}
			};


			D3D12_RESOURCE_BARRIER resolutionBarrier1
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
				{
					.pResource = g_IntermediateBuffers[currentBackBufferIndex].Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
					.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE
				}
			};

			D3D12_RESOURCE_BARRIER resolutionBarrier2
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
				{
					.pResource = g_IntermediateBuffers[currentBackBufferIndex].Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
					.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
				}
			};


			D3D12_RESOURCE_BARRIER resolutionDestBarrier1
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
				{
					.pResource = g_BackBuffers[currentBackBufferIndex].Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
					.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST
				}
			};

			D3D12_RESOURCE_BARRIER resolutionDestBarrier2
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
				{
					.pResource = g_BackBuffers[currentBackBufferIndex].Get(),
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST,
					.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
				}
			};

			D3D12_CPU_DESCRIPTOR_HANDLE rtv1 = {
					.ptr = (SIZE_T)(
							(INT64)g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr +
							(INT64)currentBackBufferIndex *
							(INT64)g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
						)
			};
			D3D12_CPU_DESCRIPTOR_HANDLE rtv2 = {
					.ptr = (SIZE_T)(
							(INT64)g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr +
							(INT64)g_NumFrames *
							(INT64)g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) +
							(INT64)currentBackBufferIndex *
							(INT64)g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
						)
			};
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {
				.ptr = (SIZE_T)dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr
			};

			//at long last, the actual graphics commands:
			//g_CommandList->SetPipelineState(isWireframe ? pipelineStateObject2.Get() : pipelineStateObject.Get());
			g_CommandList->Reset(g_CommandAllocators[currentBackBufferIndex].Get(), isWireframe ? pipelineStateObject.Get() : pipelineStateObject2.Get());
			//cbIterations.numIterations = 1;
			{
				POINT p;
				GetCursorPos(&p);

				cbColorMultiplierData.mousePos.x = (p.x / (g_ClientWidth / 2.)) - 2;
				cbColorMultiplierData.mousePos.y = (p.y / (g_ClientHeight / 2.)) - 2;
			}

			// copy our ConstantBuffer instance to the mapped constant buffer resource


			memcpy(cbColorMultiplierGPUAddress[currentBackBufferIndex], &cbColorMultiplierData, sizeof(cbColorMultiplierData));
			memcpy(cbColorMultiplierGPUAddress[currentBackBufferIndex] + sizeof(cbColorMultiplierData), &cbIterations, sizeof(cbIterations));
			g_CommandList->SetPipelineState(pipelineStateObject_msaa.Get());
			g_CommandList->ResourceBarrier(1, &barrier1);
			g_CommandList->SetGraphicsRootSignature(rootSignature.Get());
			g_CommandList->OMSetRenderTargets(1, &rtv1, FALSE, NULL);
			g_CommandList->ClearRenderTargetView(rtv1, clearColor1, 0, nullptr);
			//g_CommandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// set constant buffer descriptor heap
			ID3D12DescriptorHeap* descriptorHeaps[] = { mainDescriptorHeap[currentBackBufferIndex].Get() };
			g_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			// set the root descriptor table 0 to the constant buffer descriptor heap
			g_CommandList->SetGraphicsRootDescriptorTable(0, mainDescriptorHeap[currentBackBufferIndex]->GetGPUDescriptorHandleForHeapStart());

			g_CommandList->RSSetViewports(1, &viewport);
			g_CommandList->RSSetScissorRects(1, &scissorRect);
			g_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			g_CommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			g_CommandList->IASetIndexBuffer(&indexBufferView);
			g_CommandList->DrawIndexedInstanced(6, 1, 6, 0, 0);

			g_CommandList->SetPipelineState(pipelineStateObject2.Get());
			g_CommandList->OMSetRenderTargets(1, &rtv2, FALSE, NULL);
			//TODO: convert intermediate buffer state to D3D12_RESOURCE_STATE_RESOLVE_SOURCE
			g_CommandList->ResourceBarrier(1, &resolutionBarrier1);
			g_CommandList->ResourceBarrier(1, &resolutionDestBarrier1);
			g_CommandList->ResolveSubresource(g_BackBuffers[currentBackBufferIndex].Get(), 0, g_IntermediateBuffers[currentBackBufferIndex].Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
			//g_CommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
			g_CommandList->ResourceBarrier(1, &resolutionBarrier2);
			g_CommandList->ResourceBarrier(1, &resolutionDestBarrier2);
			g_CommandList->ResourceBarrier(1, &barrier2);

			THROW_ON_FAIL(g_CommandList->Close());

			ID3D12CommandList* const commandLists[] = { g_CommandList.Get() };

			g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);//submit the command list for execution
		}

		g_FenceValue++;
		THROW_ON_FAIL(g_CommandQueue->Signal(g_Fence.Get(), g_FenceValue));

		g_FrameFenceValues[currentBackBufferIndex] = g_FenceValue;

		{
			const UINT syncInterval = g_VSync ? 1 : 0;
			const UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;

			g_SwapChain->Present(syncInterval, presentFlags);
		}

		if (g_Fence->GetCompletedValue() < g_FrameFenceValues[currentBackBufferIndex])
		{
			HANDLE fenceEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, FENCE_NAME);
			if (fenceEvent == NULL)
				throw std::exception();
			THROW_ON_FAIL(g_Fence->SetEventOnCompletion(g_FrameFenceValues[currentBackBufferIndex], fenceEvent));
			WaitForSingleObject(fenceEvent, INFINITE);
		}
	}
	break;
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

		switch (wParam)
		{
		case 'Q'://zoom out
			cbColorMultiplierData.colorMultiplier.z *= 1.1f;
			break;
		case 'E'://zoom in
			cbColorMultiplierData.colorMultiplier.z *= .9;
			break;
		case 'Z':
			isWireframe = !isWireframe;
			break;
		case 'A':
			cbColorMultiplierData.colorMultiplier.x -= .1 * cbColorMultiplierData.colorMultiplier.z;
			break;
		case 'D':
			cbColorMultiplierData.colorMultiplier.x += .1 * cbColorMultiplierData.colorMultiplier.z;
			break;
		case 'S':
			cbColorMultiplierData.colorMultiplier.y -= .1 * cbColorMultiplierData.colorMultiplier.z;
			break;
		case 'W':
			cbColorMultiplierData.colorMultiplier.y += .1 * cbColorMultiplierData.colorMultiplier.z;
			break;
		case 'I':
			break;
		case 'K':
			break;
		case 'J':
			break;
		case 'L':
			break;
		case 'V':
			g_VSync = !g_VSync;
			break;
		case 'T':
			cbIterations.numIterations += 1;
			break;
		case 'Y':
			cbIterations.numIterations -= 1;
			break;
		case 'G':
			cbIterations.numIterations += 100;
			break;
		case 'H':
			cbIterations.numIterations -= 100;
			break;
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;
		case VK_RETURN:
		case VK_F11:
			if (alt)
			{
				if (g_Fullscreen = !g_Fullscreen)
				{
					// Store the current window dimensions so they can be restored 
					// when switching out of fullscreen state.
					GetWindowRect(hwnd, &g_WindowRect);

					// Set the window style to a borderless window so the client area fills
					// the entire screen.
					UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

					SetWindowLongW(hwnd, GWL_STYLE, windowStyle);

					// Query the name of the nearest display device for the window.
					// This is required to set the fullscreen dimensions of the window
					// when using a multi-monitor setup.
					HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

					MONITORINFOEXW monitorInfo{ {.cbSize = sizeof(MONITORINFOEXW)} };

					GetMonitorInfoW(hMonitor, &monitorInfo);

					SetWindowPos(hwnd, HWND_TOPMOST,
						monitorInfo.rcMonitor.left,
						monitorInfo.rcMonitor.top,
						monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
						monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
						SWP_FRAMECHANGED | SWP_NOACTIVATE);

					ShowWindow(hwnd, SW_MAXIMIZE);
				}
				else
				{
					// Restore all the window decorators.
					SetWindowLongW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

					SetWindowPos(hwnd, HWND_NOTOPMOST,
						g_WindowRect.left,
						g_WindowRect.top,
						g_WindowRect.right - g_WindowRect.left,
						g_WindowRect.bottom - g_WindowRect.top,
						SWP_FRAMECHANGED | SWP_NOACTIVATE);

					ShowWindow(hwnd, SW_NORMAL);
				}

			}
			break;
		}
	}
	break;
	// The default window procedure will play a system notification sound 
	// when pressing the Alt+Enter keyboard combination if this message is 
	// not handled.
	case WM_SYSCHAR:
		break;
	case WM_SIZE:
	{
		RECT clientRect = {};
		GetClientRect(hwnd, &clientRect);

		int width = clientRect.right - clientRect.left;
		int height = clientRect.bottom - clientRect.top;

		if (g_ClientWidth != width || g_ClientHeight != height)
		{
			// Don't allow 0 size swap chain back buffers.
			g_ClientWidth = std::max(1ui32, (uint32_t)width);
			g_ClientHeight = std::max(1ui32, (uint32_t)height);

			// Flush the GPU queue to make sure the swap chain's back buffers
			// are not being referenced by an in-flight command list.
			THROW_ON_FAIL(g_CommandQueue->Signal(g_Fence.Get(), ++g_FenceValue));


			if (g_Fence->GetCompletedValue() < g_FenceValue)
			{
				HANDLE fenceEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, FENCE_NAME);
				THROW_ON_FAIL(g_Fence->SetEventOnCompletion(g_FenceValue, fenceEvent));
				WaitForSingleObject(fenceEvent, static_cast<DWORD>(std::chrono::milliseconds::max().count()));
			}

			for (int i = 0; i < g_NumFrames; i++)
			{
				// Any references to the back buffers must be released
				// before the swap chain can be resized.
				g_BackBuffers[i].Reset();
				g_FrameFenceValues[i] = g_FrameFenceValues[g_SwapChain->GetCurrentBackBufferIndex()];
			}

			DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
			THROW_ON_FAIL(g_SwapChain->GetDesc(&swapChainDesc));
			THROW_ON_FAIL(g_SwapChain->ResizeBuffers(g_NumFrames, g_ClientWidth, g_ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
			const INT64 incrementSize = (INT64)g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			for (int i = 0; i < g_NumFrames; i++)
			{
				ComPtr<ID3D12Resource> backBuffer;
				THROW_ON_FAIL(g_SwapChain->GetBuffer(i, __uuidof(ID3D12Resource), &backBuffer));

				g_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

				g_BackBuffers[i] = backBuffer;
				rtvHandle.ptr = (SIZE_T)((INT64)rtvHandle.ptr + incrementSize);
			}
		}
		viewport = {
			.TopLeftX = 0,
			.TopLeftY = 0,
			.Width = (FLOAT)g_ClientWidth,
			.Height = (FLOAT)g_ClientHeight,
			.MinDepth = 0.0F,
			.MaxDepth = 1.0F
		};
		//std::cout << g_ClientWidth << ", " << g_ClientHeight << std::endl;

		//viewport2 = {
		//		.TopLeftX = 0,
		//		.TopLeftY = 0,
		//		.Width = (FLOAT)g_ClientWidth,
		//		.Height = (FLOAT)g_ClientHeight,
		//		.MinDepth = 0.0F,
		//		.MaxDepth = 1.0F
		//};

		scissorRect = {
			.left = 0,
			.top = 0,
			.right = (LONG)g_ClientWidth,
			.bottom = (LONG)g_ClientHeight
		};
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}

	return 0;
}

#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("CreateWindow")