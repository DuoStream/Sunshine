/**
 * @file src/platform/windows/display_base.cpp
 * @brief Definitions for the Windows display base code.
 */
#include <cmath>
#include <initguid.h>
#include <thread>

#include <boost/algorithm/string/join.hpp>
#include <boost/process.hpp>
#include <shtypes.h>
#include <shellscalingapi.h>
#include <windows.h>

// We have to include boost/process.hpp before display.h due to WinSock.h,
// but that prevents the definition of NTSTATUS so we must define it ourself.
typedef long NTSTATUS;

#include "display.h"
#include "misc.h"
#include "src/config.h"
#include "src/logging.h"
#include "src/platform/common.h"
#include "src/video.h"

namespace platf {
  using namespace std::literals;
}
namespace platf::dxgi {
  namespace bp = boost::process;

  float GetPrimaryMonitorScale(PRECT desktopRectangle)
  {
    float scale = 1.0f;
    const POINT ptZero = { 0, 0 };
    HMONITOR monitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
    if (monitor != NULL)
    {
      MONITORINFOEX monitorInfo = {0};
      monitorInfo.cbSize = sizeof(monitorInfo);
      if (GetMonitorInfoA(monitor, (LPMONITORINFO)&monitorInfo))
      {
        DEVMODE deviceInfo = {0};
        deviceInfo.dmSize = sizeof(deviceInfo);
        if (EnumDisplaySettingsA(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &deviceInfo))
        {
          scale = (float)deviceInfo.dmPelsWidth / (float)(desktopRectangle->right - desktopRectangle->left);
          float value = (int)(scale * 100 + .5);
          scale = (float)value / 100;
        }
      }
    }
    return scale;
  }

  void duplication_t::map_shared_buffer(ID3D11Device* baseDevice) {
    // We haven't opened the shared buffer handle yet
    if (sharedBufferHandle == NULL)
    {
      // Open the shared buffer handle
      char sharedBufferName[128];
      DWORD sessionId;
      if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
      {
        sprintf(sharedBufferName, "Global\\RdpIddCaptureBuffer%lu", sessionId);
      }
      else
      {
        sprintf(sharedBufferName, "Global\\RdpIddCaptureBuffer");
      }
      sharedBufferHandle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, sharedBufferName);

      // We managed to open the shared buffer handle
      if (sharedBufferHandle != NULL)
      {
        // Map the shared buffer
        sharedBuffer = (PRdpIddCaptureBuffer)MapViewOfFile(sharedBufferHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(RdpIddCaptureBuffer));
      }
    }

    // We need to re-duplicate the shared framebuffer handle
    if (sharedBuffer != NULL && sharedBuffer->WUDFHostProcessId != 0 && sharedBuffer->WUDFHostFrameBufferHandle != wudfFrameBufferHandle && frameBufferHandle != NULL)
    {
      // Close the previous shared framebuffer handle
      CloseHandle(frameBufferHandle);

      // Reset the handle
      frameBufferHandle = NULL;
    }

    // It's time to open the shared framebuffer texture
    if (sharedBuffer != NULL && sharedBuffer->WUDFHostProcessId != 0 && sharedBuffer->WUDFHostFrameBufferHandle != NULL && frameBufferHandle == NULL)
    {
      // Open the wudfhost.exe process
      HANDLE wudfHostProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, sharedBuffer->WUDFHostProcessId);
      if (wudfHostProcess != NULL)
      {
        // Open a true handle to this sunshine.exe process
        HANDLE sunshineProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
        if (sunshineProcess != NULL)
        {
          // Keep track of the wudfhost.exe-sided handle
          wudfFrameBufferHandle = sharedBuffer->WUDFHostFrameBufferHandle;

          // Duplicate the handle into the sunshine process
          if (DuplicateHandle(wudfHostProcess, wudfFrameBufferHandle, sunshineProcess, &frameBufferHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
          {
            // We have an existing framebuffer texture we need to release
            if (frameBufferTexture != NULL)
            {
              // Release the framebuffer texture
              frameBufferTexture->Release();

              // And NULL its reference
              frameBufferTexture = NULL;
            }
          }

          // Close the sunshine.exe process handle
          CloseHandle(sunshineProcess);
        }

        // Close the wudfhost.exe process handle
        CloseHandle(wudfHostProcess);
      }
    }

    // We can and need to re-open the shared texture
    if ((frameBufferTexture == NULL || baseDevice != frameReaderDevice) && frameBufferHandle != NULL)
    {
      // Reset the object
      reset();

      // Keep track of the current base device
      frameReaderDevice = baseDevice;

      // Query the ID3D11Device1 interface we need to open shared resources
      ID3D11Device1* d3d11Device1 = NULL;
      if (SUCCEEDED(frameReaderDevice->QueryInterface(&d3d11Device1)))
      {
        // Open the shared framebuffer texture
        d3d11Device1->OpenSharedResource1(frameBufferHandle, __uuidof(ID3D11Texture2D), (void**)&frameBufferTexture);

        // Release the D3D11Device1 interface
        d3d11Device1->Release();
      }
    }
  }

