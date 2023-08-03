#include <pe/module.h>
#include "detours.h";
#include "LeanPlugin.hpp"
#include "BSFunctions.h";
#include <iostream>
#include "searchers.h";
#include "xorstr.hpp";
#include <Windows.h>
#include <exception>
#include <filesystem>
#include "SimpleIni.h"
#include <unordered_map>
#include <functional>

//need these non public detour structs for the mid-function hook

struct DETOUR_ALIGN
{
	BYTE    obTarget : 3;
	BYTE    obTrampoline : 5;
};

struct DETOUR_INFO
{
	// An X64 instuction can be 15 bytes long.
	// In practice 11 seems to be the limit.
	BYTE            rbCode[30];     // target code + jmp to pbRemain.
	BYTE            cbCode;         // size of moved target code.
	BYTE            cbCodeBreak;    // padding to make debugging easier.
	BYTE            rbRestore[30];  // original target code.
	BYTE            cbRestore;      // size of original target code.
	BYTE            cbRestoreBreak; // padding to make debugging easier.
	DETOUR_ALIGN    rAlign[8];      // instruction alignment array.
	PBYTE           pbRemain;       // first instruction after moved code. [free list]
	PBYTE           pbDetour;       // first instruction of detour function.
	BYTE            rbCodeIn[8];    // jmp [pbDetour]
};

extern "C"
{
	void hkAltitudeHack();
	LPVOID lpRemain;
	uintptr_t xyzAddr = 0;
	uintptr_t isCameraCompareAddr = 0;
	bool altitudeEnabled = false;
	bool gravityEnabled = false;
}

static BSMessaging* Messaging;
gsl::span<uint8_t> data;
pe::module* module;
uintptr_t handle;
uintptr_t GEngineAddress;

bool cheaterMode = false;
bool speedhack = false;
bool speedfallToggle = false;


float* speed;
float ToggleSpeedvalue = (float)1;
int ToggleSpeedHotkey = -1;
bool ToggleSpeedAlt = false;
bool ToggleSpeedShift = false;
bool ToggleSpeedCtrl = false;
bool ToggleSpeedMessage = false;
bool speedSetup = false;

float ToggleAltitudeValue = (float)1;
float FlyingSpeedValue = (float)3;
int ToggleAltitudeHotkey = -1;
bool ToggleAltitudeAlt = false;
bool ToggleAltitudeShift = false;
bool ToggleAltitudeCtrl = false;
bool ToggleAltitudeMessage = false;
bool altitudeSetup = false;

int ToggleGravityHotkey = -1;
bool ToggleGravityAlt = false;
bool ToggleGravityShift = false;
bool ToggleGravityCtrl = false;
bool ToggleGravityMessage = false;
bool gravitySetup = false;

int ToggleFallHotkey = -1;
bool ToggleFallAlt = false;
bool ToggleFallShift = false;
bool ToggleFallCtrl = false;
bool ToggleFallMessage = false;
bool fallSetup = false;

extern "C" {
	float* altitude = &ToggleAltitudeValue;
}

/// <summary>
/// Get the common section values from the ini
/// </summary>
/// <param name="ini"></param>
/// <param name="sectionName"></param>
/// <param name="hotkey"></param>
/// <param name="value"></param>
/// <param name="alt"></param>
/// <param name="shift"></param>
/// <param name="ctrl"></param>
/// <param name="message"></param>
void GetSectionValues(const CSimpleIniA& ini, const char* sectionName, std::string& hotkey, std::string& value, std::string& alt, std::string& shift, std::string& ctrl, std::string& message) {
	try {
		hotkey = ini.GetValue(sectionName, "hotkey", "");
		value = ini.GetValue(sectionName, "value", "");
		alt = ini.GetValue(sectionName, "alt", "");
		shift = ini.GetValue(sectionName, "shift", "");
		ctrl = ini.GetValue(sectionName, "ctrl", "");
		message = ini.GetValue(sectionName, "message", "");
	}
	catch (...) {
#ifdef _DEBUG
		std::cout << "Error reading ini values" << std::endl;
#endif // _DEBUG
	}
}

