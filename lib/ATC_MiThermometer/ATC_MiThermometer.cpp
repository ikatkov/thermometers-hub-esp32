///////////////////////////////////////////////////////////////////////////////////////////////////
// ATC_MiThermometer.cpp
//
// Bluetooth low energy thermometer/hygrometer sensor client for MCUs supported by NimBLE-Arduino.
// For sensors running ATC_MiThermometer firmware (see https://github.com/pvvx/ATC_MiThermometer)
//
// https://github.com/matthias-bs/ATC_MiThermometer
//
// Based on:
// ---------
// NimBLE-Arduino by h2zero (https://github.com/h2zero/NimBLE-Arduino)
// LYWSD03MMC.py by JsBergbau (https://github.com/JsBergbau/MiTemperature2)
//
// created: 11/2022
//
//
// MIT License
//
// Copyright (c) 2022 Matthias Prinke
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// History:
//
// 20221123 Created
// 20221223 Added support for ATC1441 format
//
// ToDo: 
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <ATC_MiThermometer.h>


/*!
 * \class MyAdvertisedDeviceCallbacks
 * 
 * \brief Callback for advertised device found during scan
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice* advertisedDevice) {
    log_d("Advertised Device: %s", advertisedDevice->toString().c_str());
    /*
     * Here we add the device scanned to the whitelist based on service data but any
     * advertised data can be used for your preffered data.
     */
    if (advertisedDevice->haveServiceData()) {
      /* If this is a device with data we want to capture, add it to the whitelist */
      if (advertisedDevice->getServiceData(NimBLEUUID("181A")) != "") {
        log_d("Adding %s to whitelist", std::string(advertisedDevice->getAddress()).c_str());
        NimBLEDevice::whiteListAdd(advertisedDevice->getAddress());
      }
    }
  }
};


// Set up BLE scanning
void ATC_MiThermometer::begin(void)
{
    NimBLEDevice::init("");
    _pBLEScan = BLEDevice::getScan(); //create new scan
    _pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    _pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
    _pBLEScan->setInterval(100);
    _pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);
    _pBLEScan->setWindow(99);  // less or equal setInterval value
}


// Get sensor data by running BLE device scan
unsigned ATC_MiThermometer::getData(uint32_t duration) {
    BLEScanResults foundDevices = _pBLEScan->start(duration, false /* is_continue */);
  
    log_d("Whitelist contains:");
    for (auto i=0; i<NimBLEDevice::getWhiteListCount(); ++i) {
        log_d("%s", NimBLEDevice::getWhiteListAddress(i).toString().c_str());
    }
  
    log_d("Assigning scan results...");
    for (unsigned i=0; i<foundDevices.getCount(); i++) {
        
        // Match all devices found against list of known sensors
        for (unsigned n = 0; n < _known_sensors.size(); n++) {
            log_d("Found: %s  comparing to: %s", 
                  foundDevices.getDevice(i).getAddress().toString().c_str(), 
                  BLEAddress(_known_sensors[n]).toString().c_str());
            if (foundDevices.getDevice(i).getAddress() == BLEAddress(_known_sensors[n])) {
                log_d(" -> Match! Index: %d", n);
                data[n].valid = true;
                
                int len = foundDevices.getDevice(i).getServiceData().length();
                log_d("Length of ServiceData: %d", len);
                
                if (len == 15) {
                    log_d("Custom format");
                    // Temperature
                    int temp_msb = foundDevices.getDevice(i).getServiceData().c_str()[7];
                    int temp_lsb = foundDevices.getDevice(i).getServiceData().c_str()[6];
                    data[n].temperature = (temp_msb << 8) | temp_lsb;

                    // Humidity
                    int hum_msb = foundDevices.getDevice(i).getServiceData().c_str()[9];
                    int hum_lsb = foundDevices.getDevice(i).getServiceData().c_str()[8];
                    data[n].humidity = (hum_msb << 8) | hum_lsb;

                    // Battery voltage
                    int volt_msb = foundDevices.getDevice(i).getServiceData().c_str()[11];
                    int volt_lsb = foundDevices.getDevice(i).getServiceData().c_str()[10];
                    data[n].batt_voltage = (volt_msb << 8) | volt_lsb;

                    // Battery state [%]
                    data[n].batt_level = foundDevices.getDevice(i).getServiceData().c_str()[12];         
                }
                else if (len == 13) {
                    log_d("ATC1441 format");
                    
                    // Temperature
                    int temp_lsb = foundDevices.getDevice(i).getServiceData().c_str()[7];
                    int temp_msb = foundDevices.getDevice(i).getServiceData().c_str()[6];
                    data[n].temperature  = (temp_msb << 8) | temp_lsb;
                    data[n].temperature *= 10;

                    // Humidity
                    data[n].humidity  = foundDevices.getDevice(i).getServiceData().c_str()[8];
                    data[n].humidity *= 100;

                    // Battery voltage
                    int volt_lsb = foundDevices.getDevice(i).getServiceData().c_str()[11];
                    int volt_msb = foundDevices.getDevice(i).getServiceData().c_str()[10];
                    data[n].batt_voltage = (volt_msb << 8) | volt_lsb;

                    // Battery state [%]
                    data[n].batt_level = foundDevices.getDevice(i).getServiceData().c_str()[9];
                } else {
                    log_d("Unknown ServiceData format");
                }
                
                // Received Signal Strength Indicator [dBm]
                data[n].rssi = foundDevices.getDevice(i).getRSSI();
            } else {
                log_d();
            }
        }
    }
    return foundDevices.getCount();
}

        
// Set all array members invalid
void ATC_MiThermometer::resetData(void)
{
    for (int i=0; i < _known_sensors.size(); i++) {
        data[i].valid = false;
    }
}