  capture_e duplication_t::iddblt(ID3D11Device* baseDevice, ID3D11Resource** texture)
  {
    // The capture result
    capture_e result = capture_e::timeout;

    // Map the shared buffer
    map_shared_buffer(baseDevice);

    // We have a new frame to return
    if (baseDevice == frameReaderDevice && sharedBuffer != NULL && sharedBuffer->CapturedFrames != encodedFrames && frameBufferTexture != NULL)
    {
      // Read the number of captured frames
      SIZE_T capturedFrames = sharedBuffer->CapturedFrames;

      // Set the internal encoded frame count
      encodedFrames = capturedFrames;

      // We need to adjust the mode
      if (sharedBuffer->Mode.Width != mode.Width || sharedBuffer->Mode.Height != mode.Height || sharedBuffer->Mode.RefreshRate != mode.RefreshRate || (!sharedBuffer->Mode.IsIddCx19Based && sharedBuffer->Mode.HDR != mode.HDR))
      {
        // Log the current mode
        BOOST_LOG(info) << "The current mode is " << sharedBuffer->Mode.Width << "x" << sharedBuffer->Mode.Height << "@" << sharedBuffer->Mode.RefreshRate << " using the " << (sharedBuffer->Mode.HDR ? "HDR" : "SDR") << " colorspace";

        // Log the wanted mode
        BOOST_LOG(info) << "The requested mode is " << mode.Width << "x" << mode.Height << "@" << mode.RefreshRate << " using the " << (!sharedBuffer->Mode.IsIddCx19Based && mode.HDR ? "HDR" : "SDR") << " colorspace";

        // Copy over the parameters
        sharedBuffer->Mode.Width = mode.Width;
        sharedBuffer->Mode.Height = mode.Height;
        sharedBuffer->Mode.RefreshRate = mode.RefreshRate;
        sharedBuffer->Mode.HDR = !sharedBuffer->Mode.IsIddCx19Based && mode.HDR;

        // Request a mode change
        sharedBuffer->ModeChangePending = TRUE;

        // We aren't encoding this frame
        resumeSwapChain();
      }

      // We're in the right mode already
      else
      {
        // Get the texture description
        D3D11_TEXTURE2D_DESC desc;
        frameBufferTexture->GetDesc(&desc);

        // The desktop resolution or bit-depth changed
        if (frameBufferTextureDescription.Format != DXGI_FORMAT_UNKNOWN && (frameBufferTextureDescription.Width != desc.Width || frameBufferTextureDescription.Height != desc.Height || frameBufferTextureDescription.Format != desc.Format))
        {
          // Log the format change
          BOOST_LOG(info) << "Switching mode to " << desc.Width << "x" << desc.Height << " (Format: " << desc.Format << ")";

          // We aren't encoding this frame
          resumeSwapChain();

          // Re-initialize the capture
          result = capture_e::reinit;
        }

        // The desktop hasn't changed
        else
        {
          // Return the shared texture
          *texture = (ID3D11Resource*)frameBufferTexture;

          // And let the caller know he can access it
          result = capture_e::ok;
        }

        // Keep track of the texture description
        frameBufferTextureDescription = desc;
      }
    }

    // Return the capture result
    return result;
  }

  void duplication_t::resumeSwapChain()
  {
    // We have access to the shared buffer
    if (sharedBuffer != NULL)
    {
      // Let the driver know how far along we are with processing
      sharedBuffer->EncodedFrames = encodedFrames;
    }
  }

  bool duplication_t::test(IDXGIAdapter1* baseAdapter1)
  {
    // The test result
    bool result = false;

    // Query the IDXGIAdapter interface
    IDXGIAdapter* baseAdapter = NULL;
    if (SUCCEEDED(baseAdapter1->QueryInterface(IID_IDXGIAdapter, (void **)&baseAdapter)))
    {
      // The accepted feature levels
      D3D_FEATURE_LEVEL featureLevels[] {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
      };

      // Create a temporary ID3D11Device
      ID3D11Device* baseDevice = nullptr;
      if (SUCCEEDED(D3D11CreateDevice(baseAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_FLAGS, featureLevels, sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION, &baseDevice, nullptr, nullptr)))
      {
        // Test the ID3D11Device
        result = test(baseDevice);

        // Release the ID3D11Device
        baseDevice->Release();

        // And NULL its reference
        baseDevice = NULL;
      }

      // Release the IDXGIAdapter interface
      baseAdapter->Release();

      // And NULL its reference
      baseAdapter = NULL;
    }

    // Return the result
    return result;
  }

  bool duplication_t::test(ID3D11Device* baseDevice)
  {
    // Reset the object
    reset();

    // Map the shared buffer
    map_shared_buffer(baseDevice);

    // Check if the ID3D11Device passed the capture test
    bool result = baseDevice == frameReaderDevice && sharedBuffer != NULL && frameBufferTexture != NULL;

    // Return the test result
    return result;
  }

  void duplication_t::reset() {
    // We're holding onto a shared texture
    if (frameBufferTexture != NULL)
    {
      // Release the texture
      frameBufferTexture->Release();

      // And NULL its reference
      frameBufferTexture = NULL;
    }
  }

  void duplication_t::changeMode(const ::video::config_t &config)
  {
    // Log the call
    BOOST_LOG(info) << "Requesting mode change to " << config.width << "x" << config.height << "@" << config.framerate << " using the " << (config.dynamicRange != 0 ? "HDR" : "SDR") << " colorspace";

    // Set the mode parameters
    mode.Width = config.width;
    mode.Height = config.height;
    mode.RefreshRate = config.framerate;
    mode.HDR = config.dynamicRange != 0;
  }

