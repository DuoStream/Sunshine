/**
 * @file src/platform/windows/input.cpp
 * @brief Definitions for input handling on Windows.
 */
#define WINVER 0x0A00

// platform includes
#include <Windows.h>

// standard includes
#include <cmath>
#include <thread>

// local includes
#include "keylayout.h"
#include "misc.h"
#include "src/config.h"
#include "src/globals.h"
#include "src/logging.h"
#include "src/platform/common.h"

#ifdef __MINGW32__
WINUSERAPI HSYNTHETICPOINTERDEVICE WINAPI CreateSyntheticPointerDevice(POINTER_INPUT_TYPE pointerType, ULONG maxCount, POINTER_FEEDBACK_MODE mode);
WINUSERAPI BOOL WINAPI InjectSyntheticPointerInput(HSYNTHETICPOINTERDEVICE device, CONST POINTER_TYPE_INFO *pointerInfo, UINT32 count);
WINUSERAPI VOID WINAPI DestroySyntheticPointerDevice(HSYNTHETICPOINTERDEVICE device);

/**
 * @brief The supported controller types.
 */
typedef enum _DUO_CONTROLLER_TYPE
{
  DuoControllerTypeXbox = 0,
  DuoControllerTypeDualShock4 = 1,
  DuoControllerTypeDualSense = 2,
  DuoControllerTypeDualSenseEdge = 3,
} DUO_CONTROLLER_TYPE;

/**
 * @brief The populated fields in the output report.
 */
typedef enum _SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAGS
{
  SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_MOTOR_VALID = 0x1,
  SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_MOTOR_VALID = 0x2,
  SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_TRIGGER_VALID = 0x4,
  SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_TRIGGER_VALID = 0x8,
} SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAGS;

/**
 * The power state of the DualSense controller's battery.
 */
typedef enum _POWER_STATE {
  Discharging = 0x00, // Use PowerPercent
  Charging = 0x01, // Use PowerPercent
  Complete = 0x02, // PowerPercent not valid? assume 100%?
  AbnormalVoltage = 0x0A, // PowerPercent not valid?
  AbnormalTemperature = 0x0B, // PowerPercent not valid?
  ChargingError = 0x0F  // PowerPercent not valid?
} POWER_STATE;

#pragma pack(push, 1)

/**
 * @brief The Xbox controller input report structure.
 */
typedef struct _DUO_CONTROLLER_INPUT_REPORT_XBOX
{
  UINT8 Sync : 1; // Unused
  UINT8 Guide : 1;
  UINT8 Start : 1;
  UINT8 Back : 1;

  UINT8 A : 1;
  UINT8 B : 1;
  UINT8 X : 1;
  UINT8 Y : 1;

  UINT8 DPad : 4; // 0=None, 1=North, 2=Northeast, ..., 8=Northwest (clockwise)

  UINT8 LeftBumper : 1;
  UINT8 RightBumper : 1;
  UINT8 LeftStick : 1;
  UINT8 RightStick : 1;

  UINT8 LeftTrigger; // Analog 0-255
  UINT8 RightTrigger; // Analog 0-255

  UINT16 LeftStickHorizontal; // 0 (far left) to 65535 (far right)
  UINT16 LeftStickVertical;   // 0 (far top) to 65535 (far bottom)
  UINT16 RightStickHorizontal; // 0 (far left) to 65535 (far right)
  UINT16 RightStickVertical; // 0 (far top) to 65535 (far bottom)
} DUO_CONTROLLER_INPUT_REPORT_XBOX;

/**
 * Touch finger data for a single touch point. Bit-packed per DualSense wire format (4 bytes).
 */
typedef struct _DS_TOUCH_FINGER_DATA
{
  UINT32 Index : 7;
  UINT32 NotTouching : 1;
  UINT32 FingerX : 12;
  UINT32 FingerY : 12;
} DS_TOUCH_FINGER_DATA;

/**
 * @brief Touch data block from DualSense input report (9 bytes = 2 finger records + timestamp).
 */
typedef struct _DS_TOUCH_DATA
{
  DS_TOUCH_FINGER_DATA Finger[2];
  UINT8 Timestamp;
} DS_TOUCH_DATA;

/**
 * @brief The DualSense Edge controller input report structure.
 */
typedef struct _DUO_CONTROLLER_INPUT_REPORT_DS
{
  UINT8 LeftStickX;
  UINT8 LeftStickY;
  UINT8 RightStickX;
  UINT8 RightStickY;
  UINT8 TriggerLeft;
  UINT8 TriggerRight;
  UINT8 SeqNo;

  UINT8 DPad : 4;
  UINT8 ButtonSquare : 1;
  UINT8 ButtonCross : 1;
  UINT8 ButtonCircle : 1;
  UINT8 ButtonTriangle : 1;

  UINT8 ButtonL1 : 1;
  UINT8 ButtonR1 : 1;
  UINT8 ButtonL2 : 1;
  UINT8 ButtonR2 : 1;
  UINT8 ButtonCreate : 1;
  UINT8 ButtonOptions : 1;
  UINT8 ButtonL3 : 1;
  UINT8 ButtonR3 : 1;

  UINT8 ButtonHome : 1;
  UINT8 ButtonPad : 1;
  UINT8 ButtonMute : 1;
  UINT8 Reserved1 : 1;
  UINT8 ButtonLeftFunction : 1;
  UINT8 ButtonRightFunction : 1;
  UINT8 ButtonLeftPaddle : 1;
  UINT8 ButtonRightPaddle : 1;

  UINT8 Reserved2;
  UINT32 ReservedCounter;

  INT16 AngularVelocityX;
  INT16 AngularVelocityY;
  INT16 AngularVelocityZ;

  INT16 AccelerometerX;
  INT16 AccelerometerY;
  INT16 AccelerometerZ;

  UINT32 SensorTimestamp;
  INT8 Temperature;

  DS_TOUCH_DATA TouchData;

  UINT8 TriggerRightStopLocation : 4;
  UINT8 TriggerRightStatus : 4;

  UINT8 TriggerLeftStopLocation : 4;
  UINT8 TriggerLeftStatus : 4;

  UINT32 HostTimestamp;

  UINT8 TriggerRightEffect : 4;
  UINT8 TriggerLeftEffect : 4;

  UINT32 DeviceTimeStamp;

  UINT8 PowerPercent : 4;
  UINT8 PowerState : 4;

  UINT8 PluggedHeadphones : 1;
  UINT8 PluggedMic : 1;
  UINT8 MicMuted : 1;
  UINT8 PluggedUsbData : 1;
  UINT8 PluggedUsbPower : 1;
  UINT8 UsbPowerOnBT : 1;
  UINT8 DockDetect : 1;
  UINT8 PluggedUnk : 1;

  UINT8 PluggedExternalMic : 1;
  UINT8 HapticLowPassFilter : 1;
  UINT8 Reserved3 : 6;

  UINT8 AesCmac[8];
} DUO_CONTROLLER_INPUT_REPORT_DS;

/**
 * @brief The DualShock 4 controller input report structure.
 */
typedef struct _DUO_CONTROLLER_INPUT_REPORT_DS4
{
  UINT8 LeftStickHorizontal;
  UINT8 LeftStickVertical;
  UINT8 RightStickHorizontal;
  UINT8 RightStickVertical;

  UINT8 LeftTrigger;
  UINT8 RightTrigger;

  UINT8 DPad; // 0 = Up, 1 = Up-Right, 2 = Right, 3 = Down-Right, 4 = Down, 5 = Down-Left, 6 = Left, 7 = Up-Left, 8 = Neutral

  UINT8 Square : 1;
  UINT8 Cross : 1;
  UINT8 Circle : 1;
  UINT8 Triangle : 1;
  UINT8 L1 : 1;
  UINT8 R1 : 1;
  UINT8 L2 : 1;
  UINT8 R2 : 1;
  UINT8 Share : 1;
  UINT8 Options : 1;
  UINT8 L3 : 1;
  UINT8 R3 : 1;
  UINT8 PS : 1;
  UINT8 Touchpad : 1;

  INT16 AngularVelocityX;
  INT16 AngularVelocityY;
  INT16 AngularVelocityZ;

  INT16 AccelerometerX;
  INT16 AccelerometerY;
  INT16 AccelerometerZ;

  DS_TOUCH_DATA TouchData;
} DUO_CONTROLLER_INPUT_REPORT_DS4;

/**
 * @brief The controller force feedback report structure.
 */
typedef struct _DUO_CONTROLLER_FORCE_FEEDBACK_REPORT
{
  UINT8 Flags; // SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_MOTOR_VALID | SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_MOTOR_VALID | SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_RIGHT_TRIGGER_VALID | SYNTHETIC_CONTROLLER_OUTPUT_REPORT_FLAG_LEFT_TRIGGER_VALID
  UINT8 LeftTrigger; // 0-255
  UINT8 RightTrigger; // 0-255
  UINT8 LeftMotor; // 0-255
  UINT8 RightMotor; // 0-255
  UINT8 Duration; // 0-255 (246 on test capture, must be 255 when combined with Delay)
  UINT8 Delay; // 0-255 (9 on test capture, must be 255 when combined with Duration)
  UINT8 Repeat; // 0 or 1 (0 on test capture)
  UINT8 PulsePeriod; // 0 on test capture
  UINT8 NumberOfPulses; // 235 on test capture
} DUO_CONTROLLER_FORCE_FEEDBACK_REPORT;

