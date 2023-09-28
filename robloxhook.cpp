#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h> // Added for GetModuleInformation
#include <string>
#include <iostream>

HHOOK hKeyboardHook;

// Structure to hold the address and size information
struct MemoryInfo {
    LPVOID lpBaseAddress;
    SIZE_T nSize;
};

// Function to get the process ID by name
DWORD GetProcessIdByName(const wchar_t* processName) {
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD processId = 0; // Initialize with an invalid value

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                processId = pe32.th32ProcessID; // Store the process ID
                break; // Exit the loop when found
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processId;
}

// Function to read data structures from the target process's memory
bool ReadDataStructuresFromProcess(DWORD processId, const MemoryInfo& memInfo) {
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, processId);

    if (hProcess == NULL) {
        std::cerr << "Failed to open target process. Error code: " << GetLastError() << std::endl;
        return false;
    }

    // Allocate a buffer to read data from the process
    BYTE* buffer = new BYTE[memInfo.nSize];

    // Read the data from the process
    SIZE_T bytesRead;
    if (ReadProcessMemory(hProcess, memInfo.lpBaseAddress, buffer, memInfo.nSize, &bytesRead)) {
        // Successfully read the data from the memory region

        // Interpret the buffer as a pointer (DWORD_PTR is used for 64-bit compatibility)
        DWORD_PTR instructionPointer = *reinterpret_cast<DWORD_PTR*>(buffer);

        // Print the instruction pointer as a hexadecimal value with "0x" prefix
        printf("Instruction Pointer: 0x%IX\n", instructionPointer);

        CloseHandle(hProcess);
        return true;
    }


    std::cerr << "Failed to read process memory. Error code: " << GetLastError() << std::endl;

    delete[] buffer;
    CloseHandle(hProcess);
    return false;
}

// Function to get memory information from the target process
bool GetMemoryInfoFromProcess(DWORD processId, MemoryInfo* memInfo) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);

    if (hProcess == NULL) {
        std::cerr << "Failed to open target process. Error code: " << GetLastError() << std::endl;
        return false;
    }

    HMODULE hModules[1];
    DWORD cbNeeded;

    // Get the first module (RobloxPlayerBeta.dll) in the process
    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
        MODULEINFO moduleInfo;
        if (GetModuleInformation(hProcess, hModules[0], &moduleInfo, sizeof(MODULEINFO))) {
            // Set the base address and size
            memInfo->lpBaseAddress = moduleInfo.lpBaseOfDll;
            memInfo->nSize = moduleInfo.SizeOfImage;

            CloseHandle(hProcess);
            return true;
        }
    }

    CloseHandle(hProcess);
    return false;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            KBDLLHOOKSTRUCT* pKeyInfo = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

            // Handle the key press here
            // Example: If you want to intercept the 'A' key press
            if (pKeyInfo->vkCode == 'A') {
                // Retrieve memory information from the target process (Roblox in this case)
                const wchar_t* robloxProcessName = L"RobloxPlayerBeta.exe";
                DWORD robloxProcessId = GetProcessIdByName(robloxProcessName);

                if (robloxProcessId != 0) {
                    MemoryInfo memInfo;
                    if (GetMemoryInfoFromProcess(robloxProcessId, &memInfo)) {
                        if (ReadDataStructuresFromProcess(robloxProcessId, memInfo)) {
                            std::cout << "Structs read" << std::endl;
                        }
                        else {
                            std::cerr << "Unable to read structs" << std::endl;
                        }
                    }
                    else {
                        std::cerr << "Failed to get memory info from process" << std::endl;
                    }
                }
                else {
                    std::cerr << "Roblox process not found!" << std::endl;
                }
            }
        }
    }

    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

int main() {
    // Set the hook
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);

    if (hKeyboardHook == NULL) {
        MessageBoxW(NULL, L"Failed to set keyboard hook!", L"Error", MB_ICONERROR);
        return 1;
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unhook the hook
    UnhookWindowsHookEx(hKeyboardHook);

    return 0;
}