  capture_e
  display_base_t::capture(const push_captured_image_cb_t &push_captured_image_cb, const pull_free_image_cb_t &pull_free_image_cb, bool *cursor) {
    auto adjust_client_frame_rate = [&]() -> DXGI_RATIONAL {
      // Adjust capture frame interval when display refresh rate is not integral but very close to requested fps.
      if (display_refresh_rate.Denominator > 1) {
        DXGI_RATIONAL candidate = display_refresh_rate;
        if (client_frame_rate % display_refresh_rate_rounded == 0) {
          candidate.Numerator *= client_frame_rate / display_refresh_rate_rounded;
        }
        else if (display_refresh_rate_rounded % client_frame_rate == 0) {
          candidate.Denominator *= display_refresh_rate_rounded / client_frame_rate;
        }
        double candidate_rate = (double) candidate.Numerator / candidate.Denominator;
        // Can only decrease requested fps, otherwise client may start accumulating frames and suffer increased latency.
        if (client_frame_rate > candidate_rate && candidate_rate / client_frame_rate > 0.99) {
          BOOST_LOG(info) << "Adjusted capture rate to " << candidate_rate << "fps to better match display";
          return candidate;
        }
      }

      return { (uint32_t) client_frame_rate, 1 };
    };

    DXGI_RATIONAL client_frame_rate_adjusted = adjust_client_frame_rate();
    std::optional<std::chrono::steady_clock::time_point> frame_pacing_group_start;
    uint32_t frame_pacing_group_frames = 0;

    // Keep the display awake during capture. If the display goes to sleep during
    // capture, best case is that capture stops until it powers back on. However,
    // worst case it will trigger us to reinit DD, waking the display back up in
    // a neverending cycle of waking and sleeping the display of an idle machine.
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
    auto clear_display_required = util::fail_guard([]() {
      SetThreadExecutionState(ES_CONTINUOUS);
    });

    sleep_overshoot_logger.reset();

    while (true) {
      // This will return false if the HDR state changes or for any number of other
      // display or GPU changes. We should reinit to examine the updated state of
      // the display subsystem. It is recommended to call this once per frame.
      if (!factory->IsCurrent()) {
        return platf::capture_e::reinit;
      }

      platf::capture_e status = capture_e::ok;
      std::shared_ptr<img_t> img_out;

      // Try to continue frame pacing group, snapshot() is called with zero timeout after waiting for client frame interval
      if (frame_pacing_group_start) {
        const uint32_t seconds = (uint64_t) frame_pacing_group_frames * client_frame_rate_adjusted.Denominator / client_frame_rate_adjusted.Numerator;
        const uint32_t remainder = (uint64_t) frame_pacing_group_frames * client_frame_rate_adjusted.Denominator % client_frame_rate_adjusted.Numerator;
        const auto sleep_target = *frame_pacing_group_start +
                                  std::chrono::nanoseconds(1s) * seconds +
                                  std::chrono::nanoseconds(1s) * remainder / client_frame_rate_adjusted.Numerator;
        const auto sleep_period = sleep_target - std::chrono::steady_clock::now();

        if (sleep_period <= 0ns) {
          // We missed next frame time, invalidating current frame pacing group
          frame_pacing_group_start = std::nullopt;
          frame_pacing_group_frames = 0;
          status = capture_e::timeout;
        }
        else {
          timer->sleep_for(sleep_period);
          sleep_overshoot_logger.first_point(sleep_target);
          sleep_overshoot_logger.second_point_now_and_log();

          status = snapshot(pull_free_image_cb, img_out, 0ms, *cursor);

          if (status == capture_e::ok && img_out) {
            frame_pacing_group_frames += 1;
          }
          else {
            frame_pacing_group_start = std::nullopt;
            frame_pacing_group_frames = 0;
          }
        }
      }

      // Start new frame pacing group if necessary, snapshot() is called with non-zero timeout
      if (status == capture_e::timeout || (status == capture_e::ok && !frame_pacing_group_start)) {
        status = snapshot(pull_free_image_cb, img_out, 1000ms, *cursor);

        if (status == capture_e::ok && img_out) {
          frame_pacing_group_start = img_out->frame_timestamp;

          if (!frame_pacing_group_start) {
            BOOST_LOG(warning) << "snapshot() provided image without timestamp";
            frame_pacing_group_start = std::chrono::steady_clock::now();
          }

          frame_pacing_group_frames = 1;
        }
      }

      switch (status) {
        case platf::capture_e::reinit:
        case platf::capture_e::error:
        case platf::capture_e::interrupted:
          return status;
        case platf::capture_e::timeout:
          if (!push_captured_image_cb(std::move(img_out), false)) {
            return capture_e::ok;
          }
          break;
        case platf::capture_e::ok:
          if (!push_captured_image_cb(std::move(img_out), true)) {
            return capture_e::ok;
          }
          break;
        default:
          BOOST_LOG(error) << "Unrecognized capture status ["sv << (int) status << ']';
          return status;
      }
    }

    return capture_e::ok;
  }

  bool
  set_gpu_preference_on_self(int preference) {
    // The GPU preferences key uses app path as the value name.
    WCHAR sunshine_path[MAX_PATH];
    GetModuleFileNameW(NULL, sunshine_path, ARRAYSIZE(sunshine_path));

    WCHAR value_data[128];
    swprintf_s(value_data, L"GpuPreference=%d;", preference);

    auto status = RegSetKeyValueW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\DirectX\\UserGpuPreferences",
      sunshine_path,
      REG_SZ,
      value_data,
      (wcslen(value_data) + 1) * sizeof(WCHAR));
    if (status != ERROR_SUCCESS) {
      BOOST_LOG(error) << "Failed to set GPU preference: "sv << status;
      return false;
    }

