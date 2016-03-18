
/******************************************************
 *Download RedBear "BLE Controller" APP 
 *From APP store(IOS)/Play Store(Android)                                         
 ******************************************************/
 
#include "pin_inf.h"
/******************************************************
 *                      Macros
 ******************************************************/
#if defined(ARDUINO) 
SYSTEM_MODE(MANUAL);//do not connect to cloud
#else
SYSTEM_MODE(AUTOMATIC);//connect to cloud
#endif

#define TXRX_BUF_LEN               8     

#define DEVICE_NAME                "BLE Controller"

#define PIN_CAPABILITY_NONE        0x00
#define PIN_CAPABILITY_DIGITAL     0x01
#define PIN_CAPABILITY_ANALOG      0x02
#define PIN_CAPABILITY_PWM         0x04
#define PIN_CAPABILITY_SERVO       0x08

#define ANALOG                     0x02 // analog pin in analogInput mode
#define PWM                        0x03 // digital pin in PWM output mode
#define SERVO                      0x04 // digital pin in Servo output mode

/******************************************************
 *              Device Variable Definitions
 ******************************************************/
Servo                             servos[TOTAL_PINS_NUM];

//for BLEController
byte pins_mode[TOTAL_PINS_NUM];
byte pins_state[TOTAL_PINS_NUM];
byte pins_pwm[TOTAL_PINS_NUM];
byte pins_servo[TOTAL_PINS_NUM];

static byte queryDone            = false;
static uint8_t reback_pin        = 0;
static uint8_t status_check_flag = 1;

static uint8_t duo_pin[TOTAL_PINS_NUM] = {D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15, D16, D17};

/******************************************************
 *               BLE Variable Definitions
 ******************************************************/
static uint8_t service1_uuid[16]       ={0x71,0x3d,0x00,0x00,0x50,0x3e,0x4c,0x75,0xba,0x94,0x31,0x48,0xf1,0x8d,0x94,0x1e};
static uint8_t service1_tx_uuid[16]    ={0x71,0x3d,0x00,0x03,0x50,0x3e,0x4c,0x75,0xba,0x94,0x31,0x48,0xf1,0x8d,0x94,0x1e};
static uint8_t service1_rx_uuid[16]    ={0x71,0x3d,0x00,0x02,0x50,0x3e,0x4c,0x75,0xba,0x94,0x31,0x48,0xf1,0x8d,0x94,0x1e};

static uint8_t  appearance[2]    = {0x00, 0x02};
static uint8_t  change[2]        = {0x00, 0x00};
static uint8_t  conn_param[8]    = {0x28, 0x00, 0x90, 0x01, 0x00, 0x00, 0x90, 0x01};

static uint16_t character1_handle = 0x0000;
static uint16_t character2_handle = 0x0000;
static uint16_t character3_handle = 0x0000;

static uint8_t characteristic1_data[TXRX_BUF_LEN]={0x01};
static uint8_t characteristic2_data[TXRX_BUF_LEN]={0x00};

static btstack_timer_source_t status_check;
static btstack_timer_source_t pin_status_back;

static advParams_t adv_params;
static uint8_t adv_data[]={0x02,0x01,0x06,0x08,0x08,'B','i','s','c','u','i','t',0x11,0x07,0x1e,0x94,0x8d,0xf1,0x48,0x31,0x94,0xba,0x75,0x4c,0x3e,0x50,0x00,0x00,0x3d,0x71};

 /******************************************************
 *               Function Definitions
 ******************************************************/

 
void deviceConnectedCallback(BLEStatus_t status, uint16_t handle) {
    switch (status){
        case BLE_STATUS_OK:
            Serial.println("Device connected!");
            break;
        default:
            break;
    }
}

void deviceDisconnectedCallback(uint16_t handle){
    Serial.println("Disconnected.");
}

void reportPinDigitalData(byte pin)
{
    //Serial.println("reportPinDigitalData");
    byte state = digitalRead(duo_pin[pin]);
    byte mode = pins_mode[pin];
    byte buf[] = {'G', pin, mode, state};
    memcpy(characteristic2_data, buf, 4);
    ble.sendNotify(character2_handle, characteristic2_data, 4);
}