#pragma pack(pop)

/**
 * @brief Receives vibration data from a Duo controller.
 * @param controller The controller.
 * @param report The force feedback report.
 * @param context The context.
 */
typedef void (*DuoController_VibrationReportCallback_t)(void* controller, DUO_CONTROLLER_FORCE_FEEDBACK_REPORT* report, void* context);

/**
 * @brief Initializes the DuoController library.
 * @returns S_OK if the initialization was successful.
 */
WINUSERAPI HRESULT WINAPI DuoController_Initialize();

/**
 * @brief Uninitializes the DuoController library.
 * @returns S_OK if the uninitialization was successful.
 */
WINUSERAPI HRESULT WINAPI DuoController_Uninitialize();

/**
 * @brief Creates a new Duo controller.
 * @param controllerType The type of controller to create.
 * @param vibrationCallback The vibration report callback.
 * @param vibrationCallbackContext The vibration callback context.
 * @param controller Receives the created controller.
 * @returns S_OK if the controller was created successfully.
 */
WINUSERAPI HRESULT WINAPI DuoController_CreateController(DUO_CONTROLLER_TYPE controllerType, DuoController_VibrationReportCallback_t vibrationCallback, void* vibrationCallbackContext, void** controller);

/**
 * @brief Removes a Duo controller.
 * @param controller The controller to remove.
 * @returns S_OK if the controller was removed successfully.
 */
WINUSERAPI HRESULT WINAPI DuoController_RemoveController(void* controller);

/**
 * @brief Sends an input report to the given Duo controller.
 * @param controller The controller to send the input report to.
 * @param inputReport The input report to send.
 * @returns S_OK if the report was sent successfully.
 */
WINUSERAPI HRESULT WINAPI DuoController_SendReport(void* controller, void* inputReport);
#endif

namespace platf {
  using namespace std::literals;

  thread_local HDESK _lastKnownInputDesktop = nullptr;

  constexpr touch_port_t target_touch_port {
    0,
    0,
    65535,
    65535
  };

  struct gamepad_context_t {
    // The DuoController handle
    void* gp;

    // The feedback queue used to report back vibration
    feedback_queue_t feedback_queue;

    // The controller type
    DUO_CONTROLLER_TYPE type;

    // The current Xbox input report containing the buttons and axes
    DUO_CONTROLLER_INPUT_REPORT_XBOX xbox_report;

    // The current DualSense input report containing the buttons and axes
    DUO_CONTROLLER_INPUT_REPORT_DS ds_report;

    // The current DualShock 4 input report containing the buttons and axes
    DUO_CONTROLLER_INPUT_REPORT_DS4 ds4_report;

    // Map from pointer ID to pointer index
    std::map<uint32_t, uint8_t> pointer_id_map;
    uint8_t available_pointers;

    // The client relative index
    uint8_t client_relative_index;

    // The last reported rumble motor states
    gamepad_feedback_msg_t last_rumble;
  };

  constexpr float EARTH_G = 9.80665f;

#define MPS2_TO_DS_ACCEL(x) (int32_t) (((x) / EARTH_G) * 8192)
#define DPS_TO_DS_GYRO(x) (int32_t) ((x) * (1024 / 64))

  /**
   * @brief Updates the input report with the provided motion data.
   * @details Acceleration is in m/s^2 and gyro is in deg/s.
   * @tparam T The report type (DUO_CONTROLLER_INPUT_REPORT_DS or DUO_CONTROLLER_INPUT_REPORT_DS4).
   * @param report The input report to update.
   * @param motion_type The type of motion data.
   * @param x X component of motion.
   * @param y Y component of motion.
   * @param z Z component of motion.
   */
  template <typename T>
  static void update_motion(T &report, uint8_t motion_type, float x, float y, float z) {
    // Use int32 to process this data, so we can clamp if needed.
    int32_t intX, intY, intZ;

    switch (motion_type) {
      case LI_MOTION_TYPE_ACCEL:
        // Convert to the DualSense accelerometer scale
        intX = MPS2_TO_DS_ACCEL(x);
        intY = MPS2_TO_DS_ACCEL(y);
        intZ = MPS2_TO_DS_ACCEL(z);
        break;
      case LI_MOTION_TYPE_GYRO:
        // Convert to the DualSense gyro scale
        intX = DPS_TO_DS_GYRO(x);
        intY = DPS_TO_DS_GYRO(y);
        intZ = DPS_TO_DS_GYRO(z);
        break;
      default:
        return;
    }

    // Clamp the values to the range of the data type
    intX = std::clamp(intX, INT16_MIN, INT16_MAX);
    intY = std::clamp(intY, INT16_MIN, INT16_MAX);
    intZ = std::clamp(intZ, INT16_MIN, INT16_MAX);

    // Populate the report
    switch (motion_type) {
      case LI_MOTION_TYPE_ACCEL:
        report.AccelerometerX = (int16_t) intX;
        report.AccelerometerY = (int16_t) intY;
        report.AccelerometerZ = (int16_t) intZ;
        break;
      case LI_MOTION_TYPE_GYRO:
        report.AngularVelocityX = (int16_t) intX;
        report.AngularVelocityY = (int16_t) intY;
        report.AngularVelocityZ = (int16_t) intZ;
        break;
      default:
        return;
    }
  }

  class DuoController_t {
  public:
    /**
     * @brief Initializes the DuoController helper class.
     * @returns 0 if the initialization was successful.
     */
    int init()
    {
      // Load the DuoController.dll module
      mDuoController = LoadLibraryA("DuoController\\DuoController.dll");

      // We managed to load the DuoController.dll module
      if (mDuoController == NULL) {
        BOOST_LOG(fatal) << "DuoController library failed to load!"sv;
        return -1;
      }

      // Get pointers to the DuoController.dll functions
      fnDuoController_Initialize = (decltype(DuoController_Initialize) *) GetProcAddress(mDuoController, "DuoController_Initialize");
      fnDuoController_Uninitialize = (decltype(DuoController_Uninitialize) *) GetProcAddress(mDuoController, "DuoController_Uninitialize");
      fnDuoController_CreateController = (decltype(DuoController_CreateController) *) GetProcAddress(mDuoController, "DuoController_CreateController");
      fnDuoController_RemoveController = (decltype(DuoController_RemoveController) *) GetProcAddress(mDuoController, "DuoController_RemoveController");
      fnDuoController_SendReport = (decltype(DuoController_SendReport) *) GetProcAddress(mDuoController, "DuoController_SendReport");

      // Initialize the DuoController API
      if (fnDuoController_Initialize == NULL ||
          fnDuoController_Uninitialize == NULL ||
          fnDuoController_CreateController == NULL ||
          fnDuoController_RemoveController == NULL ||
          fnDuoController_SendReport == NULL) {
        // We failed to load the DuoController library
        BOOST_LOG(fatal) << "DuoController library is unsupported!"sv;
        return -1;
      }

      // Probe DuoController during startup so we can show an error in the UI *before* a stream starts.
      auto status = fnDuoController_Initialize();
      if (FAILED(status)) {
        // We failed to initialize the DuoController library
        BOOST_LOG(fatal) << "DuoController library failed to initialize! " << util::hex(status).to_string_view();
        return -1;
      }

      // Initialize the gamepad array
      gamepads.resize(MAX_GAMEPADS);

      // Return success
      return 0;
    }