    BOOST_LOG(info) << "Set GPU preference: "sv << preference;
    return true;
  }

  bool
  validate_and_test_gpu_preference(const std::string &display_name, bool verify_frame_capture) {
    std::string cmd = "tools\\ddprobe.exe";

    // We start at 1 because 0 is automatic selection which can be overridden by
    // the GPU driver control panel options. Since ddprobe.exe can have different
    // GPU driver overrides than Sunshine.exe, we want to avoid a scenario where
    // autoselection might work for ddprobe.exe but not for us.
    for (int i = 1; i < 5; i++) {
      // Run the probe tool. It returns the status of DuplicateOutput().
      //
      // Arg format: [GPU preference] [Display name] [--verify-frame-capture]
      HRESULT result;
      std::vector<std::string> args = { std::to_string(i), display_name };
      try {
        if (verify_frame_capture) {
          args.emplace_back("--verify-frame-capture");
        }
        result = bp::system(cmd, bp::args(args), bp::std_out > bp::null, bp::std_err > bp::null);
      }
      catch (bp::process_error &e) {
        BOOST_LOG(error) << "Failed to start ddprobe.exe: "sv << e.what();
        return false;
      }

      BOOST_LOG(info) << "ddprobe.exe " << boost::algorithm::join(args, " ") << " returned 0x"
                      << util::hex(result).to_string_view();

      // E_ACCESSDENIED can happen at the login screen. If we get this error,
      // we know capture would have been supported, because DXGI_ERROR_UNSUPPORTED
      // would have been raised first if it wasn't.
      if (result == S_OK || result == E_ACCESSDENIED) {
        // We found a working GPU preference, so set ourselves to use that.
        if (set_gpu_preference_on_self(i)) {
          return true;
        }
        else {
          return false;
        }
      }
    }

    // If no valid configuration was found, return false
    return false;
  }

  int
  GetPrimaryMonitorRefreshRate(int defaultValue) {
    // Get the primary monitor's device context
    HDC hdc = GetDC(NULL);

    // Get the refresh rate
    int refreshRate = GetDeviceCaps(hdc, VREFRESH);

    // Release the device context
    ReleaseDC(NULL, hdc);

    // Return the refresh rate
    return refreshRate;
  }

  int
  display_base_t::init(const ::video::config_t &config, const std::string &display_name) {
    std::once_flag windows_cpp_once_flag;

    std::call_once(windows_cpp_once_flag, []() {
      DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);

      typedef BOOL (*User32_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT value);

      auto user32 = LoadLibraryA("user32.dll");
      auto f = (User32_SetProcessDpiAwarenessContext) GetProcAddress(user32, "SetProcessDpiAwarenessContext");
      if (f) {
        f(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
      }

      FreeLibrary(user32);
    });

    // Get rectangle of full desktop for absolute mouse coordinates
    env_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    env_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HRESULT status;

    status = CreateDXGIFactory1(IID_IDXGIFactory1, (void **) &factory);
    if (FAILED(status)) {
      BOOST_LOG(error) << "Failed to create DXGIFactory1 [0x"sv << util::hex(status).to_string_view() << ']';
      return -1;
    }

    adapter_t::pointer adapter_p;
    for (int tries = 0; tries < 2; ++tries) {
      for (int x = 0; factory->EnumAdapters1(x, &adapter_p) != DXGI_ERROR_NOT_FOUND; ++x) {
        dxgi::adapter_t adapter_tmp { adapter_p };
        DXGI_ADAPTER_DESC1 adapter_desc;
        adapter_tmp->GetDesc1(&adapter_desc);

        dxgi::output_t::pointer output_p;
        for (int y = 0; adapter_tmp->EnumOutputs(y, &output_p) != DXGI_ERROR_NOT_FOUND; ++y) {
          dxgi::output_t output_tmp { output_p };

          DXGI_OUTPUT_DESC desc;
          output_tmp->GetDesc(&desc);

          if (desc.AttachedToDesktop) {
            output = std::move(output_tmp);

            offset_x = desc.DesktopCoordinates.left;
            offset_y = desc.DesktopCoordinates.top;
            float scale = GetPrimaryMonitorScale(&desc.DesktopCoordinates);
            width = (desc.DesktopCoordinates.right - offset_x) * scale;
            height = (desc.DesktopCoordinates.bottom - offset_y) * scale;

            display_rotation = desc.Rotation;
            if (display_rotation == DXGI_MODE_ROTATION_ROTATE90 ||
                display_rotation == DXGI_MODE_ROTATION_ROTATE270) {
              width_before_rotation = height;
              height_before_rotation = width;
            }
            else {
              width_before_rotation = width;
              height_before_rotation = height;
            }

            // left and bottom may be negative, yet absolute mouse coordinates start at 0x0
            // Ensure offset starts at 0x0
            offset_x -= GetSystemMetrics(SM_XVIRTUALSCREEN);
            offset_y -= GetSystemMetrics(SM_YVIRTUALSCREEN);

            break;
          }
        }

        if (dup.test(adapter_p)) {
          adapter = std::move(adapter_tmp);
        }

        if (output && adapter) {
          break;
        }
      }

      if (output && adapter) {
        break;
      }

      // If we made it here without finding an output, try to power on the display and retry.
      if (tries == 0) {
        SetThreadExecutionState(ES_DISPLAY_REQUIRED);
        Sleep(5000);
      }
    }

    if (!adapter) {
      BOOST_LOG(error) << "Failed to locate an adapter, restarting session"sv;
      ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0);
      return -1;
    }

    if (!output) {
      BOOST_LOG(error) << "Failed to locate an output device, restarting session"sv;
      ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0);
      return -1;
    }

    D3D_FEATURE_LEVEL featureLevels[] {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
      D3D_FEATURE_LEVEL_9_3,
      D3D_FEATURE_LEVEL_9_2,
      D3D_FEATURE_LEVEL_9_1
    };

    status = adapter->QueryInterface(IID_IDXGIAdapter, (void **) &adapter_p);
    if (FAILED(status)) {
      BOOST_LOG(error) << "Failed to query IDXGIAdapter interface"sv;
      return -1;
    }

    int d3d11createdevice_retry_count = 10;
    retry_d3d11createdevice:
    status = D3D11CreateDevice(
      adapter_p,
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr,
      D3D11_CREATE_DEVICE_FLAGS,
      featureLevels, sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL),
      D3D11_SDK_VERSION,
      &device,
      &feature_level,
      &device_ctx);

    adapter_p->Release();

    if (FAILED(status)) {
      BOOST_LOG(error) << "Failed to create D3D11 device [0x"sv << util::hex(status).to_string_view() << ']';

      if (status == E_OUTOFMEMORY && d3d11createdevice_retry_count > 0) {
        d3d11createdevice_retry_count--;
        Sleep(50);
        goto retry_d3d11createdevice;
      }

      return -1;
    }

    DXGI_ADAPTER_DESC adapter_desc;
    adapter->GetDesc(&adapter_desc);

    auto description = to_utf8(adapter_desc.Description);
    BOOST_LOG(info)
      << std::endl
      << "Device Description : " << description << std::endl
      << "Device Vendor ID   : 0x"sv << util::hex(adapter_desc.VendorId).to_string_view() << std::endl
      << "Device Device ID   : 0x"sv << util::hex(adapter_desc.DeviceId).to_string_view() << std::endl
      << "Device Video Mem   : "sv << adapter_desc.DedicatedVideoMemory / 1048576 << " MiB"sv << std::endl
      << "Device Sys Mem     : "sv << adapter_desc.DedicatedSystemMemory / 1048576 << " MiB"sv << std::endl
      << "Share Sys Mem      : "sv << adapter_desc.SharedSystemMemory / 1048576 << " MiB"sv << std::endl
      << "Feature Level      : 0x"sv << util::hex(feature_level).to_string_view() << std::endl
      << "Capture size       : "sv << width << 'x' << height << std::endl
      << "Offset             : "sv << offset_x << 'x' << offset_y << std::endl
      << "Virtual Desktop    : "sv << env_width << 'x' << env_height;

    // Bump up thread priority
    {
      const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
      TOKEN_PRIVILEGES tp;
      HANDLE token;
      LUID val;

      if (OpenProcessToken(GetCurrentProcess(), flags, &token) &&
          !!LookupPrivilegeValue(NULL, SE_INC_BASE_PRIORITY_NAME, &val)) {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = val;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL, NULL)) {
          BOOST_LOG(warning) << "Could not set privilege to increase GPU priority";
        }
      }

      CloseHandle(token);

      HMODULE gdi32 = GetModuleHandleA("GDI32");
      if (gdi32) {
        auto check_hags = [&](const LUID &adapter) -> bool {
          auto d3dkmt_open_adapter = (PD3DKMTOpenAdapterFromLuid) GetProcAddress(gdi32, "D3DKMTOpenAdapterFromLuid");
          auto d3dkmt_query_adapter_info = (PD3DKMTQueryAdapterInfo) GetProcAddress(gdi32, "D3DKMTQueryAdapterInfo");
          auto d3dkmt_close_adapter = (PD3DKMTCloseAdapter) GetProcAddress(gdi32, "D3DKMTCloseAdapter");
          if (!d3dkmt_open_adapter || !d3dkmt_query_adapter_info || !d3dkmt_close_adapter) {
            BOOST_LOG(error) << "Couldn't load d3dkmt functions from gdi32.dll to determine GPU HAGS status";
            return false;
          }

          D3DKMT_OPENADAPTERFROMLUID d3dkmt_adapter = { adapter };
          if (FAILED(d3dkmt_open_adapter(&d3dkmt_adapter))) {
            BOOST_LOG(error) << "D3DKMTOpenAdapterFromLuid() failed while trying to determine GPU HAGS status";
            return false;
          }

          bool result;

          D3DKMT_WDDM_2_7_CAPS d3dkmt_adapter_caps = {};
          D3DKMT_QUERYADAPTERINFO d3dkmt_adapter_info = {};
          d3dkmt_adapter_info.hAdapter = d3dkmt_adapter.hAdapter;
          d3dkmt_adapter_info.Type = KMTQAITYPE_WDDM_2_7_CAPS;
          d3dkmt_adapter_info.pPrivateDriverData = &d3dkmt_adapter_caps;
          d3dkmt_adapter_info.PrivateDriverDataSize = sizeof(d3dkmt_adapter_caps);

          if (SUCCEEDED(d3dkmt_query_adapter_info(&d3dkmt_adapter_info))) {
            result = d3dkmt_adapter_caps.HwSchEnabled;
          }
          else {
            BOOST_LOG(warning) << "D3DKMTQueryAdapterInfo() failed while trying to determine GPU HAGS status";
            result = false;
          }

          D3DKMT_CLOSEADAPTER d3dkmt_close_adapter_wrap = { d3dkmt_adapter.hAdapter };
          if (FAILED(d3dkmt_close_adapter(&d3dkmt_close_adapter_wrap))) {
            BOOST_LOG(error) << "D3DKMTCloseAdapter() failed while trying to determine GPU HAGS status";
          }

          return result;
        };

        auto d3dkmt_set_process_priority = (PD3DKMTSetProcessSchedulingPriorityClass) GetProcAddress(gdi32, "D3DKMTSetProcessSchedulingPriorityClass");
        if (d3dkmt_set_process_priority) {
          auto priority = D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME;
          bool hags_enabled = check_hags(adapter_desc.AdapterLuid);
          if (adapter_desc.VendorId == 0x10DE) {
            // As of 2023.07, NVIDIA driver has unfixed bug(s) where "realtime" can cause unrecoverable encoding freeze or outright driver crash
            // This issue happens more frequently with HAGS, in DX12 games or when VRAM is filled close to max capacity
            // Track OBS to see if they find better workaround or NVIDIA fixes it on their end, they seem to be in communication
            if (hags_enabled && !config::video.nv_realtime_hags) priority = D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH;
          }
          BOOST_LOG(info) << "Active GPU has HAGS " << (hags_enabled ? "enabled" : "disabled");
          BOOST_LOG(info) << "Using " << (priority == D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH ? "high" : "realtime") << " GPU priority";
          if (FAILED(d3dkmt_set_process_priority(GetCurrentProcess(), priority))) {
            BOOST_LOG(warning) << "Failed to adjust GPU priority. Please run application as administrator for optimal performance.";
          }
        }
        else {
          BOOST_LOG(error) << "Couldn't load D3DKMTSetProcessSchedulingPriorityClass function from gdi32.dll to adjust GPU priority";
        }
      }

      dxgi::dxgi_t dxgi;
      status = device->QueryInterface(IID_IDXGIDevice, (void **) &dxgi);
      if (FAILED(status)) {
        BOOST_LOG(warning) << "Failed to query DXGI interface from device [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      status = dxgi->SetGPUThreadPriority(7);
      if (FAILED(status)) {
        BOOST_LOG(warning) << "Failed to increase capture GPU thread priority. Please run application as administrator for optimal performance.";
      }
    }

    // Try to reduce latency
    {
      dxgi::dxgi1_t dxgi {};
      status = device->QueryInterface(IID_IDXGIDevice, (void **) &dxgi);
      if (FAILED(status)) {
        BOOST_LOG(error) << "Failed to query DXGI interface from device [0x"sv << util::hex(status).to_string_view() << ']';
        return -1;
      }

      status = dxgi->SetMaximumFrameLatency(1);
      if (FAILED(status)) {
        BOOST_LOG(warning) << "Failed to set maximum frame latency [0x"sv << util::hex(status).to_string_view() << ']';
      }
    }

    display_refresh_rate_rounded = GetPrimaryMonitorRefreshRate(60);
    BOOST_LOG(info) << "Display refresh rate [" << display_refresh_rate_rounded << "Hz]";

    client_frame_rate = config.framerate;
    BOOST_LOG(info) << "Requested frame rate [" << client_frame_rate << "fps]";

    dxgi::output6_t output6 {};
    status = output->QueryInterface(IID_IDXGIOutput6, (void **) &output6);
    if (SUCCEEDED(status)) {
      DXGI_OUTPUT_DESC1 desc1;
      output6->GetDesc1(&desc1);

      BOOST_LOG(info)
        << std::endl
        << "Colorspace         : "sv << colorspace_to_string(desc1.ColorSpace) << std::endl
        << "Bits Per Color     : "sv << desc1.BitsPerColor << std::endl
        << "Red Primary        : ["sv << desc1.RedPrimary[0] << ',' << desc1.RedPrimary[1] << ']' << std::endl
        << "Green Primary      : ["sv << desc1.GreenPrimary[0] << ',' << desc1.GreenPrimary[1] << ']' << std::endl
        << "Blue Primary       : ["sv << desc1.BluePrimary[0] << ',' << desc1.BluePrimary[1] << ']' << std::endl
        << "White Point        : ["sv << desc1.WhitePoint[0] << ',' << desc1.WhitePoint[1] << ']' << std::endl
        << "Min Luminance      : "sv << desc1.MinLuminance << " nits"sv << std::endl
        << "Max Luminance      : "sv << desc1.MaxLuminance << " nits"sv << std::endl
        << "Max Full Luminance : "sv << desc1.MaxFullFrameLuminance << " nits"sv;
    }

    if (!timer || !*timer) {
      BOOST_LOG(error) << "Uninitialized high precision timer";
      return -1;
    }

    // Changes the virtual monitor's mode
    dup.changeMode(config);

    return 0;
  }

  bool
  display_base_t::is_hdr() {
    dxgi::output6_t output6 {};

    auto status = output->QueryInterface(IID_IDXGIOutput6, (void **) &output6);
    if (FAILED(status)) {
      BOOST_LOG(warning) << "Failed to query IDXGIOutput6 from the output"sv;
      return false;
    }

    DXGI_OUTPUT_DESC1 desc1;
    output6->GetDesc1(&desc1);

    return desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
  }

  bool
  display_base_t::get_hdr_metadata(SS_HDR_METADATA &metadata) {
    dxgi::output6_t output6 {};

    std::memset(&metadata, 0, sizeof(metadata));

    auto status = output->QueryInterface(IID_IDXGIOutput6, (void **) &output6);
    if (FAILED(status)) {
      BOOST_LOG(warning) << "Failed to query IDXGIOutput6 from the output"sv;
      return false;
    }

    DXGI_OUTPUT_DESC1 desc1;
    output6->GetDesc1(&desc1);

    // The primaries reported here seem to correspond to scRGB (Rec. 709)
    // which we then convert to Rec 2020 in our scRGB FP16 -> PQ shader
    // prior to encoding. It's not clear to me if we're supposed to report
    // the primaries of the original colorspace or the one we've converted
    // it to, but let's just report Rec 2020 primaries and D65 white level
    // to avoid confusing clients by reporting Rec 709 primaries with a
    // Rec 2020 colorspace. It seems like most clients ignore the primaries
    // in the metadata anyway (luminance range is most important).
    desc1.RedPrimary[0] = 0.708f;
    desc1.RedPrimary[1] = 0.292f;
    desc1.GreenPrimary[0] = 0.170f;
    desc1.GreenPrimary[1] = 0.797f;
    desc1.BluePrimary[0] = 0.131f;
    desc1.BluePrimary[1] = 0.046f;
    desc1.WhitePoint[0] = 0.3127f;
    desc1.WhitePoint[1] = 0.3290f;

    metadata.displayPrimaries[0].x = desc1.RedPrimary[0] * 50000;
    metadata.displayPrimaries[0].y = desc1.RedPrimary[1] * 50000;
    metadata.displayPrimaries[1].x = desc1.GreenPrimary[0] * 50000;
    metadata.displayPrimaries[1].y = desc1.GreenPrimary[1] * 50000;
    metadata.displayPrimaries[2].x = desc1.BluePrimary[0] * 50000;
    metadata.displayPrimaries[2].y = desc1.BluePrimary[1] * 50000;

    metadata.whitePoint.x = desc1.WhitePoint[0] * 50000;
    metadata.whitePoint.y = desc1.WhitePoint[1] * 50000;

    metadata.maxDisplayLuminance = desc1.MaxLuminance;
    metadata.minDisplayLuminance = desc1.MinLuminance * 10000;

    // These are content-specific metadata parameters that this interface doesn't give us
    metadata.maxContentLightLevel = 0;
    metadata.maxFrameAverageLightLevel = 0;

    metadata.maxFullFrameLuminance = desc1.MaxFullFrameLuminance;

    return true;
  }

  const char *format_str[] = {
    "DXGI_FORMAT_UNKNOWN",
    "DXGI_FORMAT_R32G32B32A32_TYPELESS",
    "DXGI_FORMAT_R32G32B32A32_FLOAT",
    "DXGI_FORMAT_R32G32B32A32_UINT",
    "DXGI_FORMAT_R32G32B32A32_SINT",
    "DXGI_FORMAT_R32G32B32_TYPELESS",
    "DXGI_FORMAT_R32G32B32_FLOAT",
    "DXGI_FORMAT_R32G32B32_UINT",
    "DXGI_FORMAT_R32G32B32_SINT",
    "DXGI_FORMAT_R16G16B16A16_TYPELESS",
    "DXGI_FORMAT_R16G16B16A16_FLOAT",
    "DXGI_FORMAT_R16G16B16A16_UNORM",
    "DXGI_FORMAT_R16G16B16A16_UINT",
    "DXGI_FORMAT_R16G16B16A16_SNORM",
    "DXGI_FORMAT_R16G16B16A16_SINT",
    "DXGI_FORMAT_R32G32_TYPELESS",
    "DXGI_FORMAT_R32G32_FLOAT",
    "DXGI_FORMAT_R32G32_UINT",
    "DXGI_FORMAT_R32G32_SINT",
    "DXGI_FORMAT_R32G8X24_TYPELESS",
    "DXGI_FORMAT_D32_FLOAT_S8X24_UINT",
    "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS",
    "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT",
    "DXGI_FORMAT_R10G10B10A2_TYPELESS",
    "DXGI_FORMAT_R10G10B10A2_UNORM",
    "DXGI_FORMAT_R10G10B10A2_UINT",
    "DXGI_FORMAT_R11G11B10_FLOAT",
    "DXGI_FORMAT_R8G8B8A8_TYPELESS",
    "DXGI_FORMAT_R8G8B8A8_UNORM",
    "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB",
    "DXGI_FORMAT_R8G8B8A8_UINT",
    "DXGI_FORMAT_R8G8B8A8_SNORM",
    "DXGI_FORMAT_R8G8B8A8_SINT",
    "DXGI_FORMAT_R16G16_TYPELESS",
    "DXGI_FORMAT_R16G16_FLOAT",
    "DXGI_FORMAT_R16G16_UNORM",
    "DXGI_FORMAT_R16G16_UINT",
    "DXGI_FORMAT_R16G16_SNORM",
    "DXGI_FORMAT_R16G16_SINT",
    "DXGI_FORMAT_R32_TYPELESS",
    "DXGI_FORMAT_D32_FLOAT",
    "DXGI_FORMAT_R32_FLOAT",
    "DXGI_FORMAT_R32_UINT",
    "DXGI_FORMAT_R32_SINT",
    "DXGI_FORMAT_R24G8_TYPELESS",
    "DXGI_FORMAT_D24_UNORM_S8_UINT",
    "DXGI_FORMAT_R24_UNORM_X8_TYPELESS",
    "DXGI_FORMAT_X24_TYPELESS_G8_UINT",
    "DXGI_FORMAT_R8G8_TYPELESS",
    "DXGI_FORMAT_R8G8_UNORM",
    "DXGI_FORMAT_R8G8_UINT",
    "DXGI_FORMAT_R8G8_SNORM",
    "DXGI_FORMAT_R8G8_SINT",
    "DXGI_FORMAT_R16_TYPELESS",
    "DXGI_FORMAT_R16_FLOAT",
    "DXGI_FORMAT_D16_UNORM",
    "DXGI_FORMAT_R16_UNORM",
    "DXGI_FORMAT_R16_UINT",
    "DXGI_FORMAT_R16_SNORM",
    "DXGI_FORMAT_R16_SINT",
    "DXGI_FORMAT_R8_TYPELESS",
    "DXGI_FORMAT_R8_UNORM",
    "DXGI_FORMAT_R8_UINT",
    "DXGI_FORMAT_R8_SNORM",
    "DXGI_FORMAT_R8_SINT",
    "DXGI_FORMAT_A8_UNORM",
    "DXGI_FORMAT_R1_UNORM",
    "DXGI_FORMAT_R9G9B9E5_SHAREDEXP",
    "DXGI_FORMAT_R8G8_B8G8_UNORM",
    "DXGI_FORMAT_G8R8_G8B8_UNORM",
    "DXGI_FORMAT_BC1_TYPELESS",
    "DXGI_FORMAT_BC1_UNORM",
    "DXGI_FORMAT_BC1_UNORM_SRGB",
    "DXGI_FORMAT_BC2_TYPELESS",
    "DXGI_FORMAT_BC2_UNORM",
    "DXGI_FORMAT_BC2_UNORM_SRGB",
    "DXGI_FORMAT_BC3_TYPELESS",
    "DXGI_FORMAT_BC3_UNORM",
    "DXGI_FORMAT_BC3_UNORM_SRGB",
    "DXGI_FORMAT_BC4_TYPELESS",
    "DXGI_FORMAT_BC4_UNORM",
    "DXGI_FORMAT_BC4_SNORM",
    "DXGI_FORMAT_BC5_TYPELESS",
    "DXGI_FORMAT_BC5_UNORM",
    "DXGI_FORMAT_BC5_SNORM",
    "DXGI_FORMAT_B5G6R5_UNORM",
    "DXGI_FORMAT_B5G5R5A1_UNORM",
    "DXGI_FORMAT_B8G8R8A8_UNORM",
    "DXGI_FORMAT_B8G8R8X8_UNORM",
    "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM",
    "DXGI_FORMAT_B8G8R8A8_TYPELESS",
    "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB",
    "DXGI_FORMAT_B8G8R8X8_TYPELESS",
    "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB",
    "DXGI_FORMAT_BC6H_TYPELESS",
    "DXGI_FORMAT_BC6H_UF16",
    "DXGI_FORMAT_BC6H_SF16",
    "DXGI_FORMAT_BC7_TYPELESS",
    "DXGI_FORMAT_BC7_UNORM",
    "DXGI_FORMAT_BC7_UNORM_SRGB",
    "DXGI_FORMAT_AYUV",
    "DXGI_FORMAT_Y410",
    "DXGI_FORMAT_Y416",
    "DXGI_FORMAT_NV12",
    "DXGI_FORMAT_P010",
    "DXGI_FORMAT_P016",
    "DXGI_FORMAT_420_OPAQUE",
    "DXGI_FORMAT_YUY2",
    "DXGI_FORMAT_Y210",
    "DXGI_FORMAT_Y216",
    "DXGI_FORMAT_NV11",
    "DXGI_FORMAT_AI44",
    "DXGI_FORMAT_IA44",
    "DXGI_FORMAT_P8",
    "DXGI_FORMAT_A8P8",
    "DXGI_FORMAT_B4G4R4A4_UNORM",

    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

    "DXGI_FORMAT_P208",
    "DXGI_FORMAT_V208",
    "DXGI_FORMAT_V408"
  };

  const char *
  display_base_t::dxgi_format_to_string(DXGI_FORMAT format) {
    return format_str[format];
  }

  const char *
  display_base_t::colorspace_to_string(DXGI_COLOR_SPACE_TYPE type) {
    const char *type_str[] = {
      "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709",
      "DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709",
      "DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709",
      "DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020",
      "DXGI_COLOR_SPACE_RESERVED",
      "DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601",
      "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709",
      "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020",
      "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020",
      "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020",
      "DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020",
      "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020",
      "DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020",
      "DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709",
      "DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020",
      "DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020",
    };

    if (type < ARRAYSIZE(type_str)) {
      return type_str[type];
    }
    else {
      return "UNKNOWN";
    }
  }

}  // namespace platf::dxgi