void reportPinPWMData(byte pin)
{
    //Serial.println("reportPinPWMData");
    byte value = pins_pwm[pin];
    byte mode = pins_mode[pin];
    byte buf[] = {'G', pin, mode, value};
    memcpy(characteristic2_data, buf, min(4,TXRX_BUF_LEN));
    ble.sendNotify(character2_handle, characteristic2_data, min(4,TXRX_BUF_LEN));
}

void reportPinServoData(byte pin)
{
    //Serial.println("reportPinServoData");
    byte value = pins_servo[pin];
    byte mode = pins_mode[pin];
    byte buf[] = {'G', pin, mode, value};
    memcpy(characteristic2_data, buf, min(4,TXRX_BUF_LEN));
    ble.sendNotify(character2_handle, characteristic2_data, min(4,TXRX_BUF_LEN));
}

void reportPinCapability(byte pin)
{
    //Serial.println("reportPinCapability");
    byte buf[] = {'P', pin, 0x00};
    byte pin_cap = 0;

    if (IS_PIN_DIGITAL(pin))
        pin_cap |= PIN_CAPABILITY_DIGITAL;

    if (IS_PIN_ANALOG(pin))
        pin_cap |= PIN_CAPABILITY_ANALOG;

    if (IS_PIN_PWM(pin))
        pin_cap |= PIN_CAPABILITY_PWM;

    if (IS_PIN_SERVO(pin))
        pin_cap |= PIN_CAPABILITY_SERVO;

    buf[2] = pin_cap;
    //Serial.print("report: ");
    //for(uint8_t index=0; index<3; index++)
    //{
    //    Serial.print(buf[index], HEX);
    //    Serial.print(" ");
    //}
    //Serial.println(" ");
    memcpy(characteristic2_data, buf, 3);
    ble.sendNotify(character2_handle, characteristic2_data, 3);
}

void sendCustomData(uint8_t *buf, uint8_t len)
{
    //Serial.println("sendCustomData");
    uint8_t data[20] = "Z";

    memcpy(&data[1], buf, len);
    memcpy(characteristic2_data, data, len+1 );
    ble.sendNotify(character2_handle, characteristic2_data, len+1);
}

byte reportDigitalInput()
{
    static byte pin = 0;
    byte report = 0;

    if (!IS_PIN_DIGITAL(pin))
    {
        pin++;
        if (pin >= TOTAL_PINS_NUM)
            pin = 0;
        return 0;
    }

    if (pins_mode[pin] == INPUT)
    {
        byte current_state = digitalRead(duo_pin[pin]);

        if (pins_state[pin] != current_state)
        {
            pins_state[pin] = current_state;
            byte buf[] = {'G', pin, INPUT, current_state};
            memcpy(characteristic2_data, buf, 4);
            ble.sendNotify(character2_handle, characteristic2_data, 4);

            report = 1;
        }
    }
    pin++;
    if (pin >= TOTAL_PINS_NUM)
        pin = 0;

    return report;
}

byte reportPinAnalogData()
{
    static byte pin = 0;
    byte report = 0;

    if (!IS_PIN_DIGITAL(pin))
    {
        pin++;
        if (pin >= TOTAL_PINS_NUM)
            pin = 0;
        return 0;
    }

    if (pins_mode[pin] == ANALOG)
    {
        uint16_t value = analogRead(duo_pin[pin]);
        byte value_lo = value;
        byte value_hi = value>>8;

        byte mode = pins_mode[pin];
        mode = (value_hi << 4) | mode;

        byte buf[] = {'G', pin, mode, value_lo};
        memcpy(characteristic2_data, buf, 4);
        ble.sendNotify(character2_handle, characteristic2_data, 4);
    }

    pin++;
    if (pin >= TOTAL_PINS_NUM)
        pin = 0;

    return report;
}


static void status_check_handle(btstack_timer_source_t *ts)
{
    if(status_check_flag)
    {
        byte input_data_pending = reportDigitalInput();
        if(input_data_pending)
        {
            // reset
            ble.setTimer(ts, 20);
            ble.addTimer(ts);
            return;
         }
         reportPinAnalogData();
    }
    // reset
    ble.setTimer(ts, 20);
    ble.addTimer(ts);
}