/// <summary>
/// Read lean.ini from plugins directory and setup all config values
/// </summary>
/// <returns></returns>
void WINAPI InitConfigValues() {
	WCHAR fullpath[MAX_PATH];
	CSimpleIniA hotkeys;
	hotkeys.SetUnicode();
	GetModuleFileNameW(NULL, fullpath, MAX_PATH);
	std::filesystem::path base(fullpath);
	std::wstring inipath(base.parent_path());
	inipath += xorstr_(L"/plugins/lean.ini");
	const wchar_t* fullinipath = inipath.c_str();
#ifdef _DEBUG
	std::wcout << fullinipath << std::endl;
#endif // _DEBUG

	hotkeys.LoadFile(fullinipath);

	std::string speedHotkey, speedValue, speedAlt, speedShift, speedCtrl, speedMessage;
	std::string flyHotkey, flyValue, flyAlt, flyShift, flyCtrl, flySpeed, flyMessage;
	std::string gravityHotkey, gravityValue, gravityAlt, gravityShift, gravityCtrl, gravityMessage;
	std::string fallHotkey, fallValue, fallAlt, fallShift, fallCtrl, fallMessage;

	GetSectionValues(hotkeys, "speed", speedHotkey, speedValue, speedAlt, speedShift, speedCtrl, speedMessage);
	if (!speedHotkey.empty() && !speedValue.empty()) {
		try {
			ToggleSpeedvalue = std::stof(speedValue);
			ToggleSpeedHotkey = std::stoi(speedHotkey, nullptr, 0);
			if (!speedAlt.empty()) {
				ToggleSpeedAlt = std::stoi(speedAlt);
			}
			if (!speedShift.empty()) {
				ToggleSpeedShift = std::stoi(speedShift);
			}
			if (!speedCtrl.empty()) {
				ToggleSpeedCtrl = std::stoi(speedCtrl);
			}
			if (!speedMessage.empty()) {
				ToggleSpeedMessage = std::stoi(speedMessage);
			}
			speedSetup = true;
		}
		catch (...) {
			speedSetup = false;
		}
	}
	GetSectionValues(hotkeys, "fly", flyHotkey, flyValue, flyAlt, flyShift, flyCtrl, flyMessage);
	if (!flyHotkey.empty() && !flyValue.empty() && hotkeys.GetValue("fly", "speed")) {
		try {
			FlyingSpeedValue = std::stof(hotkeys.GetValue("fly", "speed"));
			ToggleAltitudeValue = std::stof(flyValue);
			ToggleAltitudeHotkey = std::stoi(flyHotkey, nullptr, 0);
			if (!flyAlt.empty()) {
				ToggleAltitudeAlt = std::stoi(flyAlt);
			}
			if (!flyShift.empty()) {
				ToggleAltitudeShift = std::stoi(flyShift);
			}
			if (!flyCtrl.empty()) {
				ToggleAltitudeCtrl = std::stoi(flyCtrl);
			}
			if (!flyMessage.empty()) {
				ToggleAltitudeMessage = std::stoi(flyMessage);
			}
			altitudeSetup = true;
		}
		catch (...) {
			altitudeSetup = false;
		}
	}
	GetSectionValues(hotkeys, "gravity", gravityHotkey, gravityValue, gravityAlt, gravityShift, gravityCtrl, gravityMessage);
	if (!gravityHotkey.empty()) {
		try {
			ToggleGravityHotkey = std::stoi(gravityHotkey, nullptr, 0);
			if (!gravityAlt.empty()) {
				ToggleGravityAlt = std::stoi(gravityAlt);
			}
			if (!gravityShift.empty()) {
				ToggleGravityShift = std::stoi(gravityShift);
			}
			if (!gravityCtrl.empty()) {
				ToggleGravityCtrl = std::stoi(gravityCtrl);
			}
			if (!gravityMessage.empty()) {
				ToggleGravityMessage = std::stoi(gravityMessage);
			}
			gravitySetup = true;
		}
		catch (...) {
			gravitySetup = false;
		}
	}
	GetSectionValues(hotkeys, "fall", fallHotkey, fallValue, fallAlt, fallShift, fallCtrl, fallMessage);
	if (!fallHotkey.empty()) {
		try {
			ToggleFallHotkey = std::stoi(fallHotkey, nullptr, 0);
			if (!fallAlt.empty()) {
				ToggleFallAlt = std::stoi(fallAlt);
			}
			if (!fallShift.empty()) {
				ToggleFallShift = std::stoi(fallShift);
			}
			if (!fallCtrl.empty()) {
				ToggleFallCtrl = std::stoi(fallCtrl);
			}
			if (!fallMessage.empty()) {
				ToggleFallMessage = std::stoi(fallMessage);
			}
			fallSetup = true;
		}
		catch (...) {
			fallSetup = false;
		}
	}
}

