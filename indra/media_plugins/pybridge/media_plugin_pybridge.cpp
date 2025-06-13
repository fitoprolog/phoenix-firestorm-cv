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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>
#include <cstring>

class MediaPluginPyBridge : public MediaPluginBase
{
public:
    MediaPluginPyBridge(LLPluginInstance::sendMessageFunction host_send_func,
                        void *host_user_data);
    ~MediaPluginPyBridge() override;

    void receiveMessage(const char *message_string) override;
    static void idle(void *userdata);

private:
    bool init();
    void send_json(const std::string &msg);

    int mServerFd;
    int mClientFd;
};

MediaPluginPyBridge::MediaPluginPyBridge(LLPluginInstance::sendMessageFunction host_send_func,
                                         void *host_user_data)
    : MediaPluginBase(host_send_func, host_user_data), mServerFd(-1), mClientFd(-1)
{
}

MediaPluginPyBridge::~MediaPluginPyBridge()
{
    if (mClientFd >= 0)
        ::close(mClientFd);
    if (mServerFd >= 0)
    {
        ::close(mServerFd);
    }
}

bool MediaPluginPyBridge::init()
{
    const char *path = ::getenv("PYBRIDGE_SOCKET");
    if (!path)
        path = "/tmp/firestorm_pybridge.sock";

    mServerFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (mServerFd >= 0)
    {
        struct sockaddr_un addr {};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
        ::unlink(path);
        if (::bind(mServerFd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            ::listen(mServerFd, 1);
        }
    }

    LLPluginMessage msg(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
    msg.setValue("name", "Python Bridge Plugin");
    sendMessage(msg);
    return true;
}

void MediaPluginPyBridge::send_json(const std::string &msg)
{
    if (mClientFd >= 0)
    {
        ::send(mClientFd, msg.c_str(), msg.size(), 0);
        ::send(mClientFd, "\n", 1, 0);
    }
}

void MediaPluginPyBridge::idle(void *userdata)
{
    auto *self = (MediaPluginPyBridge *)userdata;
    if (self->mClientFd < 0 && self->mServerFd >= 0)
    {
        self->mClientFd = ::accept(self->mServerFd, nullptr, nullptr);
    }
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
                send_json(json);
            }
        }
        else if (message_name == "packet_in" || message_name == "packet_out")
        {
            std::string data = message_in.getValue("data");
            std::string dir = (message_name == "packet_in") ? "in" : "out";
            std::string json = "{\"type\":\"packet\",\"dir\":\"" + dir + "\",\"data\":\"" +
                               LLBase64::encode((const U8 *)data.data(), data.size()) + "\"}";
            send_json(json);
        }
        else if (message_name == "mouse" || message_name == "keyboard")
        {
            std::string json = "{\"type\":\"" + message_name + "\",\"data\":\"" +
                               message_in.getValue("data") + "\"}";
            send_json(json);
        }
    }
}

extern "C" int init_media_plugin(LLPluginInstance::sendMessageFunction host_send_func,
                                 void *host_user_data,
                                 LLPluginInstance::sendMessageFunction *plugin_send_func,
                                 void **plugin_user_data)
{
    MediaPluginPyBridge *self = new MediaPluginPyBridge(host_send_func, host_user_data);
    *plugin_send_func = MediaPluginPyBridge::staticReceiveMessage;
    *plugin_user_data = (void *)self;
    return 0;
}