void pin_status_back_handle(btstack_timer_source_t *ts)
{
    if(reback_pin < TOTAL_PINS_NUM)
    {
        reportPinCapability(reback_pin);
        if ( (pins_mode[reback_pin] == INPUT) || (pins_mode[reback_pin] == OUTPUT) )
            reportPinDigitalData(reback_pin);
        else if (pins_mode[reback_pin] == PWM)
            reportPinPWMData(reback_pin);
        else if (pins_mode[reback_pin] == SERVO)
            reportPinServoData(reback_pin);

        reback_pin++;
        // reset
        ble.setTimer(ts, 100);
        ble.addTimer(ts);
    }
    else
    {
        queryDone = true;
        uint8_t str[] = "ABC";
        sendCustomData(str, 3);
        
        pin_status_back.process = NULL;
        ble.removeTimer(&pin_status_back);
    }  
}


int gattWriteCallback(uint16_t value_handle, uint8_t *buf, uint16_t size)
{
    uint16_t index;
    
    if(character1_handle == value_handle)
    {
        memcpy(characteristic1_data, buf, size);
        Serial.print("value: ");
        for(uint8_t index=0; index<size; index++)
        {
            Serial.print(characteristic1_data[index], HEX);
            Serial.print(" ");
        }
        Serial.println(" ");
        //Process the data
        switch(buf[0])
        {
            case 'V': //query protocol version
            {
                byte buf_tx[] = {'V', 0x00, 0x00, 0x01};
                memcpy(characteristic2_data, buf_tx, 4);
                ble.sendNotify(character2_handle, characteristic2_data, 4);   
                break;     
            }   
            case 'C': // query board total pin count
            {
                byte buf_tx[2] = {'C', TOTAL_PINS_NUM};
                memcpy(characteristic2_data, buf_tx, 2);
                ble.sendNotify(character2_handle, characteristic2_data, 2);   
                break;                  
            }
            case 'M': // query pin mode
            {
                byte buf_tx[] = {'M', buf[1], pins_mode[ buf[2] ]};
                memcpy(characteristic2_data, buf_tx, 3);
                ble.sendNotify(character2_handle, characteristic2_data, 3);  
                break;                  
            }
            case 'S': // query pin mode
            {
                byte pin = buf[1];
                byte mode = buf[2];
                if ( IS_PIN_SERVO(pin) && mode != SERVO && servos[PIN_TO_SERVO(pin)].attached() )
                    servos[PIN_TO_SERVO(pin)].detach();
                /* ToDo: check the mode is in its capability or not */
                /* assume always ok */
                if(mode != pins_mode[pin])
                {   
                    pins_mode[pin] = mode;
                    if (mode == OUTPUT)
                    {
                        pinMode(duo_pin[pin], OUTPUT);
                        digitalWrite(duo_pin[pin], LOW);
                        pins_state[pin] = LOW;
                    }
                    else if (mode == INPUT)
                    {
                        pinMode(duo_pin[pin], INPUT);
                        digitalWrite(duo_pin[pin], HIGH);
                        pins_state[pin] = HIGH;
                    }
                    else if (mode == ANALOG)
                    {
                        if (IS_PIN_ANALOG(pin))
                        {
                            //pinMode(duo_pin[pin], AN_INPUT);
                        }
                    }
                    else if (mode == PWM)
                    {
                        if (IS_PIN_PWM(pin))
                        {
                            pinMode(duo_pin[PIN_TO_PWM(pin)], OUTPUT);
                            analogWrite(duo_pin[PIN_TO_PWM(pin)], 0);
                            pins_pwm[pin] = 0;
                            pins_mode[pin] = PWM;
                        }
                    }
                    else if (mode == SERVO)
                    {
                        if (IS_PIN_SERVO(pin))
                        {
                            pins_servo[pin] = 0;
                            pins_mode[pin] = SERVO;
                            if (!servos[PIN_TO_SERVO(pin)].attached())
                                servos[PIN_TO_SERVO(pin)].attach(duo_pin[PIN_TO_DIGITAL(pin)]);
                        }
                    }
                }
                //if (mode == ANALOG)
                   //reportPinAnalogData(pin);
                if ( (mode == INPUT) || (mode == OUTPUT) )
                {
                    reportPinDigitalData(pin);
                }
                else if (mode == PWM)
                {
                    reportPinPWMData(pin);
                }
                else if (mode == SERVO)
                {
                    reportPinServoData(pin);
                }
                break;                  
            }
            case 'G': // query pin data
            {
                byte pin = buf[1];
                reportPinDigitalData(pin);
                break;              
            }
            case 'T': // set pin digital state
            {
                byte pin = buf[1];
                byte state = buf[2];
                digitalWrite(duo_pin[pin], state);
                reportPinDigitalData(pin);
                break;              
            }
            case 'N': // set PWM
            {
                byte pin = buf[1];
                byte value = buf[2];

                analogWrite(duo_pin[PIN_TO_PWM(pin)], value);
                pins_pwm[pin] = value;
                reportPinPWMData(pin);
                break;                  
            }
            case 'O': // set Servo
            {
                byte pin = buf[1];
                byte value = buf[2];

                if (IS_PIN_SERVO(pin))
                    servos[PIN_TO_SERVO(pin)].write(value);
                pins_servo[pin] = value;
                reportPinServoData(pin);
                break;                  
            }
            case 'A': // query all pin status
            {
                reback_pin = 0;
                // set one-shot timer
                pin_status_back.process = &pin_status_back_handle;
                ble.setTimer(&pin_status_back, 100);//2000ms
                ble.addTimer(&pin_status_back);
                break;
            }
            case 'P': // query pin capability
            {
                byte pin = buf[1];
                reportPinCapability(pin);  
                break;
            }
            case 'Z':
            {
                byte len = buf[1];
                Serial.println("->");
                Serial.print("Received: ");
                Serial.print(len);
                Serial.println(" byte(s)");
                Serial.print(" Hex: ");
                for (int i=0;i<len;i++)
                  Serial.print(buf[i+2], HEX);
                Serial.println();
                break;
            }
        }
    }
    status_check_flag = 1;
}


