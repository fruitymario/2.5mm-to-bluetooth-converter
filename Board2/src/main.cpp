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
    // size_t bytesRead = i_in.readBytes(data, byteCount);
    // if(bytesRead < (size_t)byteCount){
    //     memset(data + bytesRead, 0, byteCount - bytesRead);
    // }

    // int raw = analogRead(RADIO_ADC_PIN);
    // int16_t radioSample = (int16_t)((raw - 2048) * 16);

    // int16_t* samples = (int16_t*)data;
    // int32_t sampleCount = byteCount / sizeof(int16_t);

    // for(int32_t i = 0; i + 1 < sampleCount; i += 2){
    //     int32_t left = samples[i];
    //     int32_t right = samples[i + 1];

    //     if(left > 32767) left = 32767;
    //     else if(left < -32768) left = -32768;
    //     if(right > 32767) right = 32767;
    //     else if(right < -32768) right = -32768;

    //     samples[i] = (int16_t)left;
    //     samples[i + 1] = (int16_t)right;
    // }
    // return byteCount;
    size_t bytesRead = i_in.readBytes(data, byteCount);
    if(bytesRead < (size_t)byteCount)
        memset(data + bytesRead, 0, byteCount - bytesRead);
    
    static uint32_t phase = 0;
    static int16_t radioSample = 0;

    int16_t* samples = (int16_t*)data;
    int32_t sampleCount = byteCount / sizeOf(int16_t);
    
    for(int32_t i = 0; i + 1 < sampleCount; i += 2){
        phase += RADIO_SAMPLE_RATE;
        if(phase >= OUTPUT_SAMPLE_RATE){
            phase -= OUTPUT_SAMPLE_RATE;
            if(ringTail != ringHead){
                radioSample = radioRing[ringTail];
                ringTail = (ringTail + 1) & (RADIO_RING_BUFFER_SIZE - 1);
            }
        }

        int32_t left = samples[i] + radioSample;
        int32_t right = samples[i + 1] + radioSample;

        if(left > 32767) left = 32767;
        if(left < -32768) left = -32768;
        if(right > 32767) right = 32767;
        if(right < -32768) right = -32768;

        samples[i] = (int16_t)left;
        samples[i + 1] = (int16_t)right;
    }
    return byteCount;
}

void onConnectionStateChanged(esp_a2d_connection_state_t state, void* obj){
    switch(state){
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.println("Connected to earbuds");
            //source.set_volume(127);
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

    xTaskCreatePinnedToCore(radioSampleTask, "Sampler", 4096, NULL, 1, NULL, 1);

    Serial.println("Board 2 ready");
}

void loop() {
    delay(2000);
    Serial.printf("Connection State: %s\n", source.is_connected() ? "Connected" : "Not Connected");
}