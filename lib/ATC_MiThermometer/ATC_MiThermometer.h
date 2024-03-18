///////////////////////////////////////////////////////////////////////////////////////////////////
// ATC_MiThermometer.h
//
// Bluetooth low energy thermometer/hygrometer sensor library for ESP32.
// For sensors running ATC_MiThermometer firmware (see https://github.com/pvvx/ATC_MiThermometer)
//
// https://github.com/matthias-bs/ESP32_ATC_MiThermometer_Library
//
// Based on:
// ---------
// ESP32 BLE for Arduino (https://github.com/espressif/arduino-esp32/tree/master/libraries/BLE)
// LYWSD03MMC.py by JsBergbau (https://github.com/JsBergbau/MiTemperature2)
//
// created: 05/2022
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
// 20220521 Created
// 20220527 Changed to a class/into a library
//
// ToDo: 
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef ATC_MiThermometer_h
#define ATC_MiThermometer_h

//#define ATC_MiThermometer_DEBUG
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>


#define DEBUG_PORT Serial
#define DEBUG_PRINT(...) { Serial.print(__VA_ARGS__); }
#define DEBUG_PRINTLN(...) { Serial.println(__VA_ARGS__); }


// #ifdef ATC_MiThermometer_DEBUG
//     #define DEBUG_PORT Serial
//     #define DEBUG_PRINT(...) { DEBUG_PORT.print(__VA_ARGS__); }
//     #define DEBUG_PRINTLN(...) { DEBUG_PORT.println(__VA_ARGS__); }
// #else
//     #define DEBUG_PRINT(...) {}
//     #define DEBUG_PRINTLN(...) {}
// #endif


/*!
 * \struct MiThData_S 
 * 
 *  MiThermometer Data
 */
struct MiThData_S {
        bool        valid;          //!< data valid
        int16_t     temperature;    //!< temperature x 100°C
        uint8_t    humidity;       //!< humidity x 100%
        uint16_t    batt_voltage;   //!< battery voltage [mv]
        uint8_t     batt_level;     //!< battery level   [%]
        int16_t     rssi;           //!< RSSI [dBm]
};

typedef struct MiThData_S MiThData_t;


/*!
 * \class ATC_MiThermometer
 *
 * \brief BLE ATC_MiThermometer thermometer/hygrometer sensor client
 */
class ATC_MiThermometer {
    public:
        /*!
         * \brief Constructor.
         *
         * \param known_sensors List of BT MAC addresses of known sensors
         */
        ATC_MiThermometer(std::vector<std::string> known_sensors) {
            _known_sensors = known_sensors;
            data.resize(known_sensors.size());
        };

        /*!
         * \brief Initialization.
         */
        void begin(void);
        
        /*!
         * \brief Delete results from BLEScan buffer to release memory.
         */        
        void clearScanResults(void) {
            _pBLEScan->clearResults();
        };
        
        /*!
         * \brief Get data from sensors by running a BLE scan.
         *
         * \param duration Scan duration in seconds
         */                
        unsigned getData(uint32_t duration);
        
        /*!
         * \brief Set sensor data invalid.
         *
         */                        
        void resetData(void);
        
        /*!
         * \brief Sensor data.
         */
        std::vector<MiThData_t>  data;
        
    protected:
        std::vector<std::string> _known_sensors;
        BLEScan*                 _pBLEScan;
};
#endif
