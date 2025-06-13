/**
 * @file fs_python_bridge.h
 * @brief Interface for communicating with the pybridge plugin
 */

#ifndef FS_PYTHON_BRIDGE_H
#define FS_PYTHON_BRIDGE_H

#include "llplugincookiestore.h"
#include "llviewerwindow.h"

class LLPluginClassMedia;

class FSPythonBridge
{
public:
    static FSPythonBridge &instance();

    void init();
    void sendFrame(LLPointer<LLImageRaw> raw);
    void sendPacket(const std::string &data, bool outgoing);
    void sendMouseEvent(const std::string &desc);
    void sendKeyEvent(const std::string &desc);

private:
    FSPythonBridge();
    LLPluginClassMedia *mPlugin;
};

#endif // FS_PYTHON_BRIDGE_H
