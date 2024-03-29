/**
  ******************************************************************************
  * @file    main.c 
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    20-September-2012
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2012 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */


/* Includes ------------------------------------------------------------------*/
#include "main.h"

/** @addtogroup STM32F3-Discovery_Demo
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#define ABS(x)         (x < 0) ? (-x) : x

#define L3G_Sensitivity_250dps     (float)   114.285f         /*!< gyroscope sensitivity with 250 dps full scale [LSB/dps] */
#define L3G_Sensitivity_500dps     (float)    57.1429f        /*!< gyroscope sensitivity with 500 dps full scale [LSB/dps] */
#define L3G_Sensitivity_2000dps    (float)    14.285f	      /*!< gyroscope sensitivity with 2000 dps full scale [LSB/dps] */

#define LSM_Acc_Sensitivity_2g     (float)     1.0f            /*!< accelerometer sensitivity with 2 g full scale [LSB/mg] */
#define LSM_Acc_Sensitivity_4g     (float)     0.5f            /*!< accelerometer sensitivity with 4 g full scale [LSB/mg] */
#define LSM_Acc_Sensitivity_8g     (float)     0.25f           /*!< accelerometer sensitivity with 8 g full scale [LSB/mg] */
#define LSM_Acc_Sensitivity_16g    (float)     0.0834f         /*!< accelerometer sensitivity with 12 g full scale [LSB/mg] */

/* Private variables ---------------------------------------------------------*/
  RCC_ClocksTypeDef RCC_Clocks;
__IO uint32_t TimingDelay = 0;
__IO uint32_t UserButtonPressed = 0;
__IO float HeadingValue = 0.0f;  
float MagBuffer[3] = {0.0f}, AccBuffer[3] = {0.0f}, Buffer[3] = {0.0f};
uint8_t Xval, Yval = 0x00;

__IO uint8_t DataReady = 0;
extern __IO uint8_t PrevXferComplete;
__IO uint32_t USBConnectTimeOut = 100;

float fNormAcc,fSinRoll,fCosRoll,fSinPitch,fCosPitch = 0.0f, RollAng = 0.0f, PitchAng = 0.0f;
float fTiltedX,fTiltedY = 0.0f;
const uint8_t nrf_addr[] = RX_ADDR0;

/* Private function prototypes -----------------------------------------------*/
void update_AHRS(void);
/* Private functions ---------------------------------------------------------*/


/* callback functions */
void nrf_tx_done(uint8_t success)
{
}

void nrf_on_rx_data(const void* data, uint32_t len, uint8_t channel)
{
}

static uint8_t current_mode = DT_ATT;
void usb_get_data(const void* p, uint32_t len)
{
    const uint8_t* data = (const uint8_t*)p;
    if(data[0] == CMD_MODE){
        current_mode = data[1];
    }
}
#define SUM_COUNT   4
static sensor_value_t sensors = {0};
static uint8_t gyro_hungry = 0;
static uint8_t sensor_data_ready = 0;

void gyro_data_ready_irq(void)
{
    static uint32_t last_us = 0;
    uint32_t cur_us = current_us();
    static __IO uint32_t d_us;
    static __IO uint32_t d_us2;
    d_us = cur_us - last_us;
    last_us = cur_us;
    if(current_mode == DT_ATT)
    {
        int16_t gyro[3],acc[3],mag[3];
        read_raw_gyro(gyro);
        read_raw_acc(acc);
        read_raw_mag(mag);
        gyro_hungry = 0;
        sensors.gyroSum[ROLL] += gyro[1];
        sensors.gyroSum[PITCH] += gyro[0];
        sensors.gyroSum[YAW] += gyro[2];
        
        sensors.accSum[XAXIS] += acc[0];
        sensors.accSum[YAXIS] += acc[1];
        sensors.accSum[ZAXIS] += acc[2];
        
        sensors.magSum[XAXIS] += mag[0];
        sensors.magSum[YAXIS] += mag[1];
        sensors.magSum[ZAXIS] += mag[2];
        
        sensors.countSum++;
        if( sensors.countSum>= SUM_COUNT ){
            uint32_t cur_us = current_us();
            memcpy(sensors.gyroSumed,sensors.gyroSum,4*3*3);
            memset(sensors.gyroSum,0,4*3*3);
            sensors.countSumed = sensors.countSum;
            sensors.countSum = 0;
            sensor_data_ready = 1;
            if(sensors.lastSumTime_us){
                sensors.sumTime_us = cur_us - sensors.lastSumTime_us;
            }
            sensors.lastSumTime_us = cur_us;
        }
    }
    d_us2 = current_us() - last_us;
    d_us2++;
}

extern uint8_t frame_100Hz;
extern uint8_t frame_200Hz;
extern uint8_t frame_1Hz;
void prepare_rc_data(uint8_t* data)
{
    data[0] = DT_RCDATA;
    data[1] = get_pwm_values((uint16_t*)(data+2),8);
}
/**
  * @brief  Main program.
  * @param  None 
  * @retval None
  */
void setup_io_l3gd202(void);

float acc_scale_factor = 0.001;

