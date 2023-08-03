#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

class BInputKey {
public:
    int Key;
    bool bCtrlPressed;
    bool bShiftPressed;
    bool bAltPressed;
    bool bDoubleClicked;
    bool bTpsModeKey;
};

enum class EngineKeyStateType {
    EKS_PRESSED = 0,
    EKS_RELEASED = 1,
    EKS_REPEAT = 2,
    EKS_DOUBLECLICK = 3,
    EKS_AXIS = 4
};

std::string EngineKeyStateString(EngineKeyStateType type);

class EInputKeyEvent {
public:
    char padding[0x18];
    char _vKey;
    char padd_2[0x2];
    EngineKeyStateType KeyState;
    bool bCtrlPressed;
    bool bShiftPressed;
    bool bAltPressed;
};

#ifndef BInstance_H
#define BInstance_H

class BInstance {
public:
	char pad[0xA0];
	uintptr_t* GameWorld;
	char padd[0x10];
	uintptr_t* PresentationWorld;
};
#endif // !BInstance_H

typedef void(__cdecl* _AddInstantNotification)(
    uintptr_t* thisptr,
    const wchar_t* text,
    const wchar_t* particleRef,
    const wchar_t* sound,
    char track,
    bool stopPreviousSound,
    bool headline2,
    bool boss_headline,
    bool chat,
    char category,
    const wchar_t* sound2);

typedef void(__cdecl* _ExecuteConsoleCommandNoHistory)(const wchar_t* szCmd);

#ifndef BSMessaging_H
#define BSMessaging_H

class BSMessaging {
private:
    uintptr_t* BNSClientInstance;
    _AddInstantNotification oAddInstantNotification;
    void SendGameMessage_s(
    const wchar_t* text,
    const wchar_t* particleRef,
    const wchar_t* sound,
    char track,
    bool stopPreviousSound,
    bool headline2,
    bool boss_headline,
    bool chat,
    char category,
    const wchar_t* sound2);
public:
    BSMessaging(uintptr_t*, _AddInstantNotification);
	void DisplaySystemChatMessage(const wchar_t*, bool playSound);
    void DisplayScrollingTextHeadline(const wchar_t*, bool playSound);
    void DisplaySSPTestMessage();
};
#endif // !BSMessaging_H

uintptr_t GetAddress(uintptr_t AddressOfCall, int index, int length);
