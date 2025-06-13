#include "llviewerprecompiledheaders.h"
#include "fs_python_bridge.h"
#include "llpluginclassmedia.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"
#include "llviewerwindow.h"

FSPythonBridge::FSPythonBridge() : mPlugin(nullptr) {}

FSPythonBridge &FSPythonBridge::instance()
{
    static FSPythonBridge inst;
    return inst;
}

void FSPythonBridge::init()
{
    if (mPlugin)
        return;

    mPlugin = new LLPluginClassMedia("pybridge", true);
    mPlugin->init();
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

void FSPythonBridge::sendMouseEvent(const std::string &desc)
{
    if (!mPlugin)
        return;
    LLPluginMessage msg("bridge", "mouse");
    msg.setValue("data", desc);
    mPlugin->sendMessage(msg);
}

void FSPythonBridge::sendKeyEvent(const std::string &desc)
{
    if (!mPlugin)
        return;
    LLPluginMessage msg("bridge", "keyboard");
    msg.setValue("data", desc);
    mPlugin->sendMessage(msg);
}