    /**
     * @brief Allocates a new virtual gamepad.
     * @param id The gamepad index.
     * @param feedback_queue The feedback queue that will receive rumble events.
     * @param gp_type The type of gamepad.
     * @returns 0 if the gamepad was successfully allocated.
     */
    int alloc_gamepad_internal(const gamepad_id_t &id, feedback_queue_t &feedback_queue, DUO_CONTROLLER_TYPE gp_type)
    {
      // Cast the gamepad structure
      auto &gamepad = gamepads[id.globalIndex];

      // Ensure the slot isn't already in use
      assert(!gamepad.gp);

      // Set the gamepad type
      gamepad.type = gp_type;

      // Assign the client relative index
      gamepad.client_relative_index = id.clientRelativeIndex;

      // Create the virtual gamepad
      auto status = fnDuoController_CreateController ? fnDuoController_CreateController(gamepad.type, &duo_vibration_cb, this, &gamepad.gp) : E_FAIL;
      if (FAILED(status)) {
        BOOST_LOG(error) << "Could not create controller: " << util::hex(status).to_string_view();
        return -1;
      }

      // Initialize the gamepad reports
      memset(&gamepad.xbox_report, 0, sizeof(gamepad.xbox_report));
      memset(&gamepad.ds_report, 0, sizeof(gamepad.ds_report));
      memset(&gamepad.ds4_report, 0, sizeof(gamepad.ds4_report));

      // We're emulating a DualSense or DualSense Edge gamepad
      if (gamepad.type == DuoControllerTypeDualSense || gamepad.type == DuoControllerTypeDualSenseEdge) {
        // Set the virtual battery to 100%
        gamepad.ds_report.PowerPercent = 10;

        // The PowerPercent value is ignored if not set to Charging
        gamepad.ds_report.PowerState = (UINT8)Charging;

        // Pretend we're USB-wired
        gamepad.ds_report.PluggedUsbData = 1;

        // Set initial touchpad state
        for (int i = 0; i < 2; i++) {
          gamepad.ds_report.TouchData.Finger[i].Index = i;
          gamepad.ds_report.TouchData.Finger[i].NotTouching = 1;
        }

        // Set initial accelerometer and gyro state
        update_motion(gamepad.ds_report, LI_MOTION_TYPE_ACCEL, 0.0f, EARTH_G, 0.0f);
        update_motion(gamepad.ds_report, LI_MOTION_TYPE_GYRO, 0.0f, 0.0f, 0.0f);

        // Request motion events from the client at 100 Hz
        feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_ACCEL, 100));
        feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_GYRO, 100));

        // We support pointer index 0 and 1
        gamepad.available_pointers = 0x3;
      }

      // We're emulating a DualShock 4 gamepad
      else if (gamepad.type == DuoControllerTypeDualShock4) {
        // Set initial touchpad state
        for (int i = 0; i < 2; i++) {
          gamepad.ds4_report.TouchData.Finger[i].Index = i;
          gamepad.ds4_report.TouchData.Finger[i].NotTouching = 1;
        }

        // Set initial accelerometer and gyro state
        update_motion(gamepad.ds4_report, LI_MOTION_TYPE_ACCEL, 0.0f, EARTH_G, 0.0f);
        update_motion(gamepad.ds4_report, LI_MOTION_TYPE_GYRO, 0.0f, 0.0f, 0.0f);

        // Request motion events from the client at 100 Hz
        feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_ACCEL, 100));
        feedback_queue->raise(gamepad_feedback_msg_t::make_motion_event_state(gamepad.client_relative_index, LI_MOTION_TYPE_GYRO, 100));

        // We support pointer index 0 and 1
        gamepad.available_pointers = 0x3;
      }

      // Keep track of the feedback queue so we can report future rumble events
      gamepad.feedback_queue = std::move(feedback_queue);

      // Gamepad allocated successfully
      return 0;
    }

    /**
     * @brief Frees the given virtual gamepad.
     * @param nr The gamepad index.
     */
    void free_target(int nr)
    {
      // Cast the gamepad structure
      auto &gamepad = gamepads[nr];

      // The gamepad has been initialized
      if (gamepad.gp) {
        // Remove the virtual gamepad
        auto status = fnDuoController_RemoveController ? fnDuoController_RemoveController(gamepad.gp) : E_FAIL;
        if (FAILED(status)) {
          BOOST_LOG(warning) << "Could not remove controller: " << util::hex(status).to_string_view();
        }

        // Reset the internal handle
        gamepad.gp = NULL;
      }
    }

    /**
     * @brief Sends an updated gamepad report to the kernel.
     * @tparam T The report type.
     * @param nr The gamepad index.
     * @param report The updated gamepad report.
     */
    template <typename T>
    void SendReport(int nr, const T &report)
    {
      // Cast the gamepad structure
      auto &gamepad = gamepads[nr];

      // The gamepad has been initialized
      if (gamepad.gp) {
        // Send the gamepad report
        auto status = fnDuoController_SendReport ? fnDuoController_SendReport(gamepad.gp, const_cast<T*>(&report)) : E_FAIL;
        if (FAILED(status)) {
          BOOST_LOG(error) << "Could not send controller report: " << util::hex(status).to_string_view();
        }
      }
    }

    /**
     * @brief Destroys the DuoController helper class.
     */
    ~DuoController_t() {
      // Iterate all gamepads
      for (auto &gp : gamepads) {
        // Skip gamepads that aren't in use
        if (gp.gp) {
          // We have access to the DuoController module exports
          if (fnDuoController_RemoveController != NULL) {
            // Remove the controller
            fnDuoController_RemoveController(gp.gp);
          }

          // Reset the controller handle
          gp.gp = NULL;
        }
      }

      // We have access to the DuoController module exports
      if (fnDuoController_Uninitialize != NULL) {
        // Uninitialize the DuoController module
        fnDuoController_Uninitialize();
      }

      // We mapped the DuoController module
      if (mDuoController != NULL) {
        // Unmap the DuoController module
        FreeLibrary(mDuoController);

        // Reset the module handle
        mDuoController = NULL;

        // Reset the function pointers
        fnDuoController_Initialize = NULL;
        fnDuoController_Uninitialize = NULL;
        fnDuoController_CreateController = NULL;
        fnDuoController_RemoveController = NULL;
        fnDuoController_SendReport = NULL;
      }
    }

    // The virtual gamepad vector
    std::vector<gamepad_context_t> gamepads;
  private:
    // The DuoController module handle
    HMODULE mDuoController;

    // The DuoController module exports
    decltype(DuoController_Initialize) *fnDuoController_Initialize;
    decltype(DuoController_Uninitialize) *fnDuoController_Uninitialize;
    decltype(DuoController_CreateController) *fnDuoController_CreateController;
    decltype(DuoController_RemoveController) *fnDuoController_RemoveController;
    decltype(DuoController_SendReport) *fnDuoController_SendReport;

    /**
     * @brief Receives vibration data from the Windows kernel.
     * @param controller The internal controller handle.
     * @param smallMotorSpeed The small motor speed.
     * @param largeMotorSpeed The large motor speed.
     * @param context The callback context.
     */
    static void CALLBACK duo_vibration_cb(void *controller, DUO_CONTROLLER_FORCE_FEEDBACK_REPORT* report, void *context)
    {
      // Cast the DuoController instance
      auto *self = reinterpret_cast<DuoController_t*>(context);

      // Scale the motor values from 0~255 to 0~65535
      uint16_t low = static_cast<uint16_t>(report->RightMotor) << 8;
      uint16_t high = static_cast<uint16_t>(report->LeftMotor) << 8;

      // Iterate all allocated virtual gamepads
      for (int i = 0; i < self->gamepads.size(); i++)
      {
        // Cast the virtual gamepad
        auto &gp = self->gamepads[i];

        // We found the target virtual gamepad
        if (gp.gp == controller)
        {
          // Don't waste bandwidth reporting the same event over and over
          if (low != gp.last_rumble.data.rumble.highfreq || high != gp.last_rumble.data.rumble.lowfreq)
          {
            // Queue a rumble feedback message
            gamepad_feedback_msg_t msg = gamepad_feedback_msg_t::make_rumble(gp.client_relative_index, high, low);
            gp.feedback_queue->raise(msg);
            gp.last_rumble = msg;
          }

          // No reason to iterate the other virtual gamepads
          break;
        }
      }
    }
  };

  struct input_raw_t {
    ~input_raw_t() {
      delete duo;
    }

    DuoController_t *duo;

    decltype(CreateSyntheticPointerDevice) *fnCreateSyntheticPointerDevice;
    decltype(InjectSyntheticPointerInput) *fnInjectSyntheticPointerInput;
    decltype(DestroySyntheticPointerDevice) *fnDestroySyntheticPointerDevice;
  };

  input_t input() {
    input_t result {new input_raw_t {}};
    auto &raw = *(input_raw_t *) result.get();

    raw.duo = new DuoController_t {};
    if (raw.duo->init()) {
      delete raw.duo;
      raw.duo = nullptr;
    }

    // Get pointers to virtual touch/pen input functions (Win10 1809+)
    raw.fnCreateSyntheticPointerDevice = (decltype(CreateSyntheticPointerDevice) *) GetProcAddress(GetModuleHandleA("user32.dll"), "CreateSyntheticPointerDevice");
    raw.fnInjectSyntheticPointerInput = (decltype(InjectSyntheticPointerInput) *) GetProcAddress(GetModuleHandleA("user32.dll"), "InjectSyntheticPointerInput");
    raw.fnDestroySyntheticPointerDevice = (decltype(DestroySyntheticPointerDevice) *) GetProcAddress(GetModuleHandleA("user32.dll"), "DestroySyntheticPointerDevice");

    return result;
  }

  /**
   * @brief Calls SendInput() and switches input desktops if required.
   * @param i The `INPUT` struct to send.
   */
  void send_input(INPUT &i) {
  retry:
    auto send = SendInput(1, &i, sizeof(INPUT));
    if (send != 1) {
      auto hDesk = syncThreadDesktop();
      if (_lastKnownInputDesktop != hDesk) {
        _lastKnownInputDesktop = hDesk;
        goto retry;
      }
      BOOST_LOG(error) << "Couldn't send input"sv;
    }
  }

  /**
   * @brief Calls InjectSyntheticPointerInput() and switches input desktops if required.
   * @details Must only be called if InjectSyntheticPointerInput() is available.
   * @param input The global input context.
   * @param device The synthetic pointer device handle.
   * @param pointerInfo An array of `POINTER_TYPE_INFO` structs.
   * @param count The number of elements in `pointerInfo`.
   * @return true if input was successfully injected.
   */
  bool inject_synthetic_pointer_input(input_raw_t *input, HSYNTHETICPOINTERDEVICE device, const POINTER_TYPE_INFO *pointerInfo, UINT32 count) {
  retry:
    if (!input->fnInjectSyntheticPointerInput(device, pointerInfo, count)) {
      auto hDesk = syncThreadDesktop();
      if (_lastKnownInputDesktop != hDesk) {
        _lastKnownInputDesktop = hDesk;
        goto retry;
      }
      return false;
    }
    return true;
  }

  void abs_mouse(input_t &input, const touch_port_t &touch_port, float x, float y) {
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    mi.dwFlags =
      MOUSEEVENTF_MOVE |
      MOUSEEVENTF_ABSOLUTE |

      // MOUSEEVENTF_VIRTUALDESK maps to the entirety of the desktop rather than the primary desktop
      MOUSEEVENTF_VIRTUALDESK;

    auto scaled_x = std::lround((x + touch_port.offset_x) * ((float) target_touch_port.width / (float) touch_port.width));
    auto scaled_y = std::lround((y + touch_port.offset_y) * ((float) target_touch_port.height / (float) touch_port.height));

    mi.dx = scaled_x;
    mi.dy = scaled_y;

    send_input(i);
  }

  void move_mouse(input_t &input, int deltaX, int deltaY) {
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_MOVE;
    mi.dx = deltaX;
    mi.dy = deltaY;

    send_input(i);
  }

  util::point_t get_mouse_loc(input_t &input) {
    throw std::runtime_error("not implemented yet, has to pass tests");
    // TODO: Tests are failing, something wrong here?
    POINT p;
    if (!GetCursorPos(&p)) {
      return util::point_t {0.0, 0.0};
    }

    return util::point_t {
      (double) p.x,
      (double) p.y
    };
  }

  void button_mouse(input_t &input, int button, bool release) {
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    if (button == 1) {
      mi.dwFlags = release ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
    } else if (button == 2) {
      mi.dwFlags = release ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
    } else if (button == 3) {
      mi.dwFlags = release ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
    } else if (button == 4) {
      mi.dwFlags = release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
      mi.mouseData = XBUTTON1;
    } else {
      mi.dwFlags = release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
      mi.mouseData = XBUTTON2;
    }

    send_input(i);
  }

  void scroll(input_t &input, int distance) {
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_WHEEL;
    mi.mouseData = distance;

    send_input(i);
  }

  void hscroll(input_t &input, int distance) {
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_HWHEEL;
    mi.mouseData = distance;

    send_input(i);
  }

  void keyboard_update(input_t &input, uint16_t modcode, bool release, uint8_t flags) {
    INPUT i {};
    i.type = INPUT_KEYBOARD;
    auto &ki = i.ki;

    // If the client did not normalize this VK code to a US English layout, we can't accurately convert it to a scancode.
    // If we're set to always send scancodes, we will use the current keyboard layout to convert to a scancode. This will
    // assume the client and host have the same keyboard layout, but it's probably better than always using US English.
    if (!(flags & SS_KBE_FLAG_NON_NORMALIZED)) {
      // Mask off the extended key byte
      ki.wScan = VK_TO_SCANCODE_MAP[modcode & 0xFF];
    } else if (config::input.always_send_scancodes && modcode != VK_LWIN && modcode != VK_RWIN && modcode != VK_PAUSE) {
      // For some reason, MapVirtualKey(VK_LWIN, MAPVK_VK_TO_VSC) doesn't seem to work :/
      ki.wScan = MapVirtualKey(modcode, MAPVK_VK_TO_VSC);
    }

    // If we can map this to a scancode, send it as a scancode for maximum game compatibility.
    if (ki.wScan) {
      ki.dwFlags = KEYEVENTF_SCANCODE;
    } else {
      // If there is no scancode mapping or it's non-normalized, send it as a regular VK event.
      ki.wVk = modcode;
    }

    // https://docs.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
    switch (modcode) {
      case VK_LWIN:
      case VK_RWIN:
      case VK_RMENU:
      case VK_RCONTROL:
      case VK_INSERT:
      case VK_DELETE:
      case VK_HOME:
      case VK_END:
      case VK_PRIOR:
      case VK_NEXT:
      case VK_UP:
      case VK_DOWN:
      case VK_LEFT:
      case VK_RIGHT:
      case VK_DIVIDE:
      case VK_APPS:
        ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        break;
      default:
        break;
    }

    if (release) {
      ki.dwFlags |= KEYEVENTF_KEYUP;
    }

    send_input(i);
  }

  struct client_input_raw_t: public client_input_t {
    client_input_raw_t(input_t &input) {
      global = (input_raw_t *) input.get();
    }

    ~client_input_raw_t() override {
      if (penRepeatTask) {
        task_pool.cancel(penRepeatTask);
      }
      if (touchRepeatTask) {
        task_pool.cancel(touchRepeatTask);
      }

      if (pen) {
        global->fnDestroySyntheticPointerDevice(pen);
      }
      if (touch) {
        global->fnDestroySyntheticPointerDevice(touch);
      }
    }

    input_raw_t *global;

    // Device state and handles for pen and touch input must be stored in the per-client
    // input context, because each connected client may be sending their own independent
    // pen/touch events. To maintain separation, we expose separate pen and touch devices
    // for each client.

    HSYNTHETICPOINTERDEVICE pen {};
    POINTER_TYPE_INFO penInfo {};
    thread_pool_util::ThreadPool::task_id_t penRepeatTask {};

    HSYNTHETICPOINTERDEVICE touch {};
    POINTER_TYPE_INFO touchInfo[10] {};
    UINT32 activeTouchSlots {};
    thread_pool_util::ThreadPool::task_id_t touchRepeatTask {};
  };

  /**
   * @brief Allocates a context to store per-client input data.
   * @param input The global input context.
   * @return A unique pointer to a per-client input data context.
   */
  std::unique_ptr<client_input_t> allocate_client_input_context(input_t &input) {
    return std::make_unique<client_input_raw_t>(input);
  }

  /**
   * @brief Compacts the touch slots into a contiguous block and updates the active count.
   * @details Since this swaps entries around, all slot pointers/references are invalid after compaction.
   * @param raw The client-specific input context.
   */
  void perform_touch_compaction(client_input_raw_t *raw) {
    // Windows requires all active touches be contiguous when fed into InjectSyntheticPointerInput().
    UINT32 i;
    for (i = 0; i < ARRAYSIZE(raw->touchInfo); i++) {
      if (raw->touchInfo[i].touchInfo.pointerInfo.pointerFlags == POINTER_FLAG_NONE) {
        // This is an empty slot. Look for a later entry to move into this slot.
        for (UINT32 j = i + 1; j < ARRAYSIZE(raw->touchInfo); j++) {
          if (raw->touchInfo[j].touchInfo.pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
            std::swap(raw->touchInfo[i], raw->touchInfo[j]);
            break;
          }
        }

        // If we didn't find anything, we've reached the end of active slots.
        if (raw->touchInfo[i].touchInfo.pointerInfo.pointerFlags == POINTER_FLAG_NONE) {
          break;
        }
      }
    }

    // Update the number of active touch slots
    raw->activeTouchSlots = i;
  }

  /**
   * @brief Gets a pointer slot by client-relative pointer ID, claiming a new one if necessary.
   * @param raw The raw client-specific input context.
   * @param pointerId The client's pointer ID.
   * @param eventType The LI_TOUCH_EVENT value from the client.
   * @return A pointer to the slot entry.
   */
  POINTER_TYPE_INFO *pointer_by_id(client_input_raw_t *raw, uint32_t pointerId, uint8_t eventType) {
    // Compact active touches into a single contiguous block
    perform_touch_compaction(raw);

    // Try to find a matching pointer ID
    for (UINT32 i = 0; i < ARRAYSIZE(raw->touchInfo); i++) {
      if (raw->touchInfo[i].touchInfo.pointerInfo.pointerId == pointerId &&
          raw->touchInfo[i].touchInfo.pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
        if (eventType == LI_TOUCH_EVENT_DOWN && (raw->touchInfo[i].touchInfo.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT)) {
          BOOST_LOG(warning) << "Pointer "sv << pointerId << " already down. Did the client drop an up/cancel event?"sv;
        }

        return &raw->touchInfo[i];
      }
    }

    if (eventType != LI_TOUCH_EVENT_HOVER && eventType != LI_TOUCH_EVENT_DOWN) {
      BOOST_LOG(warning) << "Unexpected new pointer "sv << pointerId << " for event "sv << (uint32_t) eventType << ". Did the client drop a down/hover event?"sv;
    }

    // If there was none, grab an unused entry and increment the active slot count
    for (UINT32 i = 0; i < ARRAYSIZE(raw->touchInfo); i++) {
      if (raw->touchInfo[i].touchInfo.pointerInfo.pointerFlags == POINTER_FLAG_NONE) {
        raw->touchInfo[i].touchInfo.pointerInfo.pointerId = pointerId;
        raw->activeTouchSlots = i + 1;
        return &raw->touchInfo[i];
      }
    }

    return nullptr;
  }

  /**
   * @brief Populate common `POINTER_INFO` members shared between pen and touch events.
   * @param pointerInfo The pointer info to populate.
   * @param touchPort The current viewport for translating to screen coordinates.
   * @param eventType The type of touch/pen event.
   * @param x The normalized 0.0-1.0 X coordinate.
   * @param y The normalized 0.0-1.0 Y coordinate.
   */
  void populate_common_pointer_info(POINTER_INFO &pointerInfo, const touch_port_t &touchPort, uint8_t eventType, float x, float y) {
    switch (eventType) {
      case LI_TOUCH_EVENT_HOVER:
        pointerInfo.pointerFlags &= ~POINTER_FLAG_INCONTACT;
        pointerInfo.pointerFlags |= POINTER_FLAG_INRANGE | POINTER_FLAG_UPDATE;
        pointerInfo.ptPixelLocation.x = x * touchPort.width + touchPort.offset_x;
        pointerInfo.ptPixelLocation.y = y * touchPort.height + touchPort.offset_y;
        break;
      case LI_TOUCH_EVENT_DOWN:
        pointerInfo.pointerFlags |= POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN;
        pointerInfo.ptPixelLocation.x = x * touchPort.width + touchPort.offset_x;
        pointerInfo.ptPixelLocation.y = y * touchPort.height + touchPort.offset_y;
        break;
      case LI_TOUCH_EVENT_UP:
        // We expect to get another LI_TOUCH_EVENT_HOVER if the pointer remains in range
        pointerInfo.pointerFlags &= ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE);
        pointerInfo.pointerFlags |= POINTER_FLAG_UP;
        break;
      case LI_TOUCH_EVENT_MOVE:
        pointerInfo.pointerFlags |= POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_UPDATE;
        pointerInfo.ptPixelLocation.x = x * touchPort.width + touchPort.offset_x;
        pointerInfo.ptPixelLocation.y = y * touchPort.height + touchPort.offset_y;
        break;
      case LI_TOUCH_EVENT_CANCEL:
      case LI_TOUCH_EVENT_CANCEL_ALL:
        // If we were in contact with the touch surface at the time of the cancellation,
        // we'll set POINTER_FLAG_UP, otherwise set POINTER_FLAG_UPDATE.
        if (pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) {
          pointerInfo.pointerFlags |= POINTER_FLAG_UP;
        } else {
          pointerInfo.pointerFlags |= POINTER_FLAG_UPDATE;
        }
        pointerInfo.pointerFlags &= ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE);
        pointerInfo.pointerFlags |= POINTER_FLAG_CANCELED;
        break;
      case LI_TOUCH_EVENT_HOVER_LEAVE:
        pointerInfo.pointerFlags &= ~(POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE);
        pointerInfo.pointerFlags |= POINTER_FLAG_UPDATE;
        break;
      case LI_TOUCH_EVENT_BUTTON_ONLY:
        // On Windows, we can only pass buttons if we have an active pointer
        if (pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
          pointerInfo.pointerFlags |= POINTER_FLAG_UPDATE;
        }
        break;
      default:
        BOOST_LOG(warning) << "Unknown touch event: "sv << (uint32_t) eventType;
        break;
    }
  }

  // Active pointer interactions sent via InjectSyntheticPointerInput() seem to be automatically
  // cancelled by Windows if not repeated/updated within about a second. To avoid this, refresh
  // the injected input periodically.
  constexpr auto ISPI_REPEAT_INTERVAL = 50ms;

  /**
   * @brief Repeats the current touch state to avoid the interactions timing out.
   * @param raw The raw client-specific input context.
   */
  void repeat_touch(client_input_raw_t *raw) {
    if (!inject_synthetic_pointer_input(raw->global, raw->touch, raw->touchInfo, raw->activeTouchSlots)) {
      auto err = GetLastError();
      BOOST_LOG(warning) << "Failed to refresh virtual touch input: "sv << err;
    }

    raw->touchRepeatTask = task_pool.pushDelayed(repeat_touch, ISPI_REPEAT_INTERVAL, raw).task_id;
  }

  /**
   * @brief Repeats the current pen state to avoid the interactions timing out.
   * @param raw The raw client-specific input context.
   */
  void repeat_pen(client_input_raw_t *raw) {
    if (!inject_synthetic_pointer_input(raw->global, raw->pen, &raw->penInfo, 1)) {
      auto err = GetLastError();
      BOOST_LOG(warning) << "Failed to refresh virtual pen input: "sv << err;
    }

    raw->penRepeatTask = task_pool.pushDelayed(repeat_pen, ISPI_REPEAT_INTERVAL, raw).task_id;
  }

  /**
   * @brief Cancels all active touches.
   * @param raw The raw client-specific input context.
   */
  void cancel_all_active_touches(client_input_raw_t *raw) {
    // Cancel touch repeat callbacks
    if (raw->touchRepeatTask) {
      task_pool.cancel(raw->touchRepeatTask);
      raw->touchRepeatTask = nullptr;
    }

    // Compact touches to update activeTouchSlots
    perform_touch_compaction(raw);

    // If we have active slots, cancel them all
    if (raw->activeTouchSlots > 0) {
      for (UINT32 i = 0; i < raw->activeTouchSlots; i++) {
        populate_common_pointer_info(raw->touchInfo[i].touchInfo.pointerInfo, {}, LI_TOUCH_EVENT_CANCEL_ALL, 0.0f, 0.0f);
        raw->touchInfo[i].touchInfo.touchMask = TOUCH_MASK_NONE;
      }
      if (!inject_synthetic_pointer_input(raw->global, raw->touch, raw->touchInfo, raw->activeTouchSlots)) {
        auto err = GetLastError();
        BOOST_LOG(warning) << "Failed to cancel all virtual touch input: "sv << err;
      }
    }

    // Zero all touch state
    std::memset(raw->touchInfo, 0, sizeof(raw->touchInfo));
    raw->activeTouchSlots = 0;
  }

  // These are edge-triggered pointer state flags that should always be cleared next frame
  constexpr auto EDGE_TRIGGERED_POINTER_FLAGS = POINTER_FLAG_DOWN | POINTER_FLAG_UP | POINTER_FLAG_CANCELED | POINTER_FLAG_UPDATE;

  /**
   * @brief Sends a touch event to the OS.
   * @param input The client-specific input context.
   * @param touch_port The current viewport for translating to screen coordinates.
   * @param touch The touch event.
   */
  void touch_update(client_input_t *input, const touch_port_t &touch_port, const touch_input_t &touch) {
    auto raw = (client_input_raw_t *) input;

    // Bail if we're not running on an OS that supports virtual touch input
    if (!raw->global->fnCreateSyntheticPointerDevice ||
        !raw->global->fnInjectSyntheticPointerInput ||
        !raw->global->fnDestroySyntheticPointerDevice) {
      BOOST_LOG(warning) << "Touch input requires Windows 10 1809 or later"sv;
      return;
    }

    // If there's not already a virtual touch device, create one now
    if (!raw->touch) {
      if (touch.eventType != LI_TOUCH_EVENT_CANCEL_ALL) {
        BOOST_LOG(info) << "Creating virtual touch input device"sv;
        raw->touch = raw->global->fnCreateSyntheticPointerDevice(PT_TOUCH, ARRAYSIZE(raw->touchInfo), POINTER_FEEDBACK_DEFAULT);
        if (!raw->touch) {
          auto err = GetLastError();
          BOOST_LOG(warning) << "Failed to create virtual touch device: "sv << err;
          return;
        }
      } else {
        // No need to cancel anything if we had no touch input device
        return;
      }
    }

    // Cancel touch repeat callbacks
    if (raw->touchRepeatTask) {
      task_pool.cancel(raw->touchRepeatTask);
      raw->touchRepeatTask = nullptr;
    }

    // If this is a special request to cancel all touches, do that and return
    if (touch.eventType == LI_TOUCH_EVENT_CANCEL_ALL) {
      cancel_all_active_touches(raw);
      return;
    }

    // Find or allocate an entry for this touch pointer ID
    auto pointer = pointer_by_id(raw, touch.pointerId, touch.eventType);
    if (!pointer) {
      BOOST_LOG(error) << "No unused pointer entries! Cancelling all active touches!"sv;
      cancel_all_active_touches(raw);
      pointer = pointer_by_id(raw, touch.pointerId, touch.eventType);
    }

    pointer->type = PT_TOUCH;

    auto &touchInfo = pointer->touchInfo;
    touchInfo.pointerInfo.pointerType = PT_TOUCH;

    // Populate shared pointer info fields
    populate_common_pointer_info(touchInfo.pointerInfo, touch_port, touch.eventType, touch.x, touch.y);

    touchInfo.touchMask = TOUCH_MASK_NONE;

    // Pressure and contact area only apply to in-contact pointers.
    //
    // The clients also pass distance and tool size for hovers, but Windows doesn't
    // provide APIs to receive that data.
    if (touchInfo.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) {
      if (touch.pressureOrDistance != 0.0f) {
        touchInfo.touchMask |= TOUCH_MASK_PRESSURE;

        // Convert the 0.0f..1.0f float to the 0..1024 range that Windows uses
        touchInfo.pressure = (UINT32) (touch.pressureOrDistance * 1024);
      } else {
        // The default touch pressure is 512
        touchInfo.pressure = 512;
      }

      if (touch.contactAreaMajor != 0.0f && touch.contactAreaMinor != 0.0f) {
        // For the purposes of contact area calculation, we will assume the touches
        // are at a 45 degree angle if rotation is unknown. This will scale the major
        // axis value by width and height equally.
        float rotationAngleDegs = touch.rotation == LI_ROT_UNKNOWN ? 45 : touch.rotation;

        float majorAxisAngle = rotationAngleDegs * (M_PI / 180);
        float minorAxisAngle = majorAxisAngle + (M_PI / 2);

        // Estimate the contact rectangle
        float contactWidth = (std::cos(majorAxisAngle) * touch.contactAreaMajor) + (std::cos(minorAxisAngle) * touch.contactAreaMinor);
        float contactHeight = (std::sin(majorAxisAngle) * touch.contactAreaMajor) + (std::sin(minorAxisAngle) * touch.contactAreaMinor);

        // Convert into screen coordinates centered at the touch location and constrained by screen dimensions
        touchInfo.rcContact.left = std::max<LONG>(touch_port.offset_x, touchInfo.pointerInfo.ptPixelLocation.x - std::floor(contactWidth / 2));
        touchInfo.rcContact.right = std::min<LONG>(touch_port.offset_x + touch_port.width, touchInfo.pointerInfo.ptPixelLocation.x + std::ceil(contactWidth / 2));
        touchInfo.rcContact.top = std::max<LONG>(touch_port.offset_y, touchInfo.pointerInfo.ptPixelLocation.y - std::floor(contactHeight / 2));
        touchInfo.rcContact.bottom = std::min<LONG>(touch_port.offset_y + touch_port.height, touchInfo.pointerInfo.ptPixelLocation.y + std::ceil(contactHeight / 2));

        touchInfo.touchMask |= TOUCH_MASK_CONTACTAREA;
      }
    } else {
      touchInfo.pressure = 0;
      touchInfo.rcContact = {};
    }

    if (touch.rotation != LI_ROT_UNKNOWN) {
      touchInfo.touchMask |= TOUCH_MASK_ORIENTATION;
      touchInfo.orientation = touch.rotation;
    } else {
      touchInfo.orientation = 0;
    }

    if (!inject_synthetic_pointer_input(raw->global, raw->touch, raw->touchInfo, raw->activeTouchSlots)) {
      auto err = GetLastError();
      BOOST_LOG(warning) << "Failed to inject virtual touch input: "sv << err;
      return;
    }

    // Clear pointer flags that should only remain set for one frame
    touchInfo.pointerInfo.pointerFlags &= ~EDGE_TRIGGERED_POINTER_FLAGS;

    // If we still have an active touch, refresh the touch state periodically
    if (raw->activeTouchSlots > 1 || touchInfo.pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
      raw->touchRepeatTask = task_pool.pushDelayed(repeat_touch, ISPI_REPEAT_INTERVAL, raw).task_id;
    }
  }

  /**
   * @brief Sends a pen event to the OS.
   * @param input The client-specific input context.
   * @param touch_port The current viewport for translating to screen coordinates.
   * @param pen The pen event.
   */
  void pen_update(client_input_t *input, const touch_port_t &touch_port, const pen_input_t &pen) {
    auto raw = (client_input_raw_t *) input;

    // Bail if we're not running on an OS that supports virtual pen input
    if (!raw->global->fnCreateSyntheticPointerDevice ||
        !raw->global->fnInjectSyntheticPointerInput ||
        !raw->global->fnDestroySyntheticPointerDevice) {
      BOOST_LOG(warning) << "Pen input requires Windows 10 1809 or later"sv;
      return;
    }

    // If there's not already a virtual pen device, create one now
    if (!raw->pen) {
      if (pen.eventType != LI_TOUCH_EVENT_CANCEL_ALL) {
        BOOST_LOG(info) << "Creating virtual pen input device"sv;
        raw->pen = raw->global->fnCreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_DEFAULT);
        if (!raw->pen) {
          auto err = GetLastError();
          BOOST_LOG(warning) << "Failed to create virtual pen device: "sv << err;
          return;
        }
      } else {
        // No need to cancel anything if we had no pen input device
        return;
      }
    }

    // Cancel pen repeat callbacks
    if (raw->penRepeatTask) {
      task_pool.cancel(raw->penRepeatTask);
      raw->penRepeatTask = nullptr;
    }

    raw->penInfo.type = PT_PEN;

    auto &penInfo = raw->penInfo.penInfo;
    penInfo.pointerInfo.pointerType = PT_PEN;
    penInfo.pointerInfo.pointerId = 0;

    // Populate shared pointer info fields
    populate_common_pointer_info(penInfo.pointerInfo, touch_port, pen.eventType, pen.x, pen.y);

    // Windows only supports a single pen button, so send all buttons as the barrel button
    if (pen.penButtons) {
      penInfo.penFlags |= PEN_FLAG_BARREL;
    } else {
      penInfo.penFlags &= ~PEN_FLAG_BARREL;
    }

    switch (pen.toolType) {
      default:
      case LI_TOOL_TYPE_PEN:
        penInfo.penFlags &= ~PEN_FLAG_ERASER;
        break;
      case LI_TOOL_TYPE_ERASER:
        penInfo.penFlags |= PEN_FLAG_ERASER;
        break;
      case LI_TOOL_TYPE_UNKNOWN:
        // Leave tool flags alone
        break;
    }

    penInfo.penMask = PEN_MASK_NONE;

    // Windows doesn't support hover distance, so only pass pressure/distance when the pointer is in contact
    if ((penInfo.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) && pen.pressureOrDistance != 0.0f) {
      penInfo.penMask |= PEN_MASK_PRESSURE;

      // Convert the 0.0f..1.0f float to the 0..1024 range that Windows uses
      penInfo.pressure = (UINT32) (pen.pressureOrDistance * 1024);
    } else {
      // The default pen pressure is 0
      penInfo.pressure = 0;
    }

    if (pen.rotation != LI_ROT_UNKNOWN) {
      penInfo.penMask |= PEN_MASK_ROTATION;
      penInfo.rotation = pen.rotation;
    } else {
      penInfo.rotation = 0;
    }

    // We require rotation and tilt to perform the conversion to X and Y tilt angles
    if (pen.tilt != LI_TILT_UNKNOWN && pen.rotation != LI_ROT_UNKNOWN) {
      auto rotationRads = pen.rotation * (M_PI / 180.f);
      auto tiltRads = pen.tilt * (M_PI / 180.f);
      auto r = std::sin(tiltRads);
      auto z = std::cos(tiltRads);

      // Convert polar coordinates into X and Y tilt angles
      penInfo.penMask |= PEN_MASK_TILT_X | PEN_MASK_TILT_Y;
      penInfo.tiltX = (INT32) (std::atan2(std::sin(-rotationRads) * r, z) * 180.f / M_PI);
      penInfo.tiltY = (INT32) (std::atan2(std::cos(-rotationRads) * r, z) * 180.f / M_PI);
    } else {
      penInfo.tiltX = 0;
      penInfo.tiltY = 0;
    }

    if (!inject_synthetic_pointer_input(raw->global, raw->pen, &raw->penInfo, 1)) {
      auto err = GetLastError();
      BOOST_LOG(warning) << "Failed to inject virtual pen input: "sv << err;
      return;
    }

    // Clear pointer flags that should only remain set for one frame
    penInfo.pointerInfo.pointerFlags &= ~EDGE_TRIGGERED_POINTER_FLAGS;

    // If we still have an active pen interaction, refresh the pen state periodically
    if (penInfo.pointerInfo.pointerFlags != POINTER_FLAG_NONE) {
      raw->penRepeatTask = task_pool.pushDelayed(repeat_pen, ISPI_REPEAT_INTERVAL, raw).task_id;
    }
  }

  void unicode(input_t &input, char *utf8, int size) {
    // We can do no worse than one UTF-16 character per byte of UTF-8
    WCHAR wide[size];

    int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, size, wide, size);
    if (chars <= 0) {
      return;
    }

    // Send all key down events
    for (int i = 0; i < chars; i++) {
      INPUT input {};
      input.type = INPUT_KEYBOARD;
      input.ki.wScan = wide[i];
      input.ki.dwFlags = KEYEVENTF_UNICODE;
      send_input(input);
    }

    // Send all key up events
    for (int i = 0; i < chars; i++) {
      INPUT input {};
      input.type = INPUT_KEYBOARD;
      input.ki.wScan = wide[i];
      input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
      send_input(input);
    }
  }

  /**
   * @brief Allocates a new virtual gamepad.
   * @param input The raw input structure.
   * @param id The virtual gamepad index.
   * @param metadata The virtual gamepad metadata.
   * @param feedback_queue The feedback queue used to transmit virtual gamepad output data.
   */
  int alloc_gamepad(input_t &input, const gamepad_id_t &id, const gamepad_arrival_t &metadata, feedback_queue_t feedback_queue) {
    // Cast the raw input structure
    auto raw = (input_raw_t *) input.get();

    // The selected gamepad type
    DUO_CONTROLLER_TYPE selectedGamepadType = DuoControllerTypeDualSenseEdge;

    if (config::input.gamepad == "xone"sv) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be Xbox One controller (manual selection)"sv;
      selectedGamepadType = DuoControllerTypeXbox;
    } else if (config::input.gamepad == "ds4"sv) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (manual selection)"sv;
      selectedGamepadType = DuoControllerTypeDualShock4;
    } else if (config::input.gamepad == "ds5"sv) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualSense controller (manual selection)"sv;
      selectedGamepadType = DuoControllerTypeDualSense;
    } else if (config::input.gamepad == "ds5e"sv) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualSense Edge controller (manual selection)"sv;
      selectedGamepadType = DuoControllerTypeDualSenseEdge;
    } else if (metadata.type == LI_CTYPE_PS) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (auto-selected by client-reported type)"sv;
      selectedGamepadType = DuoControllerTypeDualShock4;
    } else if (config::input.motion_as_ds4 && (metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO))) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (auto-selected by motion sensor presence)"sv;
      selectedGamepadType = DuoControllerTypeDualShock4;
    } else if (config::input.touchpad_as_ds4 && (metadata.capabilities & LI_CCAP_TOUCHPAD)) {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (auto-selected by touchpad presence)"sv;
      selectedGamepadType = DuoControllerTypeDualShock4;
    } else {
      BOOST_LOG(info) << "Gamepad " << id.globalIndex << " will be DualShock 4 controller (default)"sv;
      selectedGamepadType = DuoControllerTypeDualShock4;
    }

    if (selectedGamepadType == DuoControllerTypeXbox) {
      if (metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO)) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " has motion sensors, but they are not usable when emulating an Xbox One controller"sv;
      }
      if (metadata.capabilities & LI_CCAP_TOUCHPAD) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " has a touchpad, but it is not usable when emulating an Xbox One controller"sv;
      }
      if (metadata.capabilities & LI_CCAP_RGB_LED) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " has an RGB LED, but it is not usable when emulating an Xbox One controller"sv;
      }
    } else if (selectedGamepadType != DuoControllerTypeXbox) {
      if (!(metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO))) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " is emulating a PlayStation controller, but the client gamepad doesn't have motion sensors active"sv;
      }
      if (!(metadata.capabilities & LI_CCAP_TOUCHPAD)) {
        BOOST_LOG(warning) << "Gamepad " << id.globalIndex << " is emulating a PlayStation controller, but the client gamepad doesn't have a touchpad"sv;
      }
    }

    // Allocate a new virtual gamepad
    return raw->duo->alloc_gamepad_internal(id, feedback_queue, selectedGamepadType);
  }

  /**
   * @brief Frees the given virtual gamepad.
   * @param input The raw input structure.
   * @param nr The virtual gamepad index.
   */
  void free_gamepad(input_t &input, int nr) {
    // Cast the raw input structure
    auto raw = (input_raw_t *) input.get();

    // Free the virtual gamepad
    raw->duo->free_target(nr);
  }

  /**
   * @brief Snaps to the analog stick axis boundary.
   * @param val The analog stick axis.
   * @return The snapped analog stick axis.
   */
  static inline uint16_t snap_to_analog_stick_axis_boundary(int16_t val)
  {
    // Convert signed range (-32768..32767) to unsigned (0..65535)
    const uint16_t unsigned_val = static_cast<uint16_t>(static_cast<int32_t>(val) + 32768);

    // Distance to the lower bound (0)
    const uint16_t dist_low = unsigned_val;

    // Distance to the upper bound (65535)
    const uint16_t dist_high = 65535 - unsigned_val;

    // We're close enough to the lower boundary
    if (dist_low <= 10) {
      return 0;
    }

    // We're close enough to the upper boundary
    if (dist_high <= 10) {
      return 65535;
    }

    // Keep the original value
    return unsigned_val;
  }

  /**
   * @brief Updates the Xbox input report with the provided gamepad state.
   * @param gamepad The gamepad to update.
   * @param gamepad_state The gamepad button/axis state sent from the client.
   */
  static void xbox_update_state(gamepad_context_t &gamepad, const gamepad_state_t &gamepad_state) {
    auto &report = gamepad.xbox_report;

    auto flags = gamepad_state.buttonFlags;

    // Convert individual DPad flags to 0-8 value (0=None, 1=North, clockwise increments of 45°)
    bool up    = (flags & DPAD_UP) != 0;
    bool down  = (flags & DPAD_DOWN) != 0;
    bool left  = (flags & DPAD_LEFT) != 0;
    bool right = (flags & DPAD_RIGHT) != 0;

    if (up && right)      report.DPad = 2;  // Northeast
    else if (right && down) report.DPad = 4;  // Southeast
    else if (down && left)  report.DPad = 6;  // Southwest
    else if (left && up)    report.DPad = 8;  // Northwest
    else if (up)            report.DPad = 1;  // North
    else if (right)         report.DPad = 3;  // East
    else if (down)          report.DPad = 5;  // South
    else if (left)          report.DPad = 7;  // West
    else                    report.DPad = 0;  // None
    report.Start = (flags & START) != 0;
    report.Back = (flags & BACK) != 0;
    report.LeftStick = (flags & LEFT_STICK) != 0;
    report.RightStick = (flags & RIGHT_STICK) != 0;
    report.LeftBumper = (flags & LEFT_BUTTON) != 0;
    report.RightBumper = (flags & RIGHT_BUTTON) != 0;
    report.Guide = (flags & (HOME | MISC_BUTTON)) != 0;
    report.A = (flags & A) != 0;
    report.B = (flags & B) != 0;
    report.X = (flags & X) != 0;
    report.Y = (flags & Y) != 0;
    report.LeftTrigger = gamepad_state.lt;
    report.RightTrigger = gamepad_state.rt;
    report.LeftStickHorizontal = snap_to_analog_stick_axis_boundary(gamepad_state.lsX);
    report.LeftStickVertical = 65535 - snap_to_analog_stick_axis_boundary(gamepad_state.lsY);
    report.RightStickHorizontal = snap_to_analog_stick_axis_boundary(gamepad_state.rsX);
    report.RightStickVertical = 65535 - snap_to_analog_stick_axis_boundary(gamepad_state.rsY);
  }

  static std::uint8_t to_ds_triggerX(std::int16_t v) {
    return (v + std::numeric_limits<std::uint16_t>::max() / 2 + 1) / 257;
  }

  static std::uint8_t to_ds_triggerY(std::int16_t v) {
    auto new_v = -((std::numeric_limits<std::uint16_t>::max() / 2 + v - 1)) / 257;

    return new_v == 0 ? 0xFF : (std::uint8_t) new_v;
  }

  /**
   * @brief Updates the DualSense input report with the provided gamepad state.
   * @param gamepad The gamepad to update.
   * @param gamepad_state The gamepad button/axis state sent from the client.
   */
  static void ds_update_state(gamepad_context_t &gamepad, const gamepad_state_t &gamepad_state) {
    auto &report = gamepad.ds_report;

    report.LeftStickX = to_ds_triggerX(gamepad_state.lsX);
    report.LeftStickY = to_ds_triggerY(gamepad_state.lsY);

    report.RightStickX = to_ds_triggerX(gamepad_state.rsX);
    report.RightStickY = to_ds_triggerY(gamepad_state.rsY);

    report.TriggerLeft = gamepad_state.lt;
    report.TriggerRight = gamepad_state.rt;

    auto flags = gamepad_state.buttonFlags;

    if ((flags & DPAD_UP) != 0) {
      if ((flags & DPAD_LEFT) != 0) {
        report.DPad = 7;
      } else if ((flags & DPAD_RIGHT) != 0) {
        report.DPad = 1;
      } else {
        report.DPad = 0;
      }
    } else if ((flags & DPAD_DOWN) != 0) {
      if ((flags & DPAD_LEFT) != 0) {
        report.DPad = 5;
      } else if ((flags & DPAD_RIGHT) != 0) {
        report.DPad = 3;
      } else {
        report.DPad = 4;
      }
    } else if ((flags & DPAD_LEFT) != 0) {
      report.DPad = 6;
    } else if ((flags & DPAD_RIGHT) != 0) {
      report.DPad = 2;
    } else {
      report.DPad = 8;
    }

    report.ButtonSquare = (flags & X) != 0;
    report.ButtonCross = (flags & A) != 0;
    report.ButtonCircle = (flags & B) != 0;
    report.ButtonTriangle = (flags & Y) != 0;
    report.ButtonL1 = (flags & LEFT_BUTTON) != 0;
    report.ButtonR1 = (flags & RIGHT_BUTTON) != 0;
    report.ButtonL2 = gamepad_state.lt > 0;
    report.ButtonR2 = gamepad_state.rt > 0;
    report.ButtonCreate = (flags & BACK) != 0;
    report.ButtonOptions = (flags & START) != 0;
    report.ButtonL3 = (flags & LEFT_STICK) != 0;
    report.ButtonR3 = (flags & RIGHT_STICK) != 0;
    report.ButtonHome = (flags & HOME) != 0;
    report.ButtonPad = (flags & TOUCHPAD_BUTTON) != 0;
    report.ButtonMute = (flags & MISC_BUTTON) != 0;
    report.ButtonLeftFunction = (flags & PADDLE1) != 0;
    report.ButtonRightFunction = (flags & PADDLE2) != 0;
    report.ButtonLeftPaddle = (flags & PADDLE3) != 0;
    report.ButtonRightPaddle = (flags & PADDLE4) != 0;
  }

  /**
   * @brief Updates the DualShock 4 input report with the provided gamepad state.
   * @param gamepad The gamepad to update.
   * @param gamepad_state The gamepad button/axis state sent from the client.
   */
  static void ds4_update_state(gamepad_context_t &gamepad, const gamepad_state_t &gamepad_state) {
    auto &report = gamepad.ds4_report;

    report.LeftStickHorizontal = to_ds_triggerX(gamepad_state.lsX);
    report.LeftStickVertical = to_ds_triggerY(gamepad_state.lsY);

    report.RightStickHorizontal = to_ds_triggerX(gamepad_state.rsX);
    report.RightStickVertical = to_ds_triggerY(gamepad_state.rsY);

    report.LeftTrigger = gamepad_state.lt;
    report.RightTrigger = gamepad_state.rt;

    auto flags = gamepad_state.buttonFlags;

    if ((flags & DPAD_UP) != 0) {
      if ((flags & DPAD_LEFT) != 0) {
        report.DPad = 7;
      } else if ((flags & DPAD_RIGHT) != 0) {
        report.DPad = 1;
      } else {
        report.DPad = 0;
      }
    } else if ((flags & DPAD_DOWN) != 0) {
      if ((flags & DPAD_LEFT) != 0) {
        report.DPad = 5;
      } else if ((flags & DPAD_RIGHT) != 0) {
        report.DPad = 3;
      } else {
        report.DPad = 4;
      }
    } else if ((flags & DPAD_LEFT) != 0) {
      report.DPad = 6;
    } else if ((flags & DPAD_RIGHT) != 0) {
      report.DPad = 2;
    } else {
      report.DPad = 8;
    }

    report.Square = (flags & X) != 0;
    report.Cross = (flags & A) != 0;
    report.Circle = (flags & B) != 0;
    report.Triangle = (flags & Y) != 0;
    report.L1 = (flags & LEFT_BUTTON) != 0;
    report.R1 = (flags & RIGHT_BUTTON) != 0;
    report.L2 = gamepad_state.lt > 0;
    report.R2 = gamepad_state.rt > 0;
    report.Share = (flags & BACK) != 0;
    report.Options = (flags & START) != 0;
    report.L3 = (flags & LEFT_STICK) != 0;
    report.R3 = (flags & RIGHT_STICK) != 0;
    report.PS = (flags & HOME) != 0;
    report.Touchpad = (flags & TOUCHPAD_BUTTON) != 0;
  }

  /**
   * @brief Updates virtual gamepad with the provided gamepad state.
   * @param input The input context.
   * @param nr The gamepad index to update.
   * @param gamepad_state The gamepad button/axis state sent from the client.
   */
  void gamepad_update(input_t &input, int nr, const gamepad_state_t &gamepad_state) {
    auto duo = ((input_raw_t *) input.get())->duo;

    auto &gamepad = duo->gamepads[nr];

    if (gamepad.type == DuoControllerTypeXbox) {
      xbox_update_state(gamepad, gamepad_state);
      duo->SendReport(nr, gamepad.xbox_report);
    } else if (gamepad.type == DuoControllerTypeDualShock4) {
      ds4_update_state(gamepad, gamepad_state);
      duo->SendReport(nr, gamepad.ds4_report);
    } else {
      ds_update_state(gamepad, gamepad_state);
      duo->SendReport(nr, gamepad.ds_report);
    }
  }

  /**
   * @brief Sends a gamepad touch event to the OS.
   * @param input The global input context.
   * @param touch The touch event.
   */
  void gamepad_touch(input_t &input, const gamepad_touch_t &touch) {
    auto duo = ((input_raw_t *) input.get())->duo;

    auto &gamepad = duo->gamepads[touch.id.globalIndex];

    if (!gamepad.gp) {
      return;
    }

    // Touch is only supported on PlayStation controllers
    if (gamepad.type == DuoControllerTypeXbox) {
      return;
    }

    auto &touchData = gamepad.type == DuoControllerTypeDualShock4
      ? gamepad.ds4_report.TouchData
      : gamepad.ds_report.TouchData;

    uint8_t pointerIndex;
    if (touch.eventType == LI_TOUCH_EVENT_DOWN) {
      if (gamepad.available_pointers & 0x1) {
        // Reserve pointer index 0 for this touch
        gamepad.pointer_id_map[touch.pointerId] = pointerIndex = 0;
        gamepad.available_pointers &= ~(1 << pointerIndex);

        // Set pointer 0 down
        touchData.Finger[0].Index = 0;
        touchData.Finger[0].NotTouching = 0;
      } else if (gamepad.available_pointers & 0x2) {
        // Reserve pointer index 1 for this touch
        gamepad.pointer_id_map[touch.pointerId] = pointerIndex = 1;
        gamepad.available_pointers &= ~(1 << pointerIndex);

        // Set pointer 1 down
        touchData.Finger[1].Index = 1;
        touchData.Finger[1].NotTouching = 0;
      } else {
        BOOST_LOG(warning) << "No more free pointer indices! Did the client miss an touch up event?"sv;
        return;
      }
    } else if (touch.eventType == LI_TOUCH_EVENT_CANCEL_ALL) {
      // Raise both pointers
      touchData.Finger[0].Index = 0;
      touchData.Finger[0].NotTouching = 1;
      touchData.Finger[1].Index = 1;
      touchData.Finger[1].NotTouching = 1;

      // Remove all pointer index mappings
      gamepad.pointer_id_map.clear();

      // All pointers are now available
      gamepad.available_pointers = 0x3;
    } else {
      auto i = gamepad.pointer_id_map.find(touch.pointerId);
      if (i == gamepad.pointer_id_map.end()) {
        BOOST_LOG(warning) << "Pointer ID not found! Did the client miss a touch down event?"sv;
        return;
      }

      pointerIndex = (*i).second;

      if (touch.eventType == LI_TOUCH_EVENT_UP || touch.eventType == LI_TOUCH_EVENT_CANCEL) {
        // Remove the pointer index mapping
        gamepad.pointer_id_map.erase(i);

        // Set pointer up
        if (pointerIndex == 0) {
          touchData.Finger[0].Index = 0;
          touchData.Finger[0].NotTouching = 1;
        } else {
          touchData.Finger[1].Index = 1;
          touchData.Finger[1].NotTouching = 1;
        }

        // Free the pointer index
        gamepad.available_pointers |= (1 << pointerIndex);
      } else if (touch.eventType != LI_TOUCH_EVENT_MOVE) {
        BOOST_LOG(warning) << "Unsupported touch event for gamepad: "sv << (uint32_t) touch.eventType;
        return;
      }
    }

    // Touchpad is 1920x943 according to ViGEm
    uint16_t x = touch.x * 1920;
    uint16_t y = touch.y * 943;

    if (touch.eventType != LI_TOUCH_EVENT_CANCEL_ALL) {
      if (pointerIndex == 0) {
        touchData.Finger[0].FingerX = x;
        touchData.Finger[0].FingerY = y;
      } else {
        touchData.Finger[1].FingerX = x;
        touchData.Finger[1].FingerY = y;
      }
    }

    if (gamepad.type == DuoControllerTypeDualShock4) {
      duo->SendReport(touch.id.globalIndex, gamepad.ds4_report);
    } else {
      duo->SendReport(touch.id.globalIndex, gamepad.ds_report);
    }
  }

  /**
   * @brief Sends a gamepad motion event to the OS.
   * @param input The global input context.
   * @param motion The motion event.
   */
  void gamepad_motion(input_t &input, const gamepad_motion_t &motion) {
    auto duo = ((input_raw_t *) input.get())->duo;

    auto &gamepad = duo->gamepads[motion.id.globalIndex];
    if (!gamepad.gp) {
      return;
    }

    // Motion is only supported on PlayStation controllers
    if (gamepad.type == DuoControllerTypeXbox) {
      return;
    }

    if (gamepad.type == DuoControllerTypeDualShock4) {
      update_motion(gamepad.ds4_report, motion.motionType, motion.x, motion.y, motion.z);
      duo->SendReport(motion.id.globalIndex, gamepad.ds4_report);
    } else {
      update_motion(gamepad.ds_report, motion.motionType, motion.x, motion.y, motion.z);
      duo->SendReport(motion.id.globalIndex, gamepad.ds_report);
    }
  }

  /**
   * @brief Sends a gamepad battery event to the OS.
   * @param input The global input context.
   * @param battery The battery event.
   */
  void gamepad_battery(input_t &input, const gamepad_battery_t &battery) {
    // Synthetic gamepads have no battery
    return;
  }

  void freeInput(void *p) {
    auto input = (input_raw_t *) p;

    delete input;
  }

  std::vector<supported_gamepad_t> &supported_gamepads(input_t *input) {
    static std::vector gps {
      supported_gamepad_t {"auto", true, ""},
      supported_gamepad_t {"xone", true, ""},
      supported_gamepad_t {"ds4", true, ""},
      supported_gamepad_t {"ds5", true, ""},
      supported_gamepad_t {"ds5e", true, ""},
    };

    return gps;
  }

  /**
   * @brief Returns the supported platform capabilities to advertise to the client.
   * @return Capability flags.
   */
  platform_caps::caps_t get_capabilities() {
    platform_caps::caps_t caps = 0;

    // We support controller touchpad input as long as we're not emulating X360
    if (config::input.gamepad != "xone"sv) {
      caps |= platform_caps::controller_touch;
    }

    // We support pen and touch input on Win10 1809+
    if (GetProcAddress(GetModuleHandleA("user32.dll"), "CreateSyntheticPointerDevice") != nullptr) {
      if (config::input.native_pen_touch) {
        caps |= platform_caps::pen_touch;
      }
    } else {
      BOOST_LOG(warning) << "Touch input requires Windows 10 1809 or later"sv;
    }

    return caps;
  }
}  // namespace platf