void setup()
{
    Serial.begin(9600);
    delay(5000);
    Serial.println("BLEController demo.");
    //ble.debugLogger(true);
    ble.init();

    ble.onConnectedCallback(deviceConnectedCallback);
    ble.onDisconnectedCallback(deviceDisconnectedCallback);
    ble.onDataWriteCallback(gattWriteCallback);

    ble.addService(0x1800);
    ble.addCharacteristic(0x2A00, ATT_PROPERTY_READ|ATT_PROPERTY_WRITE, (uint8_t*)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.addCharacteristic(0x2A01, ATT_PROPERTY_READ, appearance, sizeof(appearance));
    ble.addCharacteristic(0x2A04, ATT_PROPERTY_READ, conn_param, sizeof(conn_param));
    ble.addService(0x1801);
    ble.addCharacteristic(0x2A05, ATT_PROPERTY_INDICATE, change, sizeof(change));

    ble.addService(service1_uuid);
    character1_handle = ble.addCharacteristicDynamic(service1_tx_uuid, ATT_PROPERTY_NOTIFY|ATT_PROPERTY_WRITE|ATT_PROPERTY_WRITE_WITHOUT_RESPONSE, characteristic1_data, TXRX_BUF_LEN);
    character2_handle = ble.addCharacteristicDynamic(service1_rx_uuid, ATT_PROPERTY_NOTIFY, characteristic2_data, TXRX_BUF_LEN);

    le_connection_parameter_range_t range;
    range.le_conn_interval_min = 20;
    range.le_conn_interval_max = 100;
    range.le_conn_latency_min  = 0;
    range.le_conn_latency_max  = 0;
    range.le_supervision_timeout_min = 10;
    range.le_supervision_timeout_max = 3200;
    ble.setConnParams(range);
    
    adv_params.adv_int_min = 0x00A0;
    adv_params.adv_int_max = 0x01A0;
    adv_params.adv_type    = 0;
    adv_params.dir_addr_type = 0;
    memset(adv_params.dir_addr,0,6);
    adv_params.channel_map = 0x07;
    adv_params.filter_policy = 0x00;
    
    ble.setAdvParams(&adv_params);
    
    ble.setAdvData(sizeof(adv_data), adv_data);

    ble.startAdvertising();

    /* Default all to digital input */
    for (int pin = 0; pin < TOTAL_PINS_NUM; pin++)
    {
        // Set pin to input with internal pull up
        pinMode(duo_pin[pin], INPUT);
        digitalWrite(duo_pin[pin], HIGH);

        // Save pin mode and state
        pins_mode[pin] = INPUT;
        pins_state[pin] = LOW;
    }

    // set one-shot timer
    status_check.process = &status_check_handle;
    ble.setTimer(&status_check, 100);//100ms
    ble.addTimer(&status_check);
    
    Serial.println("BLE start advertising.");
}

void loop()
{
    
}