/// <summary>
/// Write the current player position to current_position.ini in plugins directory. Used for tp setup.
/// </summary>
/// <param name="x"></param>
/// <param name="y"></param>
/// <param name="z"></param>
/// <returns></returns>
void WINAPI WritePositionToFile(float* x, float* y, float* z) {
	WCHAR fullpath[MAX_PATH];
	CSimpleIniA posIni;
	posIni.SetUnicode();
	GetModuleFileNameW(NULL, fullpath, MAX_PATH);
	std::filesystem::path base(fullpath);
	std::wstring inipath(base.parent_path());
	inipath += xorstr_(L"/plugins/current_position.ini");
	const wchar_t* fullinipath = inipath.c_str();
	posIni.LoadFile(fullinipath);
	posIni.Delete("pos", nullptr);
	char buf[18];
	snprintf(buf, sizeof buf, "%f", *x);
	posIni.SetValue("pos", "x", buf);
	snprintf(buf, sizeof buf, "%f", *y);
	posIni.SetValue("pos", "y", buf);
	snprintf(buf, sizeof buf, "%f", *z);
	posIni.SetValue("pos", "z", buf);
	posIni.SaveFile(fullinipath);
}

DWORD64 GetPointerAddress(std::vector<DWORD64> offsets)
{
	DWORD64 addr = offsets[0];

	if (addr == 0)
	{
		return 0;
	}

	for (int i = 1; i < offsets.size(); ++i)
	{
		addr = *(DWORD64*)addr;
		addr += offsets[i];
		if (IsBadReadPtr((DWORD64*)addr, 4))
		{
			return 0;
		}
	}
#ifdef _DEBUG
	std::cout << "Exit GetPointerAddress" << std::endl;
#endif // _DEBUG
	return addr;
}

void WINAPI ScannerSetup() {
#ifdef _DEBUG
	std::cout << "ScannerSetup" << std::endl;
#endif // _DEBUG
	module = pe::get_module();
	handle = module->handle();
	const auto sections2 = module->segments();
	const auto& s2 = std::find_if(sections2.begin(), sections2.end(), [](const IMAGE_SECTION_HEADER& x) {
		return x.Characteristics & IMAGE_SCN_CNT_CODE;
		});
	data = s2->as_bytes();
}

static uintptr_t* BNSClientInstance = NULL;
static _AddInstantNotification oAddInstantNotification;
static _ExecuteConsoleCommandNoHistory oExecuteConsoleCommandNoHistory;

/// <summary>
/// Setup BnS messaging to send chat or notification messages in game.
/// From Tonic
/// </summary>
/// <returns></returns>
void WINAPI InitMessaging() {
#ifdef _DEBUG
	std::cout << "InitMessaging" << std::endl;
#endif // _DEBUG

#ifdef _DEBUG
	std::cout << "Searching sBShowHud" << std::endl;
#endif // _DEBUG

	auto sBShowHud = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("0F 29 70 C8 ?? 8B F2 48 8B ?? 48 83 79 08 00")));
	if (sBShowHud != data.end()) {
		BNSClientInstance = (uintptr_t*)GetAddress((uintptr_t)&sBShowHud[0] + 0x15, 3, 7);
	}

#ifdef _DEBUG
	std::cout << "Searching AddInstantNotification" << std::endl;
#endif // _DEBUG
	// Used for sending notifications about certain actions
	bool diffPattern = false;
	auto sAddNotif = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("45 33 DB 41 8D 42 ?? 3C 02 BB 05 00 00 00 41 0F 47 DB")));
	if (sAddNotif == data.end()) {
		// Old compiler stuff (NAEU CLIENT)
		diffPattern = true;
		sAddNotif = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("33 FF 80 BC 24 80 00 00 00 01 75 05")));
	}

	if (sAddNotif != data.end()) {
		oAddInstantNotification = module->rva_to<std::remove_pointer_t<decltype(oAddInstantNotification)>>((uintptr_t)&sAddNotif[0] - (diffPattern ? 0x13 : 0x68) - handle);
	}

#ifdef _DEBUG
	std::cout << "Searching Done" << std::endl;
#endif // _DEBUG
	Messaging = new BSMessaging(BNSClientInstance, oAddInstantNotification);
#ifdef _DEBUG
	std::cout << "Messaging object created" << std::endl;
#endif // _DEBUG


#ifdef _DEBUG
	printf("Address of BNSInstance is %p\n", (void*)BNSClientInstance);
	std::cout << std::endl;
#endif // _DEBUG
}


