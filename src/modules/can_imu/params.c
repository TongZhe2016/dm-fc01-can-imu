/**
 * Output source: 0 BMI088, 1 ICM45686 diagnostic
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI_SOURCE, 0);
/**
 * Enable 200 Hz CAN data stream
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI_TX_EN, 1);
/**
 * Heater enable bitmask: 1 BMI088, 2 ICM45686, 3 both
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI_HEAT_EN, 0);
/**
 * Target IMU temperature in Celsius
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI_HEAT_T, 48.0);
/**
 * Frequency of each of the two cascaded low pass poles
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI_LPF_HZ, 60.0);
/**
 * Calibration and configuration generation
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI_EPOCH, 1);
/**
 * IMU 0 AX B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AXB, 0.0);
/**
 * IMU 0 AX S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AXS, 1.0);
/**
 * IMU 0 AX T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AXT, 0.0);
/**
 * IMU 0 AY B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AYB, 0.0);
/**
 * IMU 0 AY S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AYS, 1.0);
/**
 * IMU 0 AY T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AYT, 0.0);
/**
 * IMU 0 AZ B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AZB, 0.0);
/**
 * IMU 0 AZ S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AZS, 1.0);
/**
 * IMU 0 AZ T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_AZT, 0.0);
/**
 * IMU 0 GX B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GXB, 0.0);
/**
 * IMU 0 GX S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GXS, 1.0);
/**
 * IMU 0 GX T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GXT, 0.0);
/**
 * IMU 0 GY B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GYB, 0.0);
/**
 * IMU 0 GY S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GYS, 1.0);
/**
 * IMU 0 GY T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GYT, 0.0);
/**
 * IMU 0 GZ B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GZB, 0.0);
/**
 * IMU 0 GZ S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GZS, 1.0);
/**
 * IMU 0 GZ T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_GZT, 0.0);
/**
 * IMU 0 A calibration valid
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI0_ACAL, 0);
/**
 * IMU 0 G calibration valid
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI0_GCAL, 0);
/**
 * IMU 1 AX B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AXB, 0.0);
/**
 * IMU 1 AX S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AXS, 1.0);
/**
 * IMU 1 AX T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AXT, 0.0);
/**
 * IMU 1 AY B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AYB, 0.0);
/**
 * IMU 1 AY S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AYS, 1.0);
/**
 * IMU 1 AY T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AYT, 0.0);
/**
 * IMU 1 AZ B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AZB, 0.0);
/**
 * IMU 1 AZ S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AZS, 1.0);
/**
 * IMU 1 AZ T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_AZT, 0.0);
/**
 * IMU 1 GX B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GXB, 0.0);
/**
 * IMU 1 GX S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GXS, 1.0);
/**
 * IMU 1 GX T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GXT, 0.0);
/**
 * IMU 1 GY B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GYB, 0.0);
/**
 * IMU 1 GY S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GYS, 1.0);
/**
 * IMU 1 GY T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GYT, 0.0);
/**
 * IMU 1 GZ B: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GZB, 0.0);
/**
 * IMU 1 GZ S: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GZS, 1.0);
/**
 * IMU 1 GZ T: bias / scale / temperature slope
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_GZT, 0.0);
/**
 * IMU 1 A calibration valid
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI1_ACAL, 0);
/**
 * IMU 1 G calibration valid
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI1_GCAL, 0);

/**
 * Persisted boot session counter
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI_BOOT, 0);

/**
 * Maximum FIFO publication rate required by the sensor helper classes
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(IMU_GYRO_RATEMAX, 2000);

/**
 * Start CAN IMU automatically after USB maintenance startup
 * @group CAN IMU
 */
PARAM_DEFINE_INT32(CI_AUTOSTART, 0);

/**
 * IMU 0 heater: Proportional gain, duty per degree C (0..1)
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_HEAT_P, 0.10);

/**
 * IMU 0 heater: Integral gain, duty per degree C per second (0..0.1)
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI0_HEAT_I, 0.01);

/**
 * IMU 1 heater: Proportional gain, duty per degree C (0..1)
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_HEAT_P, 0.10);

/**
 * IMU 1 heater: Integral gain, duty per degree C per second (0..0.1)
 * @group CAN IMU
 */
PARAM_DEFINE_FLOAT(CI1_HEAT_I, 0.01);
