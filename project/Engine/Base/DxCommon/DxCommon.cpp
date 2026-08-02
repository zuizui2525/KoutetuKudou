#include "Engine/Base/DxCommon/DxCommon.h"
#include "Engine/Base/DeferredRelease/DeferredReleaseManager.h"
#include "Engine/Base/Utils/DxUtils.h"
#include "Engine/Base/Log/Log.h"
#include "Engine/Base/Utils/StringUtility.h"
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include <iostream>
#include <thread>
#include <format>

DxCommon::~DxCommon() {
	// GPU完了を待ってから遅延解放キューを全解放
	DeferredReleaseManager::GetInstance()->ReleaseAll();

	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
}

void DxCommon::Initialize(HWND hwnd, int32_t width, int32_t height) {
	Log::Write(L" ├─ [DirectX12 初期化開始]");
	InitializeViewport(width, height);
	InitializeScissorRect(width, height);
	EnableDebugLayer();
	CreateAdapter();
	CreateDevice();
	CreateCommandObject();
	CreateSwapChain(hwnd, width, height);
	CreateRenderTargets();
	CreateDepthStencil(width, height);
	CreateFence();
	CreateDXC();
	// 遅延解放マネージャの初期化
	DeferredReleaseManager::GetInstance()->Initialize(this);
	Log::Write(L" ├─ [DirectX12 初期化完了]");
}

void DxCommon::BeginFrame() {
	isResizedThisFrame_ = false;
	backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources_[backBufferIndex_].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList_->ResourceBarrier(1, &barrier);
	
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, &dsvHandle);

	// バックバッファのクリアカラー（マジックナンバー排除のためのローカル定数）
	static constexpr FLOAT kClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f }; // 暗いグレーでクリア
	commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex_], kClearColor, 0, nullptr);
	
	// 深度バッファのクリア
	commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void DxCommon::EndFrame() {
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	if (backBufferIndex >= backBufferCount_ || !swapChainResources_[backBufferIndex]) {
		return;
	}

	if (!isResizedThisFrame_) {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		commandList_->ResourceBarrier(1, &barrier);
	}


	HRESULT hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(1, commandLists);

	// VSync同期 (enableVSync_ が true の場合は 1、そうでない場合は 0)
	UINT syncInterval = enableVSync_ ? 1 : 0;
	swapChain_->Present(syncInterval, 0);

	// 現在のフレームの実行完了フェンス値を書き込み
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
	fenceValues_[backBufferIndex] = fenceValue_;

	// 遅延解放キューのフラッシュ（GPU完了済みリソースの安全な解放）
	DeferredReleaseManager::GetInstance()->Flush();

	// 次のバックバッファインデックスを取得
	UINT nextBackBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	// 次のバックバッファの過去のGPU描画処理が完了しているかのみチェック＆非同期待機（完全並列化！）
	if (fence_->GetCompletedValue() < fenceValues_[nextBackBufferIndex]) {
		fence_->SetEventOnCompletion(fenceValues_[nextBackBufferIndex], fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	// 次のフレームで使用する CommandAllocator と CommandList をリセット
	hr = commandAllocators_[nextBackBufferIndex]->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList_->Reset(commandAllocators_[nextBackBufferIndex].Get(), nullptr);
	assert(SUCCEEDED(hr));
}


void DxCommon::PreDraw() {
	ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap_.Get() };
	commandList_->SetDescriptorHeaps(1, heaps);
	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
	commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DxCommon::DrawImGui() {
#ifdef _USEIMGUI
	ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap_.Get() };
	commandList_->SetDescriptorHeaps(1, heaps);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHandles_[swapChain_->GetCurrentBackBufferIndex()];
	commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());
#endif
}

void DxCommon::FrameStart() {
	frameStartTime_ = std::chrono::steady_clock::now();
}

