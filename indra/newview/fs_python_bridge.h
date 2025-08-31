/**
 * @file fs_python_bridge.h
 * @brief Interface for communicating with the pybridge plugin
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

#ifndef FS_PYTHON_BRIDGE_H
#define FS_PYTHON_BRIDGE_H

#include "llviewerwindow.h"
#include "llpluginclassmedia.h"
#include <SDL2/SDL.h>

class LLPluginClassMedia;

class FSPythonBridge : public LLPluginClassMediaOwner
{
public:
    static FSPythonBridge &instance();

    void init();
    void idle();
    void captureFrame();
    void sendFrame(LLPointer<LLImageRaw> raw);
    void sendPacket(const std::string &data, bool outgoing);
    void handleBridgeMessage(const LLPluginMessage &msg);
    void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event) override {}

private:
    FSPythonBridge();
    class Plugin : public LLPluginClassMedia
    {
    public:
        Plugin(FSPythonBridge *owner) : LLPluginClassMedia(owner), mOwner(owner) {}
        void receivePluginMessage(const LLPluginMessage &message) override;

        std::string allocSharedMemory(size_t size);
        void freeSharedMemory(const std::string &name);
        void writeSharedMemory(const std::string &name, const void *data, size_t size);
        void sendMessagePublic(const LLPluginMessage &message) { sendMessage(message); }
    private:
        FSPythonBridge *mOwner;
    };

    Plugin *mPlugin;
};

#endif // FS_PYTHON_BRIDGE_H
