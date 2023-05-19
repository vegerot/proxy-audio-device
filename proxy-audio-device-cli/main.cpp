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

static void list_audio_devices() {
  std::vector<AudioObjectID> currentDeviceList =
      AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();
  for (AudioObjectID &deviceId : currentDeviceList) {
    CFStringRef name_ref = AudioDevice::copyObjectName(deviceId);
    char const *name = CFStringGetCStringPtr(
        name_ref, CFStringBuiltInEncodings::kCFStringEncodingUTF8);
    if (name == nullptr) {
      CFIndex size = (sizeof(char) * CFStringGetLength(name_ref)) + 1;
      char *buf = (char *)malloc(size);
      bool success = CFStringGetCString(
          name_ref, buf, size, CFStringBuiltInEncodings::kCFStringEncodingUTF8);
      assert(success);
      name = buf;
    }
    std::printf("%s\n", name);
  }
}
/**
 TODO: don't get all the audio devices, just search for the name you're looking
 for
 */
static std::map</** name*/ std::string, /**uid*/ std::string>
get_audio_devices() {
  std::map<std::string, std::string> audio_devices;
  std::vector<AudioObjectID> currentDeviceList =
      AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();
  for (AudioObjectID &deviceId : currentDeviceList) {
    std::string name;
    {
      CFStringRef name_ref = AudioDevice::copyObjectName(deviceId);
      char const *name_cstr = CFStringGetCStringPtr(
          name_ref, CFStringBuiltInEncodings::kCFStringEncodingUTF8);
      if (name_cstr == nullptr) {
        CFIndex size = (sizeof(char) * CFStringGetLength(name_ref)) + 1;
        char *buf = (char *)malloc(size);
        bool success =
            CFStringGetCString(name_ref, buf, size,
                               CFStringBuiltInEncodings::kCFStringEncodingUTF8);
        assert(success);
        name_cstr = buf;
      }
      name = name_cstr;
    }
    std::string uid;
    {
      CFStringRef device_uid_ref = AudioDevice::copyDeviceUID(deviceId);
      char const *device_uid = CFStringGetCStringPtr(
          device_uid_ref, CFStringBuiltInEncodings::kCFStringEncodingUTF8);
      if (!device_uid) {
        CFIndex size = (sizeof(char) * CFStringGetLength(device_uid_ref)) + 1;
        char *buf = (char *)malloc(size);
        bool success =
            CFStringGetCString(device_uid_ref, buf, size,
                               CFStringBuiltInEncodings::kCFStringEncodingUTF8);
        assert(success);
        device_uid = buf;
      }
      uid = device_uid;
    }
    audio_devices.insert({name, uid});
  }
  return audio_devices;
}

static std::string find_audio_device_uid_from_name(std::string device_name) {

  std::map<std::string, std::string> audio_devices = get_audio_devices();
  std::map<std::string, std::string>::iterator device_found =
      audio_devices.find(device_name);
  assert(device_found != audio_devices.end());
  std::string device_uid = device_found->second;
  return device_uid;
}

static AudioDeviceID audioDeviceIDForBoxUID(std::string device_uid) {
  return AudioDevice::audioDeviceIDForBoxUID(CFStringCreateWithCString(
      nullptr, device_uid.c_str(),
      CFStringBuiltInEncodings::kCFStringEncodingUTF8));
}

static void set_audio_device_as_active(std::string device_uid) {
  AudioDeviceID proxy_audio_box = audioDeviceIDForBoxUID(kBox_UID);
  std::string formatted_device_uid = ({
    static constexpr char output_literal[] = "outputDevice=";
    std::size_t output_literal_len = sizeof(output_literal) - 1;
    std::size_t sizeof_the_thing =
        sizeof(char) * (output_literal_len + device_uid.size()) + 1;
    char *buf = (char *)malloc(sizeof_the_thing);
    for (std::size_t i = 0; i < output_literal_len; ++i) {
      buf[i] = output_literal[i];
    }
    for (std::size_t i = 0; i < device_uid.size(); ++i) {
      buf[i + output_literal_len] = device_uid[i];
    }
    buf;
  });
  printf("%s\n", formatted_device_uid.c_str());

  AudioDevice::setObjectName(
      proxy_audio_box, CFStringCreateWithCString(
                           nullptr, formatted_device_uid.c_str(),
                           CFStringBuiltInEncodings::kCFStringEncodingUTF8));
}

static void select_audio_device(std::string device_name) {
  std::string device_uid = find_audio_device_uid_from_name(device_name);
  set_audio_device_as_active(device_uid);
  return;
}

int main(int argc, const char *argv[]) {
  list_audio_devices();
  select_audio_device("MacBook Pro Speakers");

  return 0;
}
