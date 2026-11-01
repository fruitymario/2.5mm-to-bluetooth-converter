#include "esp_bt.h"
#include "AudioTools.h"
#include "BluetoothA2DPSource.h"

#define EARBUDS_NAME                "Galaxy Buds4 Pro"
#define RADIO_ADC_PIN               34

#define PIN_BIT_CLOCK               26
#define PIN_WORD_SELECT             25
#define PIN_DATA                    22
#define BITS_PER_SAMPLE             16
#define OUTPUT_SAMPLE_RATE          44100

#define RADIO_SAMPLE_RATE           8000
#define RADIO_GAIN                  4
#define RADIO_RING_BUFFER_SIZE      512

I2SStream i_in;
BluetoothA2DPSource source;

volatile int16_t radioRing[RADIO_RING_BUFFER_SIZE];
volatile uint32_t ringHead = 0;
volatile uint32_t ringTail = 0;

volatile int radioMin = 4095;
volatile int radioMax = 0;

void radioSampleTask(void* param){
    float bias = 2048.0f;
    uint32_t nextPeriod = micros() + 1000000UL / OUTPUT_SAMPLE_RATE;

    while(true){
        int raw = analogRead(RADIO_ADC_PIN);

        if(raw < radioMin) radioMin = raw;
        if(raw > radioMax) radioMax = raw;

        dc += (raw - dc) * .001f;

        int32_t sample = (int32_t)((raw - dc) * 16 * RADIO_GAIN);
        if(sample > 32767)
            sample = 32767;
        else if (sample < -32768)
            sample = -32768;

        uint32_t nextHead = (ringHead + 1) & (RING_SIZE - 1);
        if(nextHead != ringTail){
            radioRing[ringHead] = (int16_t)sample;
            ringHead = nextHead;
        }
        
        while((int32_t)(micros() - nextPeriod) < 0) {}
    }
}

int32_t getMixedAudio(uint8_t* data, int32_t byteCount){
 
}

void onConnectionStateChanged(esp_a2d_connection_state_t state, void* obj){
    switch(state){
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.println("Connected to earbuds");
            source.set_volume(127);
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            Serial.println("Connecting to earbuds...");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            Serial.println("Disconnected from earbuds");
            break;
        default:
            break;
    }
}

void setup(){
    Serial.begin(115200);

    I2SConfig config = i_in.defaultConfig(RX_MODE);
    config.pin_bck = PIN_BIT_CLOCK;
    config.pin_ws = PIN_WORD_SELECT;
    config.pin_data = PIN_DATA;
    config.bits_per_sample = BITS_PER_SAMPLE;
    config.channels = 2;
    config.sample_rate = OUTPUT_SAMPLE_RATE;
    config.is_master = false;
    
    i_in.begin(config);

    source.set_data_callback(getMixedAudio);
    source.set_on_connection_state_changed(onConnectionStateChanged);
    source.start(EARBUDS_NAME);

    Serial.println("Board 2 ready");
}

void loop() {
    delay(2000);
    Serial.printf("Connection State: %s\n", source.is_connected() ? "Connected" : "Not Connected");
}