bool(__fastcall* oBInputKey)(BInputKey* thisptr, EInputKeyEvent* InputKeyEvent);
bool __fastcall hkBInputKey(BInputKey* thisptr, EInputKeyEvent* InputKeyEvent);


/// <summary>
/// Copy a text to the clipboard. For debugging purposes.
/// </summary>
/// <param name="s"></param>
void toClipboard(const std::string& s) {
	OpenClipboard(0);
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
	if (!hg) {
		CloseClipboard();
		return;
	}
	memcpy(GlobalLock(hg), s.c_str(), s.size() + 1);
	GlobalUnlock(hg);
	SetClipboardData(CF_TEXT, hg);
	CloseClipboard();
	GlobalFree(hg);
}

template <typename Callable>
void handleKeyEvent(EInputKeyEvent* InputKeyEvent, int vKeyTarget, const Callable& onPress) {
	static std::unordered_map<int, bool> toggleKeys;

	if (InputKeyEvent->_vKey == vKeyTarget) {
		bool& toggleKey = toggleKeys[vKeyTarget];
		if (!toggleKey && InputKeyEvent->KeyState == EngineKeyStateType::EKS_PRESSED) {
			toggleKey = true;
			onPress();
		}
		else if (toggleKey && InputKeyEvent->KeyState == EngineKeyStateType::EKS_RELEASED) {
			toggleKey = false;
		}
	}
}

template <typename Callable>
void handleKeyEventWithModifiers(
	EInputKeyEvent* InputKeyEvent,
	int vKeyTarget,
	bool alt,
	bool shift,
	bool ctrl,
	const Callable& onPress
) {
	static std::unordered_map<int, bool> toggleKeys;

	if (InputKeyEvent->_vKey == vKeyTarget) {
		bool& toggleKey = toggleKeys[vKeyTarget];
		if (!toggleKey && InputKeyEvent->KeyState == EngineKeyStateType::EKS_PRESSED) {
			// Check for Alt, Shift, and Ctrl modifiers
			if ((alt == InputKeyEvent->bAltPressed) &&
				(shift == InputKeyEvent->bShiftPressed) &&
				(ctrl == InputKeyEvent->bCtrlPressed)) {
				toggleKey = true;
				onPress();
			}
		}
		else if (toggleKey && InputKeyEvent->KeyState == EngineKeyStateType::EKS_RELEASED) {
			toggleKey = false;
		}
	}
}


void UpdateGenginePointers() {
	DWORD64* GEngineAdr = (DWORD64*)GEngineAddress;
	speed = (float*)GetPointerAddress({ (DWORD64)GEngineAdr, 0x788, 0x80, 0x38, 0x0, 0x30, 0x2C0, 0x98 });
	auto xPos = (float*)GetPointerAddress({ (DWORD64)GEngineAdr, 0x788, 0x80, 0x38, 0x0, 0x30, 0x2C0, 0x130, 0x1f0 });
	xyzAddr = reinterpret_cast<uintptr_t>(xPos);
	isCameraCompareAddr = xyzAddr - 0x1f0;
#ifdef _DEBUG
	std::cout << "xyzAddr: " << std::hex << (void*)xyzAddr << std::endl;
	std::cout << "isCameraCompareAddr: " << std::hex << (void*)isCameraCompareAddr << std::endl;
#endif // _DEBUG
}

/// <summary>
/// Toggle the key listener for fly, gravity, fall
/// </summary>
void toggleCheaterMode() {
	cheaterMode = !cheaterMode;
	auto message = cheaterMode ? L"<image imagesetpath=\"00027918.Portrait_Alert\"/>Cheater Mode Enabled" : L"<font name=\"00008130.UI.Label_DarkYellow_out_20\">Cheater Mode Disabled</font>";
	Messaging->DisplayScrollingTextHeadline(message, true);
	if (cheaterMode) {
		DWORD64* GEngineAdr = (DWORD64*)GEngineAddress;

		std::string message1 = std::format("{}", (void*)GEngineAdr);
		message1.replace(0, 2, "0000");

		std::wstring str2(message1.length(), L' '); // Make room for characters

		// Copy string to wstring.
		std::copy(message1.begin(), message1.end(), str2.begin());
#ifdef _DEBUG
		toClipboard(message1);
		std::wstring message0 = std::format(L"Copied GEngine address to clipboard: {}", str2);
		Messaging->DisplaySystemChatMessage(message0.c_str(), false);
		printf("Address of GEngine is %p\n", (void*)GEngineAdr);
		std::cout << std::endl;
#endif // _DEBUG
	}
	else {
		speedhack = false;
		speedfallToggle = false;
		altitudeEnabled = false;
		gravityEnabled = false;
		UpdateGenginePointers();
		*speed = (float)1;
	}
}

