#include "llviewerprecompiledheaders.h"
#include "fs_python_bridge.h"
#include "llpluginmessage.h"
#include "llviewerwindow.h"
#include "lldir.h"

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

void FSPythonBridge::sendFrame(LLPointer<LLImageRaw> raw)
{
    if (!mPlugin)
        return;
    LLPluginMessage msg("bridge", "frame");
    std::string name = mPlugin->addSharedMemory(raw->getDataSize());
    mPlugin->sendSharedMemory(name, raw->getData(), raw->getDataSize());
    msg.setValue("name", name);
    msg.setValueS32("width", raw->getWidth());
    msg.setValueS32("height", raw->getHeight());
    mPlugin->sendMessage(msg);
}

void FSPythonBridge::sendPacket(const std::string &data, bool outgoing)
{
    if (!mPlugin)
        return;
    LLPluginMessage msg("bridge", outgoing ? "packet_out" : "packet_in");
    msg.setValueBinaryData("data", data.data(), data.size());
    mPlugin->sendMessage(msg);
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
