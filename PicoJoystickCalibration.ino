
#include <vector>
#include <algorithm> 
#include "hardware/flash.h"

#define FLASH_TARGET_OFFSET (256 * 1024) // Offset from the start of flash (beyond the program)


struct adcchanneldata {
  size_t centre, centrelow, centrehigh, low,high;
  void print() {
    Serial.print("Low: ");
    Serial.print(low);
    Serial.print("\tCentre Low: ");
    Serial.print(centrelow);
    Serial.print("\tCentre: ");
    Serial.print(centre);
    Serial.print("\tCentre High: ");
    Serial.print(centrehigh);
    Serial.print("\tHigh: ");
    Serial.println(high);
  }
};

struct adcdata {
  adcchanneldata X;
  adcchanneldata Y;
  adcchanneldata Z; 

  void print() {
    Serial.print("X: ");
    X.print();
    Serial.print("Y: ");
    Y.print();
    Serial.print("Z: ");
    Z.print();
  }
};

void write_to_flash(const adcdata &data) {
    // Flash page size (256 bytes) and sector size (4096 bytes)
    const uint32_t flash_sector_size = FLASH_SECTOR_SIZE;

    // Allocate a buffer to hold the flash sector (must be flash_sector_size)
    uint8_t flash_sector[flash_sector_size];
    
    // Copy the existing data from flash (to preserve it for other data in the sector)
    const uint8_t *flash_target_address = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(flash_sector, flash_target_address, flash_sector_size);

    // Place the new struct data in the buffer (overwriting desired bytes)
    memcpy(flash_sector, &data, sizeof(adcdata));

    // Disable interrupts to safely write to flash
    uint32_t interrupts = save_and_disable_interrupts();

    // Erase the flash sector
    flash_range_erase(FLASH_TARGET_OFFSET, flash_sector_size);

    // Write the updated buffer back to flash
    flash_range_program(FLASH_TARGET_OFFSET, flash_sector, flash_sector_size);

    // Restore interrupts
    restore_interrupts(interrupts);
}

adcdata read_from_flash() {
    const adcdata *data = (const adcdata *)(XIP_BASE + FLASH_TARGET_OFFSET);
    return *data; // Return a copy of the struct
}

adcdata joystick;
std::vector<size_t> xdata, ydata, zdata;

enum phases {COLLECTION, COMPLETE} phase;

void setup() {
  // put your setup code here, to run once:
  Serial.begin();
  analogReadResolution(12);
  phase = phases::COLLECTION;
  while(!Serial) {
    ;;
  }
}
size_t calculateMedian(std::vector<size_t> &vec) {
    // Sort the vector
    std::sort(vec.begin(), vec.end());

    // Check if the size of the vector is odd or even
    size_t size = vec.size();
    if (size % 2 == 0) {
        // Even number of elements: return the average of the two middle elements
        return (vec[size / 2 - 1] + vec[size / 2]) / 2.0;
    } else {
        // Odd number of elements: return the middle element
        return vec[size / 2];
    }
}

void analyseChannel(std::vector<size_t> &recording, adcchanneldata &chan) {
  size_t chMin=99999, chMax=0;
  for(auto &v: recording) {
    if (v < chMin) {
      chMin = v;
    }
    if (v > chMax) {
      chMax = v;
    }
  }
  chan.centrelow = chMin;
  chan.centrehigh = chMax;
  chan.centre = calculateMedian(recording);
}

void analyse() {
  analyseChannel(xdata, joystick.X);
  analyseChannel(ydata, joystick.Y);
  analyseChannel(zdata, joystick.Z);
}

void loop() {
  if (phase == phases::COLLECTION) {
    size_t x = analogRead(26);
    size_t y = analogRead(27);
    size_t z = analogRead(28);
    Serial.print(millis());
    Serial.print("-- x: ");
    Serial.println(x);
    xdata.push_back(x);
    Serial.print("\ty: ");
    Serial.println(y);
    ydata.push_back(y);
    Serial.print("\tz: ");
    Serial.println(z);
    zdata.push_back(z);
    delay(10);
    if (millis() > 12000) {
      analyse();
      joystick.print();
      Serial.println("Writing to flash");
      write_to_flash(joystick);     
      Serial.println("Testing read from flash");
      adcdata tmp = read_from_flash();
      tmp.print();
      phase = phases::COMPLETE;
    }
  }
}
