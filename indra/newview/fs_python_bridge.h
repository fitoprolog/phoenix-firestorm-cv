/**
 * @file fs_python_bridge.h
 * @brief Interface for communicating with the pybridge plugin
 */

#ifndef FS_PYTHON_BRIDGE_H
#define FS_PYTHON_BRIDGE_H

#include "llplugincookiestore.h"
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