namespace platf {
  std::shared_ptr<display_t>
  display(mem_type_e hwdevice_type, const std::string &display_name, const video::config_t &config) {
    if (hwdevice_type == mem_type_e::dxgi) {
      auto disp = std::make_shared<dxgi::display_vram_t>();

      if (!disp->init(config, display_name)) {
        return disp;
      }
    }
    else if (hwdevice_type == mem_type_e::system) {
      auto disp = std::make_shared<dxgi::display_ram_t>();

      if (!disp->init(config, display_name)) {
        return disp;
      }
    }

    return nullptr;
  }

  std::vector<std::string>
  display_names(mem_type_e) {
    std::vector<std::string> display_names;

    HRESULT status;

    BOOST_LOG(debug) << "Detecting monitors..."sv;

    dxgi::factory1_t factory;
    status = CreateDXGIFactory1(IID_IDXGIFactory1, (void **) &factory);
    if (FAILED(status)) {
      BOOST_LOG(error) << "Failed to create DXGIFactory1 [0x"sv << util::hex(status).to_string_view() << ']';
      return {};
    }

    dxgi::adapter_t adapter;
    for (int x = 0; factory->EnumAdapters1(x, &adapter) != DXGI_ERROR_NOT_FOUND; ++x) {
      DXGI_ADAPTER_DESC1 adapter_desc;
      adapter->GetDesc1(&adapter_desc);

      BOOST_LOG(debug)
        << std::endl
        << "====== ADAPTER ====="sv << std::endl
        << "Device Name      : "sv << to_utf8(adapter_desc.Description) << std::endl
        << "Device Vendor ID : 0x"sv << util::hex(adapter_desc.VendorId).to_string_view() << std::endl
        << "Device Device ID : 0x"sv << util::hex(adapter_desc.DeviceId).to_string_view() << std::endl
        << "Device Video Mem : "sv << adapter_desc.DedicatedVideoMemory / 1048576 << " MiB"sv << std::endl
        << "Device Sys Mem   : "sv << adapter_desc.DedicatedSystemMemory / 1048576 << " MiB"sv << std::endl
        << "Share Sys Mem    : "sv << adapter_desc.SharedSystemMemory / 1048576 << " MiB"sv << std::endl
        << std::endl
        << "    ====== OUTPUT ======"sv << std::endl;

      dxgi::output_t::pointer output_p {};
      for (int y = 0; adapter->EnumOutputs(y, &output_p) != DXGI_ERROR_NOT_FOUND; ++y) {
        dxgi::output_t output { output_p };

        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        auto device_name = to_utf8(desc.DeviceName);

        auto width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
        auto height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;

        BOOST_LOG(debug)
          << "    Output Name       : "sv << device_name << std::endl
          << "    AttachedToDesktop : "sv << (desc.AttachedToDesktop ? "yes"sv : "no"sv) << std::endl
          << "    Resolution        : "sv << width << 'x' << height << std::endl
          << std::endl;

        // Don't include the display in the list if we can't actually capture it
        if (desc.AttachedToDesktop) {
          display_names.emplace_back(std::move(device_name));
        }
      }
    }

    return display_names;
  }

  /**
   * @brief Returns if GPUs/drivers have changed since the last call to this function.
   * @return `true` if a change has occurred or if it is unknown whether a change occurred.
   */
  bool
  needs_encoder_reenumeration() {
    // Serialize access to the static DXGI factory
    static std::mutex reenumeration_state_lock;
    auto lg = std::lock_guard(reenumeration_state_lock);

    // Keep a reference to the DXGI factory, which will keep track of changes internally.
    static dxgi::factory1_t factory;
    if (!factory || !factory->IsCurrent()) {
      factory.reset();

      auto status = CreateDXGIFactory1(IID_IDXGIFactory1, (void **) &factory);
      if (FAILED(status)) {
        BOOST_LOG(error) << "Failed to create DXGIFactory1 [0x"sv << util::hex(status).to_string_view() << ']';
        factory.release();
      }

      // Always request reenumeration on the first streaming session just to ensure we
      // can deal with any initialization races that may occur when the system is booting.
      BOOST_LOG(info) << "Encoder reenumeration is required"sv;
      return true;
    }
    else {
      // The DXGI factory from last time is still current, so no encoder changes have occurred.
      return false;
    }
  }
}  // namespace platf
