float pitch, roll;
float gx, gy, gz;
int16_t mx, my, mz;
uint16_t xc, yc; 
uint8_t isTouch;
float ax, ay, az;
int16_t xc_old, yc_old;

/* Decode 12-bit signed value (assuming two's complement) */
#define TWELVE_BIT_SIGNED(x) (((x) & 0x800)?(-(((~(x)) & 0xFFF) + 1)):(x))

#define Model_ZCM1 1
//#define Model_ZCM2 0

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


void printPS3MotionAngles(){
  //test function calls
  float gx, gy, gz;
  getPS3MotionAccel(ax, ay, az);
  Serial.printf("Accel-g's: %f, %f, %f\n", ax, ay, az);
  getPS3MotionGyro(gx, gy, gz);
  Serial.printf("Gyro-deg/sec: %f, %f, %f\n", gx, gy, gz);
  getPS3MotionMag(mx, my, mz);
  Serial.printf("Mag: %d, %d, %d\n", mx, my, mz);

  getPS3MotionAngles(pitch, roll);
  Serial.printf("Pitch/Roll: %f, %f\n", pitch, roll);
}


void getPS3MotionAccel( float &ax,  float &ay,  float &az){
    int accelx = (psAxis[15]<<8 | psAxis[14]);
    int accely = (psAxis[17]<<8 | psAxis[16]);
    int accelz = (psAxis[19]<<8 | psAxis[18]);  
  #if defined(Model_ZCM1)
  	accelx = accelx-0x8000;
  	accely = accely-0x8000;
  	accelz = accelz-0x8000;
  #elif defined(Model_ZCM2)
    accelx = (accelx & 0x8000) ? (-(~accelx & 0xFFFF) + 1) : accelx;
    accely = (accely & 0x8000) ? (-(~accely & 0xFFFF) + 1) : accely;
    accelz = (accelz & 0x8000) ? (-(~accelz & 0xFFFF) + 1) : accelz;
  #endif
    ax = (float) accelx/4096;
  	ay = (float) accely/4096;
  	az = (float) accelz/4096;
  
}

void getPS3MotionAngles(float &p, float &r){
	getAccel( ax,  ay,  az);
	p = (atan2f(ay, az) + PI) * RAD_TO_DEG;
	r = (atan2f(ax, az) + PI) * RAD_TO_DEG;
}

void getPS3MotionGyro(float &gx, float &gy, float &gz){
    int gyrox = (psAxis[21]<<8 | psAxis[20]);
    int gyroy = (psAxis[23]<<8 | psAxis[22]);
    int gyroz = (psAxis[25]<<8 | psAxis[24]);
  #if defined(Model_ZCM1)
  	gyrox = gyrox-0x8000;
  	gyroy = gyroy-0x8000;
  	gyroz = gyroz-0x8000;
  #elif defined(Model_ZCM2)
    gyrox = (gyrox & 0x8000) ? (-(~gyrox & 0xFFFF) + 1) : gyrox;
    gyroy = (gyroy & 0x8000) ? (-(~gyroy & 0xFFFF) + 1) : gyroy;
    gyroz = (gyroz & 0x8000) ? (-(~gyroz & 0xFFFF) + 1) : gyroz;
  #endif

  	gx = (float) gyrox * RAD_TO_DEG/1024;
	gy = (float) gyroy * RAD_TO_DEG/1024;
	gz = (float) gyroz * RAD_TO_DEG/1024;
  
}
	
void getPS3MotionMag(int16_t &mx, int16_t &my, int16_t &mz){
  #if defined(Model_ZCM1)
      mx = TWELVE_BIT_SIGNED(((psAxis[33] & 0x0F) << 8) |
                 psAxis[34]);

      my = TWELVE_BIT_SIGNED((psAxis[35] << 4) |
                 (psAxis[36] & 0xF0) >> 4);

      mz = TWELVE_BIT_SIGNED(((psAxis[36] & 0x0F) << 8) |
                  psAxis[37]);
  #elif defined(Model_ZCM2)
      // NOTE: This model does not have magnetometers
      Serial.println("Not avail for ZCM2!");
  #endif
}