void toggleSpeedhack() {
	if (speedSetup) {
		UpdateGenginePointers();
		speedhack = !speedhack;
		if (speedhack) {
			*speed = (float)ToggleSpeedvalue;
		}
		else {
			*speed = (float)1;
		}
#ifdef _DEBUG
		std::cout << *speed << std::endl;
#endif // _DEBUG
		if (ToggleSpeedMessage) {
			auto message = speedhack ? L"Speedhack on!<br/>Speed: " + std::to_wstring(*speed) : L"Speedhack off!<br/>Speed: " + std::to_wstring(*speed);
			Messaging->DisplaySystemChatMessage(message.c_str(), true);
		}
	}
	else {
		auto message = L"<image imagesetpath=\"00027918.Tooltip_Alert\" enablescale=\"true\" scalerate=\"1.6\" />Speedhack not configured!";
		Messaging->DisplaySystemChatMessage(message, false);
	}
}

void toggleAltitude() {
	if (altitudeSetup) {
		UpdateGenginePointers();
		speedfallToggle = false;
		altitudeEnabled = !altitudeEnabled;
		if (altitudeEnabled) {
			*speed = (float)FlyingSpeedValue;
		}
		else {
			*speed = (float)1;
		}
		if (ToggleAltitudeMessage) {
			auto message = altitudeEnabled ? L"Altitude hack on!" : L"Altitude hack off!";
			Messaging->DisplaySystemChatMessage(message, true);
		}
	}
}

void toggleGravity() {
	if (gravitySetup) {
		UpdateGenginePointers();
		speedfallToggle = false;
		gravityEnabled = !gravityEnabled;
		if (ToggleGravityMessage) {
			auto message = gravityEnabled ? L"Gravity hack on!" : L"Gravity hack off!";
			Messaging->DisplaySystemChatMessage(message, true);
		}
	}
}

void toggleFall() {
	if (fallSetup) {
		UpdateGenginePointers();
		altitudeEnabled = false;
		gravityEnabled = false;
		speedfallToggle = !speedfallToggle;
		if (speedfallToggle) {
			*speed = (float)9;
		}
		else {
			*speed = (float)1;
		}
		if (ToggleFallMessage) {
			auto message = speedfallToggle ? L"Quick fall on!" : L"Quick fall off!";
			Messaging->DisplaySystemChatMessage(message, true);
		}
	}
}

/// <summary>
/// Hook game key inputs
/// </summary>
/// <param name="thisptr"></param>
/// <param name="InputKeyEvent"></param>
/// <returns></returns>
bool __fastcall hkBInputKey(BInputKey* thisptr, EInputKeyEvent* InputKeyEvent) {
	if (InputKeyEvent->bAltPressed) {
		handleKeyEvent(InputKeyEvent, 0x43, toggleCheaterMode); //C Key, toggle "cheater mode"
		handleKeyEvent(InputKeyEvent, 0x50, []() { //P Key, reload config
			InitConfigValues();
			auto message = L"<image imagesetpath=\"00027918.Tooltip_Alert\" enablescale=\"true\" scalerate=\"1.6\" />Speed, Fly, Gravity Config reloaded!";
			Messaging->DisplaySystemChatMessage(message, false);
			});
		if (cheaterMode) {
			handleKeyEvent(InputKeyEvent, 0x4E, []() { //N Key, print player position
				if (speedSetup) {
					PrintPlayerPosition();
				}
				});
		}
	}

	if (cheaterMode) {
		if (speedSetup) {
			handleKeyEventWithModifiers(InputKeyEvent, ToggleSpeedHotkey, ToggleSpeedAlt, ToggleSpeedShift, ToggleSpeedCtrl, toggleSpeedhack);
		}
		if (altitudeSetup) {
			handleKeyEventWithModifiers(InputKeyEvent, ToggleAltitudeHotkey, ToggleAltitudeAlt, ToggleAltitudeShift, ToggleAltitudeCtrl, toggleAltitude);
		}
		if (gravitySetup) {
			handleKeyEventWithModifiers(InputKeyEvent, ToggleGravityHotkey, ToggleGravityAlt, ToggleGravityShift, ToggleGravityCtrl, toggleGravity);
		}
		if (fallSetup) {
			handleKeyEventWithModifiers(InputKeyEvent, ToggleFallHotkey, ToggleFallAlt, ToggleFallShift, ToggleFallCtrl, toggleFall);
		}
	}
	return oBInputKey(thisptr, InputKeyEvent);
}

