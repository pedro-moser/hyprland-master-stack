# hyprland-master-monocle

A Hyprland layout plugin that combines a pinned master window on the left with a card-stack of slave windows on the right.

![Hyprland 0.54+](https://img.shields.io/badge/Hyprland-0.54%2B-blue)

## Layout

```
+-----------+------------------+
|           | [peek: prev]     |  <- 40px, shows title bar
|           +------------------+
|  Master   |                  |
|  (pinned) |  Focused slave   |  <- takes remaining space
|           |                  |
|           +------------------+
|           | [peek: next]     |  <- 40px, shows bottom edge
+-----------+------------------+
```

- **Master** stays pinned on the left, taking a configurable portion of the screen (`mfact`)
- **Focused slave** gets the remaining vertical space on the right
- **Adjacent slaves** peek as thin strips (top shows title bar, bottom shows footer) - no distortion
- **Non-adjacent slaves** are hidden
- Navigate slaves with `movefocus d/u` (e.g., `Super+J/K`) - no wrap-around at boundaries
- Focus-follows-mouse works between master and slaves, but not between peek strips (click to switch)
- Closing a slave focuses the adjacent slave, not master

## Requirements

- Hyprland 0.54+ (uses the `ITiledAlgorithm` API)
- C++23 compiler

## Building

```bash
make
```

## Installation

### Manual

```bash
hyprctl plugin load /path/to/master-monocle.so
```

### hyprpm

Add to your Hyprland config:

```ini
plugin {
    master-monocle {
        mfact = 0.5         # master width ratio (0.05 - 0.95)
        peek_height = 40     # peek strip height in pixels
    }
}
```

## Usage

Activate the layout:

```ini
general {
    layout = master-monocle
}
```

Or bind a key to switch to it:

```ini
bind = SUPER SHIFT, N, exec, hyprctl keyword general:layout master-monocle
```

### Layout messages

| Command | Description |
|---|---|
| `layoutmsg cyclenext` | Focus next slave |
| `layoutmsg cycleprev` | Focus previous slave |
| `layoutmsg swapwithmaster` | Swap focused slave with master |
| `layoutmsg focusmaster` | Toggle focus between master and slave |
| `layoutmsg mfact exact 0.6` | Set master width ratio |
| `layoutmsg mfact 0.05` | Adjust master width ratio |

### Window movement

- `movewindow l` from a slave: becomes master (old master goes to stack)
- `movewindow r` from master: goes to stack (focused slave becomes master)

## Notes

- The plugin hooks `movefocus` to block wrap-around at stack boundaries and restores the original dispatcher on unload
- Focus-follows-mouse is selectively disabled for peek strips using `eFocusReason` filtering
- Peek strips use `STargetBox` with separate `logicalBox`/`visualBox` to render windows at full size but clip to the peek area, avoiding content distortion
- Due to `dlopen` caching, reloading the plugin requires loading from a different file path (e.g., copy to `/tmp/`)

## License

MIT
