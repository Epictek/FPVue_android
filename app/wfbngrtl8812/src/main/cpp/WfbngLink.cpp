#include <jni.h>
#include "WfbngLink.hpp"
#include <string>
#include <android/log.h>

#include <iostream>
#include <span>
#include <cstdint>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "libusb.h"
#include "devourer/src/logger.h"
#include "devourer/src/WiFiDriver.h"
#include "wfb-ng/src/wifibroadcast.hpp"
#include "RxFrame.h"

#include <sstream>
#include <iostream>
#include <iomanip>

#define TAG "com.geehe.fpvue"

std::string uint8_to_hex_string(const uint8_t *v, const size_t s) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < s; i++) {
        ss << std::hex << std::setw(2) << static_cast<int>(v[i]);
    }
    return ss.str();
}

WfbngLink::WfbngLink(JNIEnv* env, jobject context, jint fd): fd(fd), aggregator(nullptr) {}

int WfbngLink::run(JNIEnv* env, jobject context, jint wifiChannel) {
    int r;
    libusb_context *ctx = NULL;

    r = libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY);
    __android_log_print(ANDROID_LOG_DEBUG, TAG,
                        "libusb_set_option LIBUSB_OPTION_NO_DEVICE_DISCOVERY: %d", r);

    r = libusb_init(&ctx);
    if (r < 0) {
        return r;
    }

    struct libusb_device_handle *dev_handle;
    r = libusb_wrap_sys_device(ctx, (intptr_t) fd, &dev_handle);
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_wrap_sys_device: %d", r);
    if (r < 0) {
        libusb_exit(ctx);
        return r;
    }

    /*Check if kenel driver attached*/
    if (libusb_kernel_driver_active(dev_handle, 0)) {
        r = libusb_detach_kernel_driver(dev_handle, 0); // detach driver
        __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_detach_kernel_driver: %d", r);

    }
    r = libusb_claim_interface(dev_handle, 0);
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_claim_interface: %d", r);

    Logger_t log;
    WiFiDriver wifi_driver(log);
    //WiFiDriver wifi_driver;
    rtlDevice = wifi_driver.CreateRtlDevice(dev_handle);
    if (!rtlDevice) {
        __android_log_print(ANDROID_LOG_DEBUG, TAG,
                            "CreateRtlDevice error");
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, TAG,
                            "CreateRtlDevice success");
    }

    // Config
    // TODO(geehe) Get that form the android UI.
    int client_port = 5600;
    std::string client_addr = "127.0.0.1";
    uint32_t link_id = 7669206 ; // sha1 hash of link_domain="default"
    uint8_t radio_port = 0;
    uint64_t epoch = 0;

    uint32_t channel_id_f = (link_id << 8) + radio_port;
    uint32_t channel_id_be = htobe32(channel_id_f);
    uint8_t* channel_id_be8 = reinterpret_cast<uint8_t *>(&channel_id_be);

    __android_log_print(ANDROID_LOG_ERROR, TAG,
                        "filtering packets with link_id=%d(%s)",
                        link_id,
                        uint8_to_hex_string(channel_id_be8,4).c_str());

    try {
        Aggregator agg(client_addr, client_port, keyPath, epoch, channel_id_f);
        aggregator = &agg;

        auto packetProcessor = [&agg, channel_id_be8 ](const Packet &packet) {
            RxFrame frame(packet.Data);
            if (!frame.IsValidWfbFrame() || !frame.MatchesChannelID(channel_id_be8)) {
                return;
            }
            int8_t rssi[4] = {1,1,1,1};
            uint32_t freq = 0;
            int8_t noise[4] = {1,1,1,1};
            uint8_t antenna[4] = {1,1,1,1};
            agg.process_packet(packet.Data.data() + sizeof(ieee80211_header), packet.Data.size() - sizeof(ieee80211_header) - 4, 0, antenna, rssi, noise, freq, NULL);
        };
        rtlDevice->Init(packetProcessor, SelectedChannel{
                .Channel = static_cast<uint8_t>(wifiChannel),
                .ChannelOffset = 0,
                .ChannelWidth = CHANNEL_WIDTH_20,
        });
    } catch (const std::runtime_error& error) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "runtime_error: %s", error.what());
        return -1;
    }

    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Init done, releasing...");

    r = libusb_release_interface(dev_handle, 0);
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_release_interface: %d", r);

    libusb_exit(ctx);

    return 0;
}

void WfbngLink::stop(JNIEnv* env, jobject context) {
    if (rtlDevice) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Stopping rtlDevice");
        rtlDevice->should_stop = true;
    }
}


//----------------------------------------------------JAVA bindings---------------------------------------------------------------
inline jlong jptr(WfbngLink *wfbngLinkN) {
    return reinterpret_cast<intptr_t>(wfbngLinkN);
}
inline WfbngLink *native(jlong ptr) {
    return reinterpret_cast<WfbngLink *>(ptr);
}


extern "C" JNIEXPORT jlong JNICALL
Java_com_geehe_wfbngrtl8812_WfbNgLink_nativeInitialize(JNIEnv *env, jclass clazz,
                                                        jobject context, jint fd) {
    auto* p= new WfbngLink(env, context, fd);
    return jptr(p);
}

extern "C" JNIEXPORT void JNICALL
Java_com_geehe_wfbngrtl8812_WfbNgLink_nativeRun(JNIEnv * env, jclass clazz,jlong wfbngLinkN,jobject androidContext, jint wifiChannel){
    native(wfbngLinkN)->run(env,androidContext, wifiChannel);
}

extern "C" JNIEXPORT void JNICALL
Java_com_geehe_wfbngrtl8812_WfbNgLink_nativeStop(JNIEnv * env, jclass clazz,jlong wfbngLinkN,jobject androidContext){
    native(wfbngLinkN)->stop(env,androidContext);
}

extern "C" JNIEXPORT void JNICALL
Java_com_geehe_wfbngrtl8812_WfbNgLink_nativeCallBack(JNIEnv *env, jclass clazz, jobject wfbStatChangedI, jlong wfbngLinkN) {
    if (native(wfbngLinkN)->aggregator == nullptr) {
        return;
    }
    auto aggregator = native(wfbngLinkN)->aggregator;

    jclass jClassExtendsIWfbStatChangedI= env->GetObjectClass(wfbStatChangedI);

    jclass jcStats = env->FindClass("com/geehe/wfbngrtl8812/WfbNGStats");
    if(jcStats==nullptr){
        return;
    }
    jmethodID jcStatsConstructor = env->GetMethodID(jcStats, "<init>", "(IIIIIIII)V");
    if(jcStatsConstructor==nullptr){
        return;
    }
    auto stats=env->NewObject(jcStats,jcStatsConstructor,
                              (jint)aggregator->count_p_all,
                              (jint)aggregator->count_p_dec_err,
                              (jint)aggregator->count_p_dec_ok,
                              (jint)aggregator->count_p_fec_recovered,
                              (jint)aggregator->count_p_lost,
                              (jint)aggregator->count_p_bad,
                              (jint)aggregator->count_p_override,
                              (jint)aggregator->count_p_outgoing);
    if(stats==nullptr){
        return;
    }
    jmethodID onStatsChanged = env->GetMethodID(jClassExtendsIWfbStatChangedI, "onWfbNgStatsChanged", "(Lcom/geehe/wfbngrtl8812/WfbNGStats;)V");
    if(onStatsChanged==nullptr){
        return;
    }
    env->CallVoidMethod(wfbStatChangedI,onStatsChanged,stats);
    // aggregator->reset_stats();
}