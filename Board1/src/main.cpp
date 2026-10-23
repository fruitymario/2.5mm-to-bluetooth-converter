#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

#define PIN_BIT_CLOCK       26
#define PIN_WORD_SELECT     25
#define PIN_DATA            22

I2SStream out;
BluetoothA2DPSink sink(out);

volatile uint32_t packetCount = 0;
volatile bool isConnected = false;

void onDataReceivedCallBack() {
    packetCount++;
}

void onConnectionStateChange(esp_a2d_connection_state_t state, void *obj){
    switch(state){
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            isConnected = true;
            Serial.println("Phone connected");
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            isConnected = false;
            Serial.println("Phone disconnected");
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);

    I2SConfig config = out.defaultConfig();
    config.pin_bck = PIN_BIT_CLOCK;
    config.pin_ws = PIN_WORD_SELECT;
    config.pin_data = PIN_DATA;

    out.begin(config);
    sink.set_on_data_received(onDataReceivedCallBack);
    sink.set_on_connection_state_changed(onConnectionStateChange);
    sink.start("RadioRelay-Board1");

    Serial.println("Board 1 ready");
}

void loop() {
    delay(2000);
    if(isConnected){
        Serial.printf("connected. Packets received so far: %u\n", packetCount);
    }
    else{
        Serial.println("Waiting for phone to connect...");
    }
}