// FPS固定＋経過時間更新 (アンカー目標時刻・アキュムレータ制御)
void DxCommon::FrameEnd(int targetFps) {
	using namespace std::chrono;

	constexpr int32_t kDefaultTargetFps = 60;
	constexpr int64_t kMicrosecondsPerSecond = 1000000;
	constexpr int64_t kSleepThresholdMicroseconds = 3000; // 3.0ms (OSスリープ復帰遅延によるスパイクを完全にシャットアウト)

	if (targetFps <= 0) { targetFps = kDefaultTargetFps; }

	const microseconds targetFrameTime(kMicrosecondsPerSecond / targetFps);
	auto now = steady_clock::now();

	// 初回フレーム時は現在時刻でアンカー初期化
	if (isFirstFrame_) {
		targetTime_ = now;
		isFirstFrame_ = false;
	}

	// 絶対目標時刻を正確に +16666us ずつ加算
	targetTime_ += targetFrameTime;

	// シーン切り替えや長時間ブロック発生時のアキュムレータリセット保護
	if (now > targetTime_ + targetFrameTime * 2) {
		targetTime_ = now;
	}

	// 休止できる時間がある場合は 1ms スリープで CPU 使用率を低減
	while (targetTime_ - steady_clock::now() > microseconds(kSleepThresholdMicroseconds)) {
		std::this_thread::sleep_for(milliseconds(1));
	}

	// 残りミリ秒未満は精密スピンスリープで目標時刻 targetTime_ にピッタリ固定
	while (steady_clock::now() < targetTime_) {
		// 精密吸着固定
	}

	// 経過時間で deltaTime_ を更新 (60.0FPS を超えさせない最終保証)
	auto finalTime = steady_clock::now();
	auto finalElapsed = duration_cast<microseconds>(finalTime - frameStartTime_);
	constexpr float kSecondsPerMicrosecond = 1.0f / 1000000.0f;
	float calculatedDelta = static_cast<float>(finalElapsed.count()) * kSecondsPerMicrosecond;

	// 1秒 / 60.0FPS = 0.016666667f (60.0FPS を超える微小 DeltaTime を完全にクランプ)
	constexpr float kMinDeltaTimeFor60Fps = 1.0f / 60.0f;
	deltaTime_ = (calculatedDelta < kMinDeltaTimeFor60Fps) ? kMinDeltaTimeFor60Fps : calculatedDelta;
}

void DxCommon::InitializeViewport(int32_t width, int32_t height) {
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
}

void DxCommon::InitializeScissorRect(int32_t width, int32_t height) {
	scissorRect_.left = 0;
	scissorRect_.right = width;
	scissorRect_.top = 0;
	scissorRect_.bottom = height;
}

void DxCommon::EnableDebugLayer() {
	// エラー放置ダメ絶対
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController; // デバッグ用のコントローラ
	// デバッグレイヤーのインターフェースを取得する
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
		// デバッグレイヤーを有効にする
		debugController->EnableDebugLayer();

		// 起動引数に "-gpu-validation" が指定されているかチェック
		bool enableGpuVal = false;
		int numArgs = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &numArgs);
		if (argv) {
			for (int i = 0; i < numArgs; ++i) {
				if (wcscmp(argv[i], L"-gpu-validation") == 0) {
					enableGpuVal = true;
					break;
				}
			}
			LocalFree(argv);
		}

		// コンプライアンスのチェック（指定時のみ高負荷検証を有効化）
		debugController->SetEnableGPUBasedValidation(enableGpuVal ? TRUE : FALSE);

		if (enableGpuVal) {
			Log::Write(L" ├─ 【警告】 GPU-Based Validation を有効化して起動しました（高負荷検証モード）。");
		}
	}
#endif
}

void DxCommon::CreateAdapter() {
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(useAdapter_.GetAddressOf())) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter_->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Log::Write(std::format(L" │   ├─ 【使用グラフィックス(GPU)】 {}", adapterDesc.Description));
			
			// 専用ビデオメモリ(VRAM)容量をギガバイト単位で出力
			constexpr float kBytesToGB = 1024.0f * 1024.0f * 1024.0f;
			float vramGB = static_cast<float>(adapterDesc.DedicatedVideoMemory) / kBytesToGB;
			Log::Write(std::format(L" │   ├─ 【専用ビデオメモリ(VRAM)】 {:.2f} GB", vramGB));
			break;
		}
	}
	assert(useAdapter_ != nullptr);
}

void DxCommon::CreateDevice() {
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = {
		"12.2",
		"12.1",
		"12.0"
	};

	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		HRESULT hr = D3D12CreateDevice(
			useAdapter_.Get(),
			featureLevels[i],
			IID_PPV_ARGS(device_.GetAddressOf()));
		if (SUCCEEDED(hr)) {
			Log::Write(std::format(L" │   ├─ 【機能レベル(FeatureLevel)】 {}", ConvertString(featureLevelStrings[i])));
			break;
		}
	}
	assert(device_ != nullptr);
	Log::Write(L" │   ├─ 【デバイス生成】 ID3D12Device の作成に成功しました。");

#ifdef _DEBUG
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(infoQueue_.GetAddressOf())))) {
		infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue_->PushStorageFilter(&filter);
	}
#endif
}

void DxCommon::CreateCommandObject() {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	HRESULT hr = device_->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(commandQueue_.GetAddressOf()));
	if (FAILED(hr)) {
		Log::Write(std::format(L" │   ├─ [エラー] コマンドキューの生成に失敗しました: {}", GetErrorMessage(hr)));
	}
	assert(SUCCEEDED(hr));

	for (UINT i = 0; i < backBufferCount_; ++i) {
		hr = device_->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(commandAllocators_[i].GetAddressOf()));
		if (FAILED(hr)) {
			Log::Write(std::format(L" │   ├─ [エラー] コマンドアロケータの生成に失敗しました: {}", GetErrorMessage(hr)));
		}
		assert(SUCCEEDED(hr));
	}

	hr = device_->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocators_[0].Get(),
		nullptr,
		IID_PPV_ARGS(commandList_.GetAddressOf()));
	if (FAILED(hr)) {
		Log::Write(std::format(L" │   ├─ [エラー] コマンドリストの生成に失敗しました: {}", GetErrorMessage(hr)));
	}
	assert(SUCCEEDED(hr));
}

