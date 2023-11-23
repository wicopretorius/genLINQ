#ifndef _ABB_B21_H
#define _ABB_B21_H

  //Json Document Setup
  StaticJsonDocument<250> doc;

  int slaveAddr =1;

enum DataType {
  SIGNED,
  UNSIGNED
};

struct ModbusVariable {
  char name[25];
  uint16_t registerAddress;
  uint8_t numberOfRegisters;
  float multiplier;
  int value;
  char unit[10];
  DataType dataType;
};

ModbusVariable ABB_B21[] = {
 {"V_L1N",0x5B00,2,0.1,0,"V",UNSIGNED},
 {"I_L1N",0x5B0C,2,0.01,0,"A",UNSIGNED},
 {"ActiveP_L1",0x5B14,2,0.01,0,"W",SIGNED},
 {"ApparentP_L1",0x5B24,2,0.01,0,"VA",SIGNED},
 {"ReActiveP_L1",0x5B1C,2,0.01,0,"VAr",SIGNED},
 {"Freq",0x5B2C,1,0.01,0,"Hz",UNSIGNED},
 {"PhAngleP_Tot",0x5B2D,1,0.1,0,"degrees",SIGNED},
 {"PF_Tot",0x5B3A,1,0.001,0," ",SIGNED},
 {"IQuadrant_Tot",0x5B3E,1,1,0," ",UNSIGNED}
};

const int numRegisters = sizeof(ABB_B21) / sizeof(ABB_B21[0]);

void readHoldingRegisters_ABB_B21() {
    //Serial.println("Reading Holding Register values ... ");

 for (int i = 0; i < numRegisters; i++) {
    // If the value is contained in two registers (32 bits)
    
    if (ABB_B21[i].numberOfRegisters == 2) {
      uint16_t highWord = ModbusRTUClient.holdingRegisterRead(slaveAddr, ABB_B21[i].registerAddress);
      uint16_t lowWord = ModbusRTUClient.holdingRegisterRead(slaveAddr, ABB_B21[i].registerAddress + 1);

      if (highWord != 0xFFFF && lowWord != 0xFFFF) {  // Check for read errors
        uint32_t combinedValue = ((uint32_t)highWord << 16) | lowWord;

        if (ABB_B21[i].dataType == SIGNED) {
          // Interpret as signed
          int32_t signedValue32bit = (int32_t)combinedValue;  // If needed, apply two's complement conversion
          ABB_B21[i].value = signedValue32bit;
        } else {
          // Interpret as unsigned
          uint32_t unsignedValue32bit = (uint32_t)combinedValue;
          ABB_B21[i].value = unsignedValue32bit;  // Store as int32_t; be aware this could cause issues if unsignedValue is greater than INT32_MAX
        }
       
        Serial.print(ABB_B21[i].name);
        Serial.print(": ");
        Serial.print(ABB_B21[i].value * ABB_B21[i].multiplier);
        Serial.println(ABB_B21[i].unit);

        doc[ABB_B21[i].name] = ABB_B21[i].value * ABB_B21[i].multiplier; //Load Key:Value pairs into JSON object
      } 
      else {
        if (strcmp(ABB_B21[i].name, "ReActiveP_L1") == 0){
          Serial.print("Calculating ");
          Serial.print(ABB_B21[i].name);
          Serial.print(": ");
          int32_t P, S, Q;
          for (int n = 0; n < numRegisters; n++) {
            if (strcmp(ABB_B21[n].name, "ActiveP_L1") == 0) P = ABB_B21[n].value;
            else if (strcmp(ABB_B21[n].name, "ApparentP_L1") == 0) S = ABB_B21[n].value;
          }
          Q = sqrt(S * S - P * P);
          ABB_B21[i].value = Q;
          Serial.print(ABB_B21[i].value * ABB_B21[i].multiplier);
          Serial.println(ABB_B21[i].unit);

          doc[ABB_B21[i].name] = ABB_B21[i].value * ABB_B21[i].multiplier; //Load Key:Value pairs into JSON object
        }
        else {
          Serial.print("Failed to read ");
          Serial.println(ABB_B21[i].name);
        }
      }
    }

    // If the value is contained in a single register (16 bits)
    else if (ABB_B21[i].numberOfRegisters == 1) {
      uint16_t value = ModbusRTUClient.holdingRegisterRead(slaveAddr, ABB_B21[i].registerAddress);

      if (value != 0xFFFF) {  // Check for read errors
        if (ABB_B21[i].dataType == SIGNED) {
          // Interpret as signed
          int16_t signedValue16bit = (int16_t)value;  // If needed, apply two's complement conversion
          ABB_B21[i].value = signedValue16bit;
        } else {
          // Interpret as unsigned
          uint16_t unsignedValue16bit = (uint16_t)value;
          ABB_B21[i].value = unsignedValue16bit;  // Store as int32_t; be aware this could cause issues if unsignedValue is greater than INT32_MAX
        }
        
        Serial.print(ABB_B21[i].name);
        Serial.print(": ");
        Serial.print(ABB_B21[i].value * ABB_B21[i].multiplier);
        Serial.println(ABB_B21[i].unit);

        doc[ABB_B21[i].name] = ABB_B21[i].value * ABB_B21[i].multiplier; //Load Key:Value pairs into JSON object
      } else {
        Serial.print("Failed to read ");
        Serial.println(ABB_B21[i].name);
      }
    }
  delay(1000);  
  }
}
#endif