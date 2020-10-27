import QtQuick 2.6
import QtQuick.Window 2.2
import QtWayland.Compositor 1.3

WaylandCompositor {
    // The output defines the screen.
    WaylandOutput {
        sizeFollowsWindow: true
        window: Window {
            width: 1024
            height: 768
            visible: true
            Repeater {
                model: shellSurfaces
                // ShellSurfaceItem handles displaying a shell surface.
                // It has implementations for things like interactive
                // resize/move, and forwarding of mouse and keyboard
                // events to the client process.
                ShellSurfaceItem {
                    autoCreatePopupItems: true
                    shellSurface: modelData
                    onSurfaceDestroyed: shellSurfaces.remove(index)
                }
            }
        }
    }
    // Extensions are additions to the core Wayland
    // protocol. We choose to support three different
    // shells (window management protocols). When the
    // client creates a new shell surface (i.e. a window)
    // we append it to our list of shellSurfaces.
    WlShell {
        onWlShellSurfaceCreated:
            shellSurfaces.append({shellSurface: shellSurface});
    }
    XdgShellV6 {
        onToplevelCreated:
            shellSurfaces.append({shellSurface: xdgSurface});
    }
    XdgShell {
        onToplevelCreated:
            shellSurfaces.append({shellSurface: xdgSurface});
    }
    ListModel { id: shellSurfaces }
}
