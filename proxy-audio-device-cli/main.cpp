//
//  main.cpp
//  proxy-audio-device-cli
//
//  Created by Max Coplan on 5/19/23.
//

#include <CoreAudio/CoreAudio.h>
#include <iostream>
#include <map>
#include <vector>

#include "AudioDevice.h"
#include "ProxyAudioDevice.h"

static char const *CFString_to_string(CFStringRef cfstr) {
    char const *name = CFStringGetCStringPtr(cfstr, CFStringBuiltInEncodings::kCFStringEncodingUTF8);
    if (name != nullptr)
        return name;
    CFIndex size = (sizeof(char) * CFStringGetLength(cfstr)) + 1;
    char *buf = (char *)malloc(size);
    bool success = CFStringGetCString(cfstr, buf, size, CFStringBuiltInEncodings::kCFStringEncodingUTF8);
    assert(success);
    name = buf;
    return name;
}

static void list_audio_devices() {
    std::vector<AudioObjectID> currentDeviceList =
        AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();
    for (AudioObjectID &deviceId : currentDeviceList) {
        CFStringRef name_ref = AudioDevice::copyObjectName(deviceId);
        char const *name = CFString_to_string(name_ref);
        std::printf("%s\n", name);
    }
}

static void set_this_process_as_configurator() {
    /// see the comment in
    /// `ProxyAudioDevice.cpp > SetBoxPropertyData > case kAudioObjectPropertyIdentify`
    /// for the rationale behind this.  Also see
    /// https://github.com/briankendall/proxy-audio-device/issues/29#issuecomment-1555429290

    AudioDeviceID proxy_audio_box = AudioDevice::audioDeviceIDForBoxUID(CFSTR(kBox_UID));

    if (proxy_audio_box == kAudioObjectUnknown) {
        assert(false && "Could not find Proxy Audio Device.  Did you install the driver?");
    }

    if (!AudioDevice::setIdentifyValue(proxy_audio_box, getpid())) {
        assert(false && "unable to set current process as configurator");
    }
}

/**
 FIXME: don't get all the audio devices, just search for the name you're looking
 for
 */
static std::map</** name*/ std::string, /**uid*/ std::string> get_audio_devices() {
    std::map<std::string, std::string> audio_devices;
    std::vector<AudioObjectID> currentDeviceList =
        AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();
    for (AudioObjectID &deviceId : currentDeviceList) {
        std::string name = ({
            CFStringRef name_ref = AudioDevice::copyObjectName(deviceId);
            CFString_to_string(name_ref);
        });
        std::string uid = ({
            CFStringRef device_uid_ref = AudioDevice::copyDeviceUID(deviceId);
            CFString_to_string(device_uid_ref);
        });
        audio_devices.insert({name, uid});
    }
    return audio_devices;
}

static std::string find_audio_device_uid_from_name(std::string device_name) {
    std::map<std::string, std::string> audio_devices = get_audio_devices();
    std::map<std::string, std::string>::iterator device_found = audio_devices.find(device_name);
    if (device_found == audio_devices.end()) {
        std::cerr << "no audio with this name found.  Did you run `--list`?  Did you install the driver?\n";
        assert(false && "no device found");
    }
    std::string device_uid = device_found->second;
    return device_uid;
}

static AudioDeviceID audioDeviceIDForBoxUID(std::string device_uid) {
    return AudioDevice::audioDeviceIDForBoxUID(
        CFStringCreateWithCString(nullptr, device_uid.c_str(), CFStringBuiltInEncodings::kCFStringEncodingUTF8));
}

static void set_audio_device_as_active(std::string device_uid) {
    AudioDeviceID proxy_audio_box = audioDeviceIDForBoxUID(kBox_UID);
    std::string formatted_device_uid = ({
        static constexpr char output_literal[] = "outputDevice=";
        std::size_t output_literal_len = sizeof(output_literal) - 1;
        std::size_t sizeof_the_thing = sizeof(char) * (output_literal_len + device_uid.size()) + 1;
        char *buf = (char *)malloc(sizeof_the_thing);
        for (std::size_t i = 0; i < output_literal_len; ++i) {
            buf[i] = output_literal[i];
        }
        for (std::size_t i = 0; i < device_uid.size(); ++i) {
            buf[i + output_literal_len] = device_uid[i];
        }
        buf;
    });

    AudioDevice::setObjectName(proxy_audio_box,
                               CFStringCreateWithCString(nullptr,
                                                         formatted_device_uid.c_str(),
                                                         CFStringBuiltInEncodings::kCFStringEncodingUTF8));
}

static void select_audio_device(std::string device_name) {
    set_this_process_as_configurator();
    std::string device_uid = find_audio_device_uid_from_name(device_name);
    set_audio_device_as_active(device_uid);
    return;
}

#define USAGE_TEXT                                                                                                     \
    "USAGE:"                                                                                                           \
    "\n--list:\tPrint all possible target audio devices"                                                               \
    "\n--select '<DEVICE>':\tSets the current output to DEVICE"                                                        \
    "\n\t note: DEVICE should be selected from one of the devices output "                                             \
    "using `--list`\n"

static void print_usage() {
    std::printf(USAGE_TEXT);
}

static void print_usage_err() {
    std::cerr << USAGE_TEXT;
}

enum class mode {
    err,
    list,
    select,
    help,
};

static mode parse_cli_mode(int argc, char const *argv[]) {
    if (argc <= 1)
        return mode::err;
    if (strcmp(argv[1], "--list") == 0)
        return mode::list;
    if (strcmp(argv[1], "--select") == 0)
        return mode::select;
    if (strcmp(argv[1], "--help") == 0)
        return mode::help;
    else
        return mode::err;
}

int main(int argc, const char *argv[]) {
    switch (parse_cli_mode(argc, argv)) {
        case mode::err: {
            print_usage_err();
            return 1;
        };
        case mode::list: {
            list_audio_devices();
            return 0;
        };
        case mode::select: {
            {
                if (argc <= 2) {
                    std::cerr << "DEVICE not provided\n";
                    print_usage_err();
                    return 1;
                }
                if (argc >= 4) {
                    std::cerr << "Too many arguments for `--select`.  Make sure to quote "
                                 "the device name if it has spaces in it.\n"
                              << "e.g. `--select \"My Speakers\"` \n";
                    print_usage_err();
                    return 1;
                }
            }
            // TODO: support `--select=<device>`
            const char *audio_device = argv[2];
            select_audio_device(audio_device);
            return 0;
        };
        case mode::help: {
            print_usage();
            return 0;
        }
    }
}
