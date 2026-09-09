# Send keyboard input to the visible SOMA window owned by a specific process.
# This is intentionally separate from XR controller input: SOMA's main menu is
# alive before the mod has a confirmed player camera and therefore before the
# automated OpenXR session starts.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][int]$GameProcessId,
    [Parameter(Mandatory = $true)][string[]]$Tap,
    [int]$HoldMs = 80,
    [double]$TimeoutSec = 5,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

if (-not ('SomaVrWindowInput' -as [type])) {
    Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class SomaVrWindowInput {
    public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT {
        public ushort wVk;
        public ushort wScan;
        public uint dwFlags;
        public uint time;
        public UIntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Explicit)]
    public struct INPUTUNION {
        [FieldOffset(0)] public KEYBDINPUT ki;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT {
        public uint type;
        public INPUTUNION u;
    }

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lp);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll", SetLastError=true)] public static extern uint SendInput(uint n, INPUT[] p, int cb);

    public static IntPtr FindVisibleWindow(uint targetPid, out string title) {
        IntPtr found = IntPtr.Zero;
        string foundTitle = "";
        EnumWindows((h, _) => {
            uint pid;
            GetWindowThreadProcessId(h, out pid);
            if (pid == targetPid && IsWindowVisible(h)) {
                var text = new StringBuilder(512);
                GetWindowText(h, text, text.Capacity);
                if (found == IntPtr.Zero || text.Length > foundTitle.Length) {
                    found = h;
                    foundTitle = text.ToString();
                }
            }
            return true;
        }, IntPtr.Zero);
        title = foundTitle;
        return found;
    }

    public static bool Focus(IntPtr hwnd) {
        if (IsIconic(hwnd)) ShowWindow(hwnd, 9); // SW_RESTORE
        ShowWindow(hwnd, 5);                    // SW_SHOW
        IntPtr foreground = GetForegroundWindow();
        uint foregroundPid;
        uint foregroundThread = foreground == IntPtr.Zero
            ? 0 : GetWindowThreadProcessId(foreground, out foregroundPid);
        uint currentThread = GetCurrentThreadId();
        bool attached = foregroundThread != 0 && foregroundThread != currentThread
            && AttachThreadInput(currentThread, foregroundThread, true);
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        if (attached) AttachThreadInput(currentThread, foregroundThread, false);
        return GetForegroundWindow() == hwnd;
    }

    public static bool Tap(ushort key, int holdMs) {
        var down = new INPUT { type = 1 };
        down.u.ki.wVk = key;
        var up = down;
        up.u.ki.dwFlags = 0x0002; // KEYEVENTF_KEYUP
        if (SendInput(1, new [] { down }, Marshal.SizeOf(typeof(INPUT))) != 1) return false;
        System.Threading.Thread.Sleep(Math.Max(1, holdMs));
        return SendInput(1, new [] { up }, Marshal.SizeOf(typeof(INPUT))) == 1;
    }
}
'@
}

$keys = @{
    'enter' = 0x0D; 'return' = 0x0D; 'escape' = 0x1B; 'esc' = 0x1B
    'space' = 0x20; 'left' = 0x25; 'up' = 0x26; 'right' = 0x27; 'down' = 0x28
    'tab' = 0x09; 'backspace' = 0x08
}

$process = Get-Process -Id $GameProcessId -ErrorAction Stop
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
$window = [IntPtr]::Zero
$title = ''
while ([DateTime]::UtcNow -lt $deadline -and $window -eq [IntPtr]::Zero) {
    $window = [SomaVrWindowInput]::FindVisibleWindow([uint32]$GameProcessId, [ref]$title)
    if ($window -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 100 }
}
if ($window -eq [IntPtr]::Zero) {
    throw "no visible top-level window appeared for pid $GameProcessId within $TimeoutSec seconds"
}

$focused = $false
while ([DateTime]::UtcNow -lt $deadline -and -not $focused) {
    $focused = [SomaVrWindowInput]::Focus($window)
    if (-not $focused) { Start-Sleep -Milliseconds 100 }
}
if (-not $focused) {
    throw ("could not verify foreground ownership for pid {0}, hwnd 0x{1:X}; " -f $GameProcessId, $window.ToInt64()) +
          "refusing to send keys to an unknown window"
}

foreach ($name in $Tap) {
    $key = $keys[$name.ToLowerInvariant()]
    if ($null -eq $key) { throw "unsupported key '$name'" }
    if (-not [SomaVrWindowInput]::Tap([uint16]$key, $HoldMs)) {
        throw "SendInput failed for '$name' (Win32 error $([Runtime.InteropServices.Marshal]::GetLastWin32Error()))"
    }
}

if (-not $Quiet) {
    Write-Host ("sent {0} to pid {1}, hwnd 0x{2:X}, title '{3}' (foreground verified)" -f `
        ($Tap -join ','), $GameProcessId, $window.ToInt64(), $title)
}

[pscustomobject]@{
    Pid = $GameProcessId
    Hwnd = ('0x{0:X}' -f $window.ToInt64())
    Title = $title
    ForegroundVerified = $focused
    Keys = $Tap
}
