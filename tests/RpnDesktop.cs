using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public static class RpnDesktop
{
    [StructLayout(LayoutKind.Sequential)]
    private struct Rect
    {
        public int Left, Top, Right, Bottom;

        public Rectangle Bounds
        {
            get { return Rectangle.FromLTRB(Left, Top, Right, Bottom); }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Point
    {
        public int X, Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Message
    {
        public IntPtr Window;
        public uint Id;
        public UIntPtr WParam;
        public IntPtr LParam;
        public uint Time;
        public Point Cursor;
        public uint Private;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct GuiThreadInfo
    {
        public uint Size, Flags;
        public IntPtr Active, Focus, Capture, MenuOwner, MoveSize, Caret;
        public Rect CaretBounds;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput
    {
        public int X, Y;
        public uint Data, Flags, Time;
        public UIntPtr ExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KeyboardInput
    {
        public ushort Key, Scan;
        public uint Flags, Time;
        public UIntPtr ExtraInfo;
    }

    [StructLayout(LayoutKind.Explicit)]
    private struct InputData
    {
        [FieldOffset(0)] public MouseInput Mouse;
        [FieldOffset(0)] public KeyboardInput Keyboard;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Input
    {
        public uint Type;
        public InputData Data;
    }

    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out int pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] private static extern IntPtr GetDlgItem(IntPtr dialog, int id);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool ClientToScreen(IntPtr window, ref Point point);
    [DllImport("user32.dll")] private static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll", SetLastError = true)] private static extern uint SendInput(uint count, Input[] events, int size);
    [DllImport("user32.dll")] private static extern IntPtr GetMenu(IntPtr window);
    [DllImport("user32.dll")] private static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] private static extern uint GetMenuItemID(IntPtr menu, int position);
    [DllImport("user32.dll")] private static extern IntPtr GetSubMenu(IntPtr menu, int position);
    [DllImport("user32.dll")] private static extern uint GetMenuState(IntPtr menu, uint position, uint flags);
    [DllImport("user32.dll", SetLastError = true)] private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] private static extern uint GetDpiForSystem();
    [DllImport("user32.dll")] private static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] private static extern IntPtr GetDC(IntPtr window);
    [DllImport("user32.dll")] private static extern int ReleaseDC(IntPtr window, IntPtr dc);
    [DllImport("gdi32.dll")] private static extern bool GetDCOrgEx(IntPtr dc, out Point origin);
    [DllImport("user32.dll")] private static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] private static extern IntPtr GetKeyboardLayout(uint thread);
    [DllImport("dwmapi.dll")] private static extern int DwmFlush();
    [DllImport("dwmapi.dll")] private static extern int DwmSetWindowAttribute(IntPtr window, int attribute, ref int value, int size);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool RegisterHotKey(IntPtr window, int id, uint modifiers, uint key);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool UnregisterHotKey(IntPtr window, int id);
    [DllImport("user32.dll")] private static extern bool PeekMessage(out Message message, IntPtr window, uint first, uint last, uint flags);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool PostThreadMessage(uint thread, uint message, UIntPtr wParam, IntPtr lParam);
    [DllImport("kernel32.dll")] private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", SetLastError = true)] private static extern bool GetGUIThreadInfo(uint thread, ref GuiThreadInfo info);

    private const int EscapeHotKey = 0x5250;

    public static int[] CheckEnvironment()
    {
        if (!Environment.UserInteractive || Process.GetCurrentProcess().SessionId == 0 ||
            GetForegroundWindow() == IntPtr.Zero)
            throw new InvalidOperationException("An active, unlocked interactive desktop is required.");
        int[] profile = null;
        WithPhysicalCoordinates(delegate {
            profile = new int[] { GetSystemMetrics(0), GetSystemMetrics(1), (int)GetDpiForSystem() };
        });
        return profile;
    }

    public static void RegisterEscape()
    {
        if (!RegisterHotKey(IntPtr.Zero, EscapeHotKey, 0x4000, 0x1B))
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot reserve Esc; no desktop test input was sent.");
    }

    public static void CheckEscape()
    {
        Message message;
        if (PeekMessage(out message, new IntPtr(-1), 0x0312, 0x0312, 0x0001) &&
            message.WParam.ToUInt64() == EscapeHotKey)
            throw new OperationCanceledException("Desktop test stopped by Esc.");
    }

    public static void SelfTestCancellation()
    {
        CheckEscape();
        if (!PostThreadMessage(GetCurrentThreadId(), 0x0312, new UIntPtr(EscapeHotKey), IntPtr.Zero))
            throw new Win32Exception();
        try {
            CheckEscape();
        }
        catch (OperationCanceledException) {
            CheckEscape();
            return;
        }
        throw new InvalidOperationException("The desktop test did not cancel on a queued Esc notification.");
    }

    public static void ReleaseEscape()
    {
        if (!UnregisterHotKey(IntPtr.Zero, EscapeHotKey))
            throw new Win32Exception();
    }

    public static bool MenuActive(IntPtr dialog)
    {
        CheckEscape();
        int owner;
        uint thread = GetWindowThreadProcessId(dialog, out owner);
        if (thread == 0)
            return false;
        GuiThreadInfo info = new GuiThreadInfo();
        info.Size = (uint)Marshal.SizeOf(typeof(GuiThreadInfo));
        if (!GetGUIThreadInfo(thread, ref info))
            throw new Win32Exception();
        return info.MenuOwner == dialog && (info.Flags & 0x0004) != 0;
    }

    public static IntPtr ActiveDialog(int pid, string className)
    {
        CheckEscape();
        IntPtr foreground = GetForegroundWindow();
        if (foreground == IntPtr.Zero || !IsWindowVisible(foreground))
            return IntPtr.Zero;
        int owner;
        uint thread = GetWindowThreadProcessId(foreground, out owner);
        if (thread == 0 || owner != pid)
            return IntPtr.Zero;
        StringBuilder name = new StringBuilder(32);
        if (GetClassName(foreground, name, name.Capacity) == 0 || name.ToString() != className)
            return IntPtr.Zero;
        if ((GetKeyboardLayout(thread).ToInt64() & 0xFFFF) != 0x0409)
            throw new InvalidOperationException("Select the US English keyboard layout in the test profile.");
        // The collapsed client area meets the shell's rounded bottom edge.
        int squareCorners = 1; // DWMWCP_DONOTROUND
        int result = DwmSetWindowAttribute(foreground, 33, ref squareCorners, sizeof(int));
        if (result != unchecked((int)0x80070057)) // Unsupported before Windows 11.
            Marshal.ThrowExceptionForHR(result);
        return foreground;
    }

    public static void Key(IntPtr window, ushort key, ushort modifier)
    {
        RequireForeground(window);
        if (modifier == 0)
            Send(new Input[] { KeyEvent(key, false), KeyEvent(key, true) });
        else
            Send(new Input[] { KeyEvent(modifier, false), KeyEvent(key, false),
                               KeyEvent(key, true), KeyEvent(modifier, true) });
    }

    public static void Click(IntPtr dialog, int controlId)
    {
        RequireForeground(dialog);
        WithPhysicalCoordinates(delegate {
            IntPtr control = GetDlgItem(dialog, controlId);
            Rectangle bounds = WindowBounds(control);
            Point center = new Point { X = bounds.Left + bounds.Width / 2,
                                       Y = bounds.Top + bounds.Height / 2 };
            if (WindowFromPoint(center) != control)
                throw new InvalidOperationException("Keypad control is clipped or covered: " + controlId);
            if (!SetCursorPos(center.X, center.Y))
                throw new Win32Exception();
            RequireForeground(dialog);
            Send(new Input[] { MouseEvent(0x0002), MouseEvent(0x0004) });
        });
    }

    public static Rectangle[] Geometry(IntPtr dialog, int displayId)
    {
        Rectangle[] result = null;
        WithPhysicalCoordinates(delegate {
            result = new Rectangle[] {
                WindowBounds(dialog),
                ClientBounds(dialog),
                WindowBounds(GetDlgItem(dialog, displayId))
            };
        });
        return result;
    }

    public static void MoveForPlacementCheck(IntPtr dialog)
    {
        RequireForeground(dialog);
        WithPhysicalCoordinates(delegate {
            if (!SetWindowPos(dialog, IntPtr.Zero, 120, 144, 0, 0, 0x0015))
                throw new Win32Exception();
        });
    }

    public static bool KeypadChecked(IntPtr dialog)
    {
        uint state = MenuState(GetMenu(dialog), 420);
        if (state == UInt32.MaxValue)
            throw new InvalidOperationException("View > Keypad was not found.");
        return (state & 0x0008) != 0;
    }

    public static string Capture(IntPtr dialog, int displayId, string path, bool trace)
    {
        RequireForeground(dialog);
        string geometry = null;
        WithPhysicalCoordinates(delegate {
            IntPtr display = GetDlgItem(dialog, displayId);
            Rectangle bounds = WindowBounds(display);
            Point upper = new Point { X = bounds.Left + 1, Y = bounds.Top + 1 };
            Point lower = new Point { X = bounds.Right - 2, Y = bounds.Bottom - 2 };
            if (WindowFromPoint(upper) != display || WindowFromPoint(lower) != display)
                throw new InvalidOperationException("The display is clipped or covered.");
            Marshal.ThrowExceptionForHR(DwmFlush());
            if (trace) {
                IntPtr dc = GetDC(display);
                if (dc == IntPtr.Zero)
                    throw new InvalidOperationException("Cannot obtain the display device context.");
                try {
                    Point origin;
                    if (!GetDCOrgEx(dc, out origin))
                        throw new InvalidOperationException("Cannot obtain the display device-context origin.");
                    geometry = string.Format("window={0}; dialog-client={1}; display-client={2}; " +
                        "screen-crop={3}; dc-origin=({4},{5}); app-dpi={6}; screen-dpi={7}",
                        WindowBounds(dialog), ClientBounds(dialog), ClientBounds(display), bounds,
                        origin.X, origin.Y, GetDpiForWindow(dialog), GetDpiForSystem());
                }
                finally {
                    ReleaseDC(display, dc);
                }
            }
            using (Bitmap bitmap = new Bitmap(bounds.Width, bounds.Height, PixelFormat.Format24bppRgb)) {
                using (Graphics graphics = Graphics.FromImage(bitmap))
                    graphics.CopyFromScreen(bounds.Location, System.Drawing.Point.Empty, bounds.Size);
                bitmap.Save(path, ImageFormat.Png);
            }
        });
        return geometry;
    }

    private static void RequireForeground(IntPtr window)
    {
        CheckEscape();
        if (window == IntPtr.Zero || GetForegroundWindow() != window)
            throw new InvalidOperationException("The calculator lost focus; input was not sent.");
    }

    private static void WithPhysicalCoordinates(Action action)
    {
        IntPtr previous = SetThreadDpiAwarenessContext(new IntPtr(-4));
        if (previous == IntPtr.Zero)
            throw new Win32Exception();
        try {
            action();
        }
        finally {
            SetThreadDpiAwarenessContext(previous);
        }
    }

    private static Rectangle WindowBounds(IntPtr window)
    {
        Rect bounds;
        if (window == IntPtr.Zero || !GetWindowRect(window, out bounds))
            throw new InvalidOperationException("Cannot locate the expected calculator window or control.");
        if (bounds.Right <= bounds.Left || bounds.Bottom <= bounds.Top)
            throw new InvalidOperationException("Calculator window or control has empty bounds.");
        return bounds.Bounds;
    }

    private static Rectangle ClientBounds(IntPtr window)
    {
        Rect client;
        Point origin = new Point();
        if (!GetClientRect(window, out client) || !ClientToScreen(window, ref origin))
            throw new Win32Exception();
        Point end = new Point { X = client.Right, Y = client.Bottom };
        if (!ClientToScreen(window, ref end))
            throw new Win32Exception();
        return Rectangle.FromLTRB(origin.X, origin.Y, end.X, end.Y);
    }

    private static uint MenuState(IntPtr menu, uint id)
    {
        int count = GetMenuItemCount(menu);
        for (int position = 0; position < count; ++position) {
            if (GetMenuItemID(menu, position) == id)
                return GetMenuState(menu, (uint)position, 0x0400);
            IntPtr submenu = GetSubMenu(menu, position);
            if (submenu != IntPtr.Zero) {
                uint state = MenuState(submenu, id);
                if (state != UInt32.MaxValue)
                    return state;
            }
        }
        return UInt32.MaxValue;
    }

    private static Input KeyEvent(ushort key, bool up)
    {
        Input input = new Input();
        input.Type = 1;
        input.Data.Keyboard.Key = key;
        input.Data.Keyboard.Flags = (up ? 2U : 0U) |
            (key == 0x25 || key == 0x26 || key == 0x28 || key == 0x2E ? 1U : 0U);
        return input;
    }

    private static Input MouseEvent(uint flags)
    {
        Input input = new Input();
        input.Data.Mouse.Flags = flags;
        return input;
    }

    private static void Send(Input[] events)
    {
        int size = Marshal.SizeOf(typeof(Input));
        if (SendInput((uint)events.Length, events, size) != events.Length)
            throw new InvalidOperationException("Windows did not accept all calculator input events.");
    }
}
