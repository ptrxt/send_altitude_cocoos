#ifndef BME280I2C_H_
#define BME280I2C_H_


namespace BME280 {

enum PresUnit { PresUnit_Pa };
enum TempUnit { TempUnit_Celsius };
enum ChipModel {ChipModel_BME280, ChipModel_BMP280};
}
 
class BME280I2C {
public:
  BME280I2C() {}
  ~BME280I2C() {}
  float pres(BME280::PresUnit u) {return 10.2;}
  float temp(BME280::TempUnit u) {return 30.2;}
  float hum() {return 85.6;}
  bool begin() {return true;}
  int chipModel() { return BME280::ChipModel_BME280; }
};
  


#endif