void DxCommon::CreateSwapChain(HWND hwnd, int32_t width, int32_t height) {
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = backBufferCount_;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
		commandQueue_.Get(),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
	if (FAILED(hr)) {
		Log::Write(std::format(L" │   ├─ [エラー] スワップチェーンの生成に失敗しました: {}", GetErrorMessage(hr)));
	}
	assert(SUCCEEDED(hr));
}

void DxCommon::CreateRenderTargets() {
	rtvDescriptorHeap_ = DxUtils::CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kMaxRtvCount, false);
	srvDescriptorHeap_ = DxUtils::CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2200, true);


	HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
	assert(SUCCEEDED(hr));

	hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
	assert(SUCCEEDED(hr));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = rtvFormat_;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	UINT descriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (UINT i = 0; i < kMaxRtvCount; ++i) {
		rtvHandles_[i].ptr = rtvStartHandle.ptr + i * descriptorSize;
	}

	device_->CreateRenderTargetView(
		swapChainResources_[0].Get(),
		&rtvDesc,
		rtvHandles_[0]);

	device_->CreateRenderTargetView(
		swapChainResources_[1].Get(),
		&rtvDesc,
		rtvHandles_[1]);
}

void DxCommon::CreateDepthStencil(int32_t width, int32_t height) {
	depthStencilResource_ = DxUtils::CreateDepthStencilTextureResource(device_.Get(), width, height);
	if (!dsvDescriptorHeap_) {
		dsvDescriptorHeap_ = DxUtils::CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device_->CreateDepthStencilView(
		depthStencilResource_.Get(),
		&dsvDesc,
		dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DxCommon::CreateFence() {
	fenceValue_ = 0;
	fenceValues_[0] = 0;
	fenceValues_[1] = 0;
	HRESULT hr = device_->CreateFence(
		fenceValue_,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(fence_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	if (!fenceEvent_) {
		fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(fenceEvent_ != nullptr);
	}
}

void DxCommon::CreateDXC() {
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils_.GetAddressOf()));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

void DxCommon::ResizeSwapChain(int32_t width, int32_t height) {
	if (width <= 0 || height <= 0) return;

	Log::Write(std::format(L" ├─ [リサイズ開始] スワップチェーンをリサイズします。 旧: {} x {} -> 新: {} x {}", 
		static_cast<int32_t>(viewport_.Width), static_cast<int32_t>(viewport_.Height), width, height));

	// 1. GPUの実行完了を待機 (安全なバッファ解放のため)
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
	if (fenceEvent_ != nullptr && fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	// 2. コマンドリストが開いている場合、一度クローズする
	// (ResizeBuffers実行時にコマンドリストがオープンだと失敗することがあるため)
	HRESULT hr = commandList_->Close();
	bool wasOpen = SUCCEEDED(hr);

	// 3. スワップチェーンバッファと深度ステンシルの参照をクリア
	for (UINT i = 0; i < backBufferCount_; ++i) {
		swapChainResources_[i].Reset();
	}
	depthStencilResource_.Reset();

	// 4. バッファをリサイズ
	hr = swapChain_->ResizeBuffers(
		backBufferCount_,
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM, // スワップチェーンフォーマット
		0
	);
	assert(SUCCEEDED(hr));

	// 5. RTVの再生成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = rtvFormat_;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	UINT descriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (UINT i = 0; i < backBufferCount_; ++i) {
		rtvHandles_[i].ptr = rtvStartHandle.ptr + i * descriptorSize;
		hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
		assert(SUCCEEDED(hr));

		device_->CreateRenderTargetView(
			swapChainResources_[i].Get(),
			&rtvDesc,
			rtvHandles_[i]
		);
	}

	// 6. 深度ステンシル（DSV）の再生成
	CreateDepthStencil(width, height);

	// 7. デフォルトのビューポート・シザー矩形の再初期化
	InitializeViewport(width, height);
	InitializeScissorRect(width, height);

	// 8. コマンドリストを元のオープン状態に戻す
	if (wasOpen) {
		UINT backIdx = swapChain_->GetCurrentBackBufferIndex();
		commandAllocators_[backIdx]->Reset();
		commandList_->Reset(commandAllocators_[backIdx].Get(), nullptr);
	}

	isResizedThisFrame_ = true;
	Log::Write(L" ├─ [リサイズ完了] スワップチェーンのリサイズ処理が完了しました。");
}

void DxCommon::FlushGPU() {
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
	if (fenceEvent_ != nullptr && fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}


