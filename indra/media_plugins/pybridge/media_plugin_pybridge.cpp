/**
 * @file media_plugin_pybridge.cpp
 * @brief Plugin to forward viewer data to an external Python process
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llgl.h"
#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"
#include "media_plugin_base.h"
#include "llbase64.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsdjson.h"

#include "llwebrtc.h"
#include <curl/curl.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <sstream>

class MediaPluginPyBridge : public MediaPluginBase,
                            public llwebrtc::LLWebRTCSignalingObserver,
                            public llwebrtc::LLWebRTCDataObserver,
                            public llwebrtc::LLWebRTCLogCallback
{
public:
    MediaPluginPyBridge(LLPluginInstance::sendMessageFunction host_send_func,
                        void *host_user_data);
    ~MediaPluginPyBridge() override;

    void receiveMessage(const char *message_string) override;
    static void idle(void *userdata);

    // llwebrtc callbacks
    void OnIceGatheringState(EIceGatheringState state) override {}
    void OnIceCandidate(const llwebrtc::LLWebRTCIceCandidate& candidate) override {}
    void OnOfferAvailable(const std::string& sdp) override;
    void OnRenegotiationNeeded() override {}
    void OnPeerConnectionClosed() override {}
    void OnAudioEstablished(llwebrtc::LLWebRTCAudioInterface* ai) override {}
    void OnDataChannelReady(llwebrtc::LLWebRTCDataInterface *data_interface) override;
    void OnDataReceived(const std::string& data, bool binary) override;
    void LogMessage(llwebrtc::LLWebRTCLogCallback::LogLevel level, const std::string& message) override {}

private:
    bool init();
    void enqueue_json(const std::string &msg);
    void handle_line(const std::string &line);

    llwebrtc::LLWebRTCPeerConnectionInterface *mPeer;
    llwebrtc::LLWebRTCDataInterface *mData;
    std::string mInputBuf;
    std::string mOutputBuf;
    pid_t mChildPid;
};

MediaPluginPyBridge::MediaPluginPyBridge(LLPluginInstance::sendMessageFunction host_send_func,
                                         void *host_user_data)
    : MediaPluginBase(host_send_func, host_user_data), mPeer(nullptr), mData(nullptr), mChildPid(-1)
{
}

MediaPluginPyBridge::~MediaPluginPyBridge()
{
    if (mPeer)
    {
        mPeer->shutdownConnection();
        llwebrtc::freePeerConnection(mPeer);
    }
    llwebrtc::terminate();
    if (mChildPid > 0)
    {
        ::kill(mChildPid, SIGTERM);
    }
}

bool MediaPluginPyBridge::init()
{
    llwebrtc::init(this);
    mPeer = llwebrtc::newPeerConnection();
    mPeer->setSignalingObserver(this);
    llwebrtc::LLWebRTCPeerConnectionInterface::InitOptions opts;
    mPeer->initializeConnection(opts);

    LLPluginMessage msg(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
    msg.setValue("name", "Python Bridge Plugin");
    sendMessage(msg);

    const char *script = ::getenv("PYBRIDGE_SCRIPT");
    if (script)
    {
        const char *venv = ::getenv("PYBRIDGE_VENV");
        std::string python = venv ? std::string(venv) + "/bin/python" : "python3";
        pid_t pid = ::fork();
        if (pid == 0)
        {
            ::execlp(python.c_str(), python.c_str(), script, (char *)NULL);
            ::_exit(1);
        }
        else if (pid > 0)
        {
            mChildPid = pid;
        }
    }
    return true;
}

void MediaPluginPyBridge::enqueue_json(const std::string &msg)
{
    mOutputBuf += msg + "\n";
}

void MediaPluginPyBridge::handle_line(const std::string &line)
{
    std::istringstream iss(line);
    std::string type;
    if (!(iss >> type))
        return;
    if (type == "mouse")
    {
        std::string state;
        int x = 0, y = 0, button = 0;
        if (iss >> state >> x >> y >> button)
        {
            LLPluginMessage msg("bridge", "mouse");
            msg.setValue("state", state);
            msg.setValueS32("x", x);
            msg.setValueS32("y", y);
            msg.setValueS32("button", button);
            sendMessage(msg);
        }
    }
    else if (type == "key")
    {
        std::string state;
        int key = 0, mod = 0;
        if (iss >> state >> key >> mod)
        {
            LLPluginMessage msg("bridge", "keyboard");
            msg.setValue("state", state);
            msg.setValueS32("key", key);
            msg.setValueS32("mod", mod);
            sendMessage(msg);
        }
    }
}

void MediaPluginPyBridge::idle(void *userdata)
{
    auto *self = (MediaPluginPyBridge *)userdata;
    if (self->mData)
    {
        while (!self->mOutputBuf.empty())
        {
            size_t newline = self->mOutputBuf.find('\n');
            if (newline == std::string::npos)
                break;
            std::string line = self->mOutputBuf.substr(0, newline);
            self->mOutputBuf.erase(0, newline + 1);
            self->mData->sendData(line, false);
        }
    }
}

void MediaPluginPyBridge::OnOfferAvailable(const std::string &sdp)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return;
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8080/offer");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    LLSD req;
    req["sdp"] = sdp;
    std::string post = boost::json::serialize(LlsdToJson(req));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post.size());
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr,size_t size,size_t nmemb,void *userdata)->size_t{
        std::string *res = (std::string*)userdata;
        res->append(ptr, size*nmemb);
        return size*nmemb;
    });
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    try
    {
        auto json_val = boost::json::parse(response);
        LLSD data = LlsdFromJson(json_val);
        if (data.has("sdp"))
        {
            mPeer->AnswerAvailable(data["sdp"].asString());
        }
    }
    catch (const std::exception &)
    {
        // ignore parse errors
    }
}

void MediaPluginPyBridge::OnDataChannelReady(llwebrtc::LLWebRTCDataInterface *data_interface)
{
    mData = data_interface;
    if (mData)
        mData->setDataObserver(this);
}

void MediaPluginPyBridge::OnDataReceived(const std::string &data, bool binary)
{
    if (binary)
        return;
    handle_line(data);
}

void MediaPluginPyBridge::receiveMessage(const char *message_string)
{
    LLPluginMessage message_in;
    if (message_in.parse(message_string) < 0)
        return;

    std::string message_class = message_in.getClass();
    std::string message_name = message_in.getName();

    if (message_class == LLPLUGIN_MESSAGE_CLASS_BASE)
    {
        if (message_name == "init")
        {
            LLPluginMessage message("base", "init_response");
            LLSD versions = LLSD::emptyMap();
            versions[LLPLUGIN_MESSAGE_CLASS_BASE] = LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
            versions[LLPLUGIN_MESSAGE_CLASS_MEDIA] = LLPLUGIN_MESSAGE_CLASS_MEDIA_VERSION;
            message.setValueLLSD("versions", versions);
            message.setValue("plugin_version", "pybridge 0.1");
            sendMessage(message);
            init();
        }
        else if (message_name == "idle")
        {
            idle(this);
        }
        else if (message_name == "cleanup")
        {
            LLPluginMessage message("base", "goodbye");
            sendMessage(message);
            mDeleteMe = true;
        }
        else if (message_name == "shm_added")
        {
            SharedSegmentInfo info;
            info.mAddress = message_in.getValuePointer("address");
            info.mSize = (size_t)message_in.getValueS32("size");
            std::string name = message_in.getValue("name");
            mSharedSegments.insert(SharedSegmentMap::value_type(name, info));
        }
        else if (message_name == "shm_remove")
        {
            std::string name = message_in.getValue("name");
            mSharedSegments.erase(name);
            LLPluginMessage message("base", "shm_remove_response");
            message.setValue("name", name);
            sendMessage(message);
        }
    }
    else if (message_class == "bridge")
    {
        if (message_name == "frame")
        {
            std::string name = message_in.getValue("name");
            S32 width = message_in.getValueS32("width");
            S32 height = message_in.getValueS32("height");
            auto iter = mSharedSegments.find(name);
            if (iter != mSharedSegments.end())
            {
                const U8 *data = (const U8 *)iter->second.mAddress;
                std::string encoded = LLBase64::encode(data, width * height * 3);
                std::string json = "{\"type\":\"frame\",\"width\":" + std::to_string(width) +
                                   ",\"height\":" + std::to_string(height) + ",\"data\":\"" + encoded + "\"}";
                enqueue_json(json);
            }
        }
        else if (message_name == "packet_in" || message_name == "packet_out")
        {
            std::string data = message_in.getValue("data");
            std::string dir = (message_name == "packet_in") ? "in" : "out";
            std::string json = "{\"type\":\"packet\",\"dir\":\"" + dir + "\",\"data\":\"" +
                               LLBase64::encode((const U8 *)data.data(), data.size()) + "\"}";
            enqueue_json(json);
        }
        else if (message_name == "mouse" || message_name == "keyboard")
        {
            std::string json = "{\"type\":\"" + message_name + "\",\"data\":\"" +
                               message_in.getValue("data") + "\"}";
            enqueue_json(json);
        }
    }
}

int init_media_plugin(LLPluginInstance::sendMessageFunction host_send_func,
                                 void *host_user_data,
                                 LLPluginInstance::sendMessageFunction *plugin_send_func,
                                 void **plugin_user_data)
{
    MediaPluginPyBridge *self = new MediaPluginPyBridge(host_send_func, host_user_data);
    *plugin_send_func = MediaPluginPyBridge::staticReceiveMessage;
    *plugin_user_data = (void *)self;
    return 0;
}

