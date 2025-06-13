#include "llviewerprecompiledheaders.h"
/**
 * @file fs_python_bridge.cpp
 * @brief Viewer side of the Python bridge plugin
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
#include "fs_python_bridge.h"
#include "llpluginmessage.h"
#include "llviewerwindow.h"
#include "lldir.h"
#include "llgl.h"
#include "llimage.h"
#include "llbase64.h"

FSPythonBridge::FSPythonBridge() : mPlugin(nullptr) {}

FSPythonBridge &FSPythonBridge::instance()
{
    static FSPythonBridge inst;
    return inst;
}

void FSPythonBridge::Plugin::receivePluginMessage(const LLPluginMessage &message)
{
    if (message.getClass() == "bridge")
    {
        mOwner->handleBridgeMessage(message);
    }
    else
    {
        LLPluginClassMedia::receivePluginMessage(message);
    }
}

std::string FSPythonBridge::Plugin::allocSharedMemory(size_t size)
{
    return addSharedMemory(size);
}

void FSPythonBridge::Plugin::freeSharedMemory(const std::string &name)
{
    removeSharedMemory(name);
}

void FSPythonBridge::Plugin::writeSharedMemory(const std::string &name, const void *data, size_t size)
{
    void *addr = getSharedMemoryAddress(name);
    if(addr)
        memcpy(addr, data, size);
}

void FSPythonBridge::init()
{
    if (mPlugin)
        return;

    std::string launcher = gDirUtilp->getLLPluginLauncher();
    std::string plugdir = gDirUtilp->getLLPluginDir();
    std::string plugin = gDirUtilp->getLLPluginFilename("media_plugin_pybridge");
    mPlugin = new Plugin(this);
    mPlugin->init(launcher, plugdir, plugin, false);
}

void FSPythonBridge::idle()
{
    if (mPlugin)
        mPlugin->idle();
}

void FSPythonBridge::captureFrame()
{
    if (!gViewerWindow)
        return;

    S32 width = gViewerWindow->getWindowWidthRaw();
    S32 height = gViewerWindow->getWindowHeightRaw();
    LLPointer<LLImageRaw> raw = new LLImageRaw(width, height, 3);
    LLImageDataSharedLock lock(raw);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, raw->getData());
    sendFrame(raw);
}

void FSPythonBridge::sendFrame(LLPointer<LLImageRaw> raw)
{
    if (!mPlugin)
        return;
    LLPluginMessage msg("bridge", "frame");
    std::string name = mPlugin->allocSharedMemory(raw->getDataSize());
    mPlugin->writeSharedMemory(name, raw->getData(), raw->getDataSize());
    msg.setValue("name", name);
    msg.setValueS32("width", raw->getWidth());
    msg.setValueS32("height", raw->getHeight());
    mPlugin->sendMessagePublic(msg);
    mPlugin->freeSharedMemory(name);
}

void FSPythonBridge::sendPacket(const std::string &data, bool outgoing)
{
    if (!mPlugin)
        return;
    LLPluginMessage msg("bridge", outgoing ? "packet_out" : "packet_in");
    std::string encoded = LLBase64::encode((const U8*)data.data(), data.size());
    msg.setValue("data", encoded);
    mPlugin->sendMessagePublic(msg);
}

void FSPythonBridge::handleBridgeMessage(const LLPluginMessage &msg)
{
    std::string name = msg.getName();
    if (name == "mouse")
    {
        std::string state = msg.getValue("state");
        int x = msg.getValueS32("x");
        int y = msg.getValueS32("y");
        int button = msg.getValueS32("button");
        SDL_Event event{};
        if (state == "move")
        {
            event.type = SDL_MOUSEMOTION;
            event.motion.x = x;
            event.motion.y = y;
        }
        else if (state == "down")
        {
            event.type = SDL_MOUSEBUTTONDOWN;
            event.button.x = x;
            event.button.y = y;
            event.button.button = button;
        }
        else if (state == "up")
        {
            event.type = SDL_MOUSEBUTTONUP;
            event.button.x = x;
            event.button.y = y;
            event.button.button = button;
        }
        SDL_PushEvent(&event);
    }
    else if (name == "keyboard")
    {
        std::string state = msg.getValue("state");
        int key = msg.getValueS32("key");
        int mod = msg.getValueS32("mod");
        SDL_Event event{};
        event.key.keysym.sym = (SDL_Keycode)key;
        event.key.keysym.mod = mod;
        if (state == "down")
            event.type = SDL_KEYDOWN;
        else
            event.type = SDL_KEYUP;
        SDL_PushEvent(&event);
    }
}
