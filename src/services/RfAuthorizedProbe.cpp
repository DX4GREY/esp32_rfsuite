#include "services/RfAuthorizedProbe.h"
#include "core/RfEnvironmentState.h"
#include "drivers/RadioManager.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "services/SessionRecorder.h"
RfAuthorizedProbe rfAuthorizedProbe;
bool RfAuthorizedProbe::start(){
#if !RF_LAB_TX_ENABLED
    Serial.println("Authorized probe is not compiled in this RX-only build.");return false;
#else
    if(running||!radioManager.hasAnyRadio())return false;
    auto&c=rfEnvironmentState.config;if(c.probeChannel>125||c.probeIntervalMs<20||c.probeIntervalMs>5000||c.probePacketCount<1||c.probePacketCount>1000||c.probePayloadSize<1||c.probePayloadSize>32||c.probeMaxDurationSeconds<1||c.probeMaxDurationSeconds>60)return false;
    rfEnvironmentAnalyzer.stop();stopRequested=false;sent=0;startedMs=millis();endedMs=startedMs;running=true;
    if(xTaskCreatePinnedToCore(taskEntry,"RFAuthProbe",4096,this,1,&task,0)!=pdPASS){running=false;return false;}return true;
#endif
}
void RfAuthorizedProbe::stop(){stopRequested=true;}
bool RfAuthorizedProbe::stopAndWait(uint32_t timeoutMs){
    stopRequested=true;const uint32_t started=millis();
    while(task&&millis()-started<timeoutMs)vTaskDelay(pdMS_TO_TICKS(2));
    return task==nullptr;
}
void RfAuthorizedProbe::taskEntry(void*p){static_cast<RfAuthorizedProbe*>(p)->run();vTaskDelete(nullptr);}
void RfAuthorizedProbe::run(){
#if RF_LAB_TX_ENABLED
    const auto c=rfEnvironmentState.config;uint8_t payload[32]={0x52,0x46,0x50,0x52,0x4f,0x42,0x45,0};
    while(!stopRequested&&sent<c.probePacketCount&&millis()-startedMs<c.probeMaxDurationSeconds*1000UL){payload[7]=sent&0xff;if(radioManager.transmitProbePacket(c.probeChannel,c.probePa,c.probeDataRate,c.probePayloadSize,payload))sent++;uint32_t until=millis()+c.probeIntervalMs;while(!stopRequested&&(int32_t)(until-millis())>0)vTaskDelay(pdMS_TO_TICKS(5));}
#endif
    radioManager.enterRxMode();endedMs=millis();
#if RF_LAB_TX_ENABLED
    sessionRecorder.recordProbeSummary(rfEnvironmentState.config.probeChannel,rfEnvironmentState.config.probePa,rfEnvironmentState.config.probeDataRate,rfEnvironmentState.config.probePayloadSize,sent,rfEnvironmentState.config.probeIntervalMs,endedMs-startedMs);
#endif
    running=false;task=nullptr;
}