float calc_acc_scale(uint32_t samples)
{
    uint32_t i;
    float acc[3] = {0.0f,0.0f,0.0f};
    int16_t a[3];
    for(i=0;i<samples;i++){
        read_raw_acc(a);
        acc[0] += (float)a[0];
        acc[1] += (float)a[1];
        acc[2] += (float)a[2];
    }
    acc[0] = acc[0] / samples;
    acc[1] = acc[1] / samples;
    acc[2] = acc[2] / samples;
    
    acc[0] = sqrt(acc[0]*acc[0]+acc[1]*acc[1]+acc[2]*acc[2]);
    return Gravity/acc[0];
}

int main(void)
{
    /* 2 bit for pre-emption priority, 2 bits for subpriority */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    setup_systick();
    enable_tick_count();
    setup_io_leds();
    setup_io_usb();
    
    init_sensor_config();
    
    GYRO_INIT();
    ACC_INIT();
    MAG_INIT();
    nrf_init();
    nrf_detect();
    nrf_rx_mode_dual(nrf_addr, 5, 40);
    {
        uint8_t status = nrf_read_reg(NRF_STATUS);
        nrf_write_reg(NRF_FLUSH_RX, 0xff);
        nrf_write_reg(NRF_FLUSH_TX, 0xff);
        nrf_write_reg(NRF_WRITE_REG|NRF_STATUS,status); // clear IRQ flags
    }
    
    pwm_input_init();
    
    USB_Init();
    
    acc_scale_factor = calc_acc_scale(200);
    compute_gyro_runtime_bias(sensors.gyro_rt_bias, 1000);
    
    // wait usb ready
    //while ((bDeviceState != CONFIGURED)&&(USBConnectTimeOut != 0))
    //{}
    current_mode = DT_ATT;
    
    // endless loop
    while(1)
    {
        uint8_t buf[64];
        if(frame_100Hz){
            frame_100Hz = 0;
            buf[0] = 0;
            if(current_mode == DT_RCDATA){
                prepare_rc_data(buf);
                usb_send_data(buf,64);
            }else if(current_mode == DT_SENSOR){
                buf[0] = DT_SENSOR;
                buf[1] = 9;
                read_raw_gyro((int16_t*)(buf+2));
                read_raw_acc((int16_t*)(buf+8));
                read_raw_mag((int16_t*)(buf+14));
                usb_send_data(buf,64);
            }
            if(buf[0]){
                usb_send_data(buf,64);
            }
        }
        
        if(sensor_data_ready){
            sensor_data_ready = 0;
            if(sensors.sumTime_us){
                update_AHRS();
                if(current_mode == DT_ATT){
                    buf[0] = DT_ATT;
                    buf[1] = 3;
                    sensors.height = 0.0;
                    memcpy(buf+2,sensors.attitude,sizeof(sensors.attitude) + 4);
                    usb_send_data(buf,64);
                }
                LED4_TOGGLE;
                LED5_TOGGLE;
                LED10_TOGGLE;
            }
            // process sensor data
        }
        if(frame_200Hz){
            frame_200Hz = 0;
            // if L3GD20 already contains gyro data, rising edge will not occur
            if( (current_mode == DT_ATT) && (gyro_hungry>1) ){
                if(L3GD20_INT2){
                    int16_t gyro[3];
                    read_raw_gyro(gyro);
                }
            }
            if(gyro_hungry < 10)
                gyro_hungry++;
        }
        if(frame_1Hz){
            frame_1Hz = 0;
            LED3_TOGGLE;
        }
    }
}

#define  COMPUTE_GYRO(x)    \
    sensors.gyro[x] = ((float)sensors.gyroSumed[x]/(float)sensors.countSumed + sensors.gyro_rt_bias[x])* GYRO_SCALE_FACTOR
#define COMPUTE_ACC(x) \
    sensors.acc[x] = ((float)sensors.accSumed[x]/(float)sensors.countSumed)* acc_scale_factor
#define COMPUTE_MAG(x) \
    sensors.mag[x] = (float)sensors.magSumed[x]

void update_AHRS(void)
{
    float dt = (float)sensors.sumTime_us/1000000.0f;
    COMPUTE_GYRO(ROLL);
    COMPUTE_GYRO(PITCH);
    COMPUTE_GYRO(YAW);
    
    COMPUTE_ACC(XAXIS);
    COMPUTE_ACC(YAXIS);
    COMPUTE_ACC(ZAXIS);
    
    COMPUTE_MAG(XAXIS);
    COMPUTE_MAG(YAXIS);
    COMPUTE_MAG(ZAXIS);
    
    MargAHRSupdate( sensors.gyro[ROLL], sensors.gyro[PITCH],  sensors.gyro[YAW],
                    sensors.acc[XAXIS], sensors.acc[YAXIS],   sensors.acc[ZAXIS],
                    sensors.mag[XAXIS], sensors.mag[YAXIS],   sensors.mag[ZAXIS],
                    sensorConfig.accelCutoff,  1,   dt );
    
    {
        extern float q0,q1,q2,q3,q0q0,q1q1,q2q2,q3q3;
        q0q0 = q0 * q0;
        q1q1 = q1 * q1;
        q2q2 = q2 * q2;
        q3q3 = q3 * q3;
        
        sensors.attitude[ROLL ] = atan2f( 2.0f * (q0 * q1 + q2 * q3), q0q0 - q1q1 - q2q2 + q3q3 );
        sensors.attitude[PITCH] = -asinf( 2.0f * (q1 * q3 - q0 * q2) );
        sensors.attitude[YAW  ] = atan2f( 2.0f * (q1 * q2 + q0 * q3), q0q0 + q1q1 - q2q2 - q3q3 );
    }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
