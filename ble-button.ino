// Designed for the following hardware:
//
// Heltec HTIT-WB32, or Heltec WiFi Kit 32
//
// Any of the $1.50 "anti-lost" BLE buttons on Aliexpress that support
// the button-service characteristic in this code.
//
// The purpose of this code is to do something when the user presses
// the button.

#include <BLEAddress.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <U8x8lib.h>

const int FRAME_LEN = 9;
const char FRAMES[] = "\\|/-\\|/-\\";
class Dashboard {
  public:
    Dashboard() {
      devices_found = -1;
      matches_found = -1;
      notifications = -1;
      spinner = 0;

      u8x8 = new U8X8_SSD1306_128X64_NONAME_SW_I2C(15, 4, 16);
      u8x8->begin();
      u8x8->setFont(u8x8_font_chroma48medium8_r);

      u8x8->clear();

      u8x8->print("Status:\r\n");
      u8x8->print("Found :\r\n");
      u8x8->print("Match :\r\n");
      u8x8->print("Notify:\r\n");
      u8x8->print("Error :\r\n");

      foundDevice();
      foundMatchingDevice();
      notify();
    }

    void printClip(const char* text, int limit) {
      const char* spaces = "                ";
      char buffer[16 + 1];
      strcpy(buffer, spaces);
      buffer[limit] = '\0';
      if (strlen(text) < limit) {
        limit = strlen(text);
      }
      strncpy(buffer, text, limit);
      u8x8->print(buffer);
    }

    void resetCounters() {
      devices_found = -1;
      matches_found = -1;
      notifications = -1;
      foundDevice();
      foundMatchingDevice();
      notify();
    }

    void resetNotifications() {
      notifications = -1;
      notify();
    }

    void setStatus(const char* text) {
      u8x8->setCursor(8, 0);
      printClip(text, 8);
    }

    void foundDevice() {
      u8x8->setCursor(8, 1);
      u8x8->print(++devices_found);
    }

    void foundMatchingDevice() {
      u8x8->setCursor(8, 2);
      u8x8->print(++matches_found);
    }

    void notify() {
      u8x8->setCursor(8, 3);
      u8x8->print(++notifications);
    }

    void setError(const char* text) {
      u8x8->setCursor(8, 4);
      printClip(text, 8);
    }

    void showActivity() {
      u8x8->setCursor(15, 7);
      u8x8->drawGlyph(15, 7, FRAMES[spinner++]);
      if (spinner >= FRAME_LEN) {
        spinner = 0;
      }
    }

    void setSwitchState(bool state) {
      u8x8->setCursor(0, 7);
      if (state) {
        u8x8->print("Light ON ");
      } else {
        u8x8->print("Light OFF");
      }
    }

  private:
    U8X8_SSD1306_128X64_NONAME_SW_I2C *u8x8;
    int devices_found;
    int matches_found;
    int notifications;
    int spinner;
};

Dashboard dash;

BLEScan* scanner = NULL;
class AdvertisedDeviceCallbackHandler : public BLEAdvertisedDeviceCallbacks {
  public:
    AdvertisedDeviceCallbackHandler() {
      buttonAddress = NULL;
    }

    void onResult(BLEAdvertisedDevice advertisedDevice) {
      dash.foundDevice();
      if (advertisedDevice.getName() == "iTAG            ") {
        dash.foundMatchingDevice();
        buttonAddress = new BLEAddress(advertisedDevice.getAddress());
        dash.setStatus("found");
        scanner->stop();
      }
    }

    BLEAddress *getButtonAddress() {
      return buttonAddress;
    }

  private:
    BLEAddress *buttonAddress;
};
AdvertisedDeviceCallbackHandler deviceCallbacks;

const int SCAN_TIME = 5;
BLEAddress* scan() {
  dash.resetCounters();
  dash.setStatus("scanning");
  BLEScanResults foundDevices = scanner->start(SCAN_TIME);
  dash.setStatus("end scan");

  scanner->setAdvertisedDeviceCallbacks(NULL);

  BLEAddress* result = NULL;
  if (deviceCallbacks.getButtonAddress()) {
    result = new BLEAddress(*(deviceCallbacks.getButtonAddress()->getNative()));
  }

  return result;
}

static bool lightSwitchState = false;
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  dash.notify();

  lightSwitchState = !lightSwitchState;
  dash.setSwitchState(lightSwitchState);
}

static BLEUUID SERVICE_UUID("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID CHARACTERISTIC_UUID("0000ffe1-0000-1000-8000-00805f9b34fb");
BLEClient* client = NULL;
bool connect(BLEAddress *address) {
  dash.setStatus("conn");
  if (client->isConnected() || client->connect(*address)) {
    dash.setStatus("conn OK");
    BLERemoteService* remoteService = client->getService(SERVICE_UUID);

    if (remoteService == nullptr) {
      dash.setError("svc FAIL");
      client->disconnect();
      return false;
    }
    dash.setStatus("svc OK");

    BLERemoteCharacteristic* buttonCharacteristic =
      remoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (buttonCharacteristic == nullptr) {
      dash.setError("char FAIL");
      client->disconnect();
      return false;
    }
    dash.setStatus("char OK");

    buttonCharacteristic->registerForNotify(notifyCallback);

    dash.setStatus("waiting");
    dash.resetNotifications();

    return true;
  }
  dash.setError("conn FAIL");
  return false;
}

bool connected = false;
bool identified = false;

BLEAddress* buttonAddress = NULL;
void setup() {
  BLEDevice::init("ble-button-scanner");

  client = BLEDevice::createClient();
  
  scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setAdvertisedDeviceCallbacks(&deviceCallbacks, true);

  dash.setSwitchState(false);

  dash.setStatus("OK");
}

void loop() {
  if (client->isConnected()) {
    // We're in the normal state where we're waiting for something to happen.
    // Sleep a bit so we don't spin the processor unnecessarily.
    dash.showActivity();
    dash.setError("");
    delay(1000);
  } else {
    if (buttonAddress) {
      //      buttonClient->disconnect();
      if (connect(buttonAddress)) {
        return;
      }
      delete buttonAddress;
    }
    buttonAddress = scan();
  }
}