/// <summary>
/// Print player position to file and ingame chat. For teleport setup.
/// </summary>
void PrintPlayerPosition() {
	DWORD64* GEngineAdr = (DWORD64*)GEngineAddress;
	auto xPos = (float*)GetPointerAddress({ (DWORD64)GEngineAdr, 0x788, 0x80, 0x38, 0x0, 0x30, 0x2C0, 0x130, 0x1f0 });
	auto yPos = (float*)GetPointerAddress({ (DWORD64)GEngineAdr, 0x788, 0x80, 0x38, 0x0, 0x30, 0x2C0, 0x130, 0x1f0 + 4 });
	auto zPos = (float*)GetPointerAddress({ (DWORD64)GEngineAdr, 0x788, 0x80, 0x38, 0x0, 0x30, 0x2C0, 0x130, 0x1f0 + 8 });

	auto message = L"<br/>Position:<br/>X: " + std::to_wstring(*xPos) + L"<br/>Y: " + std::to_wstring(*yPos) + L"<br/>Z: " + std::to_wstring(*zPos);
	Messaging->DisplaySystemChatMessage(message.c_str(), false);
	WritePositionToFile(xPos, yPos, zPos);
}

void WINAPI InitDetours() {
#ifdef _DEBUG
	std::cout << "InitDetours" << std::endl;
#endif // _DEBUG
	DetourTransactionBegin();
	DetourUpdateThread(NtCurrentThread());

	auto sBinput = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("0F B6 47 18 48 8D 4C 24 30 89 03")));
	if (sBinput != data.end()) {
		uintptr_t aBinput = (uintptr_t)&sBinput[0] - 0x38;
#ifdef _DEBUG
		printf("Address of aBinput is %p\n", (void*)aBinput);
		std::cout << std::endl;
#endif // _DEBUG
		oBInputKey = module->rva_to<std::remove_pointer_t<decltype(oBInputKey)>>(aBinput - handle);
		DetourAttach(&(PVOID&)oBInputKey, &hkBInputKey);
	}

	auto GEngineSearch = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("48 8B 05 ?? ?? ?? ?? 48 8B 88 ?? ?? 00 00 48 85 C9 74 ?? 48 8B 01 FF 90 ?? ?? 00 00 48 85 C0")));
	if (GEngineSearch != data.end()) {
		GEngineAddress = GetAddress((uintptr_t)&GEngineSearch[0], 3, 7);
#ifdef _DEBUG
		printf("Address of GEngine is %p\n", (void*)GEngineAddress);
		std::cout << std::endl;
#endif // _DEBUG
	}

	PDETOUR_TRAMPOLINE lpTrampolineData = {}; //to store information about hooking
	auto sAltitudeHack = std::search(data.begin(), data.end(), pattern_searcher(xorstr_("0F 11 BF F0 01 00 00 48 8B CF 0F 11 B7 00 02 00 00 E8")));
	if (sAltitudeHack != data.end()) {
		uintptr_t aAltitudeHack = (uintptr_t)((uintptr_t)&sAltitudeHack[0]);
#ifdef _DEBUG
		printf("Address of aAltitudeHack is %p\n", (void*)aAltitudeHack);
		std::cout << std::endl;
#endif // _DEBUG
		DetourAttachEx(&(PVOID&)aAltitudeHack, (PVOID)&hkAltitudeHack, &lpTrampolineData, nullptr, nullptr); //Use DetourAtttachEx to retrieve information about the hook
	}
	DetourTransactionCommit();

	const auto lpDetourInfo = (DETOUR_INFO*)lpTrampolineData;
	lpRemain = lpDetourInfo->pbRemain; //Retrieve the address to jump back to the original function
}

void ThreadedMain() {
	InitConfigValues();
	ScannerSetup();
	InitMessaging();
	InitDetours();
#ifdef _DEBUG
	std::cout << "Inits done" << std::endl;
#endif // _DEBUG
}

void WINAPI LeanPlugin_Main()
{

#ifdef _DEBUG
	//Open console and print (testing)
	AllocConsole();
	(void)freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	std::cout << "LeanPlugin Initialized" << std::endl;
#endif
	std::thread t(ThreadedMain);
	t.detach();
}