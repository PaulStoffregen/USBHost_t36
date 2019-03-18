float gx, gy, gz;
float ax, ay, az;
float pitch, roll;
uint16_t xc, yc; 
uint8_t isTouch;
int16_t xc_old, yc_old;

void printAngles(){
  //test function calls
  float gx, gy, gz;
  getAccel(ax, ay, az);
  Serial.printf("Accel-g's: %f, %f, %f\n", ax, ay, az);
  getGyro(gx, gy, gz);
  Serial.printf("Gyro-deg/sec: %f, %f, %f\n", gx, gy, gz);

  getAngles(pitch, roll);
  Serial.printf("Pitch/Roll: %f, %f\n", pitch, roll);

  getCoords(xc, yc, isTouch);
}

void getCoords(uint16_t &xc, uint16_t &yc, uint8_t &isTouch){

  //uint8_t finger = 0;  //only getting finger 1
  uint8_t Id = 0;


  // Trackpad touch 1: id, active, x, y
  xc = ((psAxis[37] & 0x0f) << 8) | psAxis[36];
  yc = psAxis[38] << 4 | ((psAxis[37] & 0xf0) >> 4),

  isTouch = psAxis[35] >> 7;
  if(xc != xc_old || yc != yc_old){
    Serial.printf("Touch: %d, %d, %d, %d\n", psAxis[33], isTouch, xc, yc);
    xc_old = xc;
    yc_old = yc;
  }
}

void getAccel( float &ax,  float &ay,  float &az){
      int accelx = (int16_t)(psAxis[20]<<8) | psAxis[19];
      int accelz = (int16_t)(psAxis[22]<<8) | psAxis[21];
      int accely = (int16_t)(psAxis[24]<<8) | psAxis[23];
  
      ax = (float) accelx/8192;
      ay = (float) accely/8192;
      az = (float) accelz/8192;
}

void getAngles(float &p, float &r){
  getAccel( ax,  ay,  az);
  p = (atan2f(ay, az) + PI) * RAD_TO_DEG;
  r = (atan2f(ax, az) + PI) * RAD_TO_DEG;
}

void getGyro(float &gx, float &gy, float &gz){
  int gyroy = (int16_t)(psAxis[14]<<8) | psAxis[13];
  int gyroz = (int16_t)(psAxis[16]<<8) | psAxis[15];
  int gyrox = (int16_t)(psAxis[18]<<8) | psAxis[17];

  gx = (float) gyrox * RAD_TO_DEG/1024;
  gy = (float) gyroy * RAD_TO_DEG/1024;
  gz = (float) gyroz * RAD_TO_DEG/1024;
}
  
