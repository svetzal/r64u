# R64U — Visual Design Specification

A comprehensive visual design system for **r64u**, providing file transfer, remote configuration, and remote control of **Ultimate 64 / 1541 Ultimate-class** devices.

---

## 1) Design Principles

1. **Nostalgia with restraint**: Beige plastic + stripes + VIC-II colors as accents; avoid turning the UI into a toy.
2. **Two speeds**: "Quick actions" are one click away; "dangerous actions" are slowed down (confirmations + undo where possible).
3. **State is explicit**: Connection, selected device, mounted media, and "unsaved config" are always visible.
4. **Everything is reversible**: Prefer safe defaults, preview, and cancellation.
5. **Keyboard-first friendly**: Power users should fly.

---

## 2) Visual Language

### 2.1 Overall Look

**Modern app UI** with subtle Commodore cues:
- Background: warm **beige plastic**
- Panels: off-white + light shadow
- Accent: **C64 rainbow stripes** used sparingly
- Text: crisp, high-contrast, no gimmicky pixel fonts for body text

### 2.2 "Breadbin" Styling Motifs

Use these as *structure*, not decoration:
- Thin horizontal ridges at top/bottom (like the breadbin case)
- Rounded rectangle "badge" elements
- A single "LED" indicator motif (small, top-right) used only for connection/state (not everywhere)

### 2.3 Light & Dark Mode

- **Light mode**: beige plastic background
- **Dark mode**: charcoal "desk mat" with muted rainbow accents

Do not invert the rainbow; keep it authentic and consistent.

---

## 3) Color Specification

### 3.1 Light Theme (Beige Plastic)

```css
/* Background colors */
--color-bg-app:         #E8DFC8;   /* breadbin plastic */
--color-bg-panel:       #F4EEDD;   /* inner panels */
--color-bg-inset:       #E1D6BB;   /* recessed areas */

/* Border colors */
--color-border-subtle:  #CFC5AA;
--color-border-strong:  #B7AE95;

/* Text colors */
--color-text-primary:   #1E1E1E;
--color-text-secondary: #4A4A4A;
--color-text-muted:     #6F6F6F;
--color-text-inverted:  #FFFFFF;

/* C64 Rainbow accents */
--color-accent-red:     #D6403A;
--color-accent-orange:  #E8842A;
--color-accent-yellow:  #E6C229;
--color-accent-green:   #3FA45B;
--color-accent-blue:    #3A7BD5;

/* State colors */
--color-state-connected: #3FA45B;
--color-state-warning:   #E6C229;
--color-state-error:     #C7372F;
--color-state-info:      #3A7BD5;

/* LED indicator */
--color-led-on:         #FF3B30;
--color-led-off:        #7A1A17;
```

### 3.2 Dark Theme (Desk Mat)

```css
/* Background colors */
--color-bg-app:         #1E1F22;
--color-bg-panel:       #2A2C31;
--color-bg-inset:       #18191C;

/* Border colors */
--color-border-subtle:  #3A3C42;
--color-border-strong:  #4A4D55;

/* Text colors */
--color-text-primary:   #F5F5F5;
--color-text-secondary: #C9C9C9;
--color-text-muted:     #9A9A9A;
--color-text-inverted:  #000000;

/* C64 Rainbow accents (slightly brighter for dark bg) */
--color-accent-red:     #E04B44;
--color-accent-orange:  #F19A3E;
--color-accent-yellow:  #F1D04B;
--color-accent-green:   #4EC07A;
--color-accent-blue:    #4A8DFF;
```

### 3.3 VIC-II Palette (For Icons & Accents)

The authentic VIC-II (Pepto) palette is used for **pixel-art icons and decorative accents**, not as a full UI theme. This maintains visual authenticity while keeping the UI modern and readable.

```css
/* VIC-II Pepto Palette (all 16 colors) */
--vic2-0-black:       #000000;
--vic2-1-white:       #FFFFFF;
--vic2-2-red:         #9F4E44;
--vic2-3-cyan:        #6ABFC6;
--vic2-4-purple:      #A057A3;
--vic2-5-green:       #5CAB5E;
--vic2-6-blue:        #50459B;
--vic2-7-yellow:      #C9D487;
--vic2-8-orange:      #A1683C;
--vic2-9-brown:       #6D5412;
--vic2-10-lightred:   #CB7E75;
--vic2-11-darkgrey:   #626262;
--vic2-12-midgrey:    #898989;
--vic2-13-lightgreen: #9AE29B;
--vic2-14-lightblue:  #887ECB;
--vic2-15-lightgrey:  #ADADAD;
```

**Where to use VIC-II colors:**
- Pixel-art icons (file types, actions, status)
- App icon and logo badge
- Rainbow stripe accent in header
- Decorative elements that evoke the C64

**Where NOT to use VIC-II colors:**
- UI chrome backgrounds (use Breadbin beige/dark theme)
- Button backgrounds
- Panel backgrounds
- Body text

### 3.4 Design Constraints

**Rainbow stripes usage:**
- Stripes are allowed only in **header badge** and/or **app icon**
- Not repeated on every panel

**LED motif:**
- LED is permitted as a **single connection-state widget** only
- Never decorative

### 3.5 Icon Palette

When making pixel icons, use a constrained palette:
- 4–5 colors per icon max
- 1–2px outline
- High contrast against both beige and dark backgrounds

---

## 4) Typography

### 4.1 UI Text (Qt Implementation)

Use system fonts for legibility. Qt will select appropriate fonts:

```css
/* Qt font-family fallback chain */
font-family: system-ui, -apple-system, "Segoe UI", "Noto Sans", sans-serif;
```

| Platform | Primary Font |
|----------|--------------|
| macOS | SF Pro / system |
| Windows | Segoe UI |
| Linux | Noto Sans / system |

### 4.2 Font Sizes

```css
--font-size-xs:  11px;
--font-size-sm:  12px;
--font-size-md:  14px;
--font-size-lg:  16px;
--font-size-xl:  20px;
```

### 4.3 Font Weights

```css
--font-weight-regular:  400;
--font-weight-medium:   500;
--font-weight-semibold: 600;
--font-weight-bold:     700;
```

### 4.4 C64 Data Font

Use **C64 Pro Mono** for content that represents device data:

```css
/* C64 data display font */
--font-c64-family: "C64 Pro Mono", monospace;
--font-c64-size-sm:  9px;
--font-c64-size-md: 10px;
--font-c64-size-lg: 11px;
```

**Use C64 Pro Mono for:**
- Directory listings from the device
- Mounted disk names
- Status telemetry
- SID metadata (title, author, copyright)
- PETSCII-style displays

**Use system font for:**
- Navigation, dialogs, toolbars
- Preference screens
- General application chrome

### 4.5 Typography Rules

- Body text never below 12px
- Tables default to 12–13px
- C64 Pro Mono only for device data views
- Display/retro fonts only for headings/logos, never paragraphs

---

## 5) Layout & Spacing Tokens

### 5.1 Spacing Scale

```css
--spacing-xs:   4px;
--spacing-sm:   8px;
--spacing-md:  12px;
--spacing-lg:  16px;
--spacing-xl:  24px;
--spacing-xxl: 32px;
```

### 5.2 Border Radius

```css
--radius-sm:  4px;   /* badges, buttons */
--radius-md:  6px;   /* panels */
--radius-lg: 10px;   /* app icon, logo badge */
```

### 5.3 Shadows (Qt-compatible box-shadow equivalents)

```css
--shadow-panel: 0 2px 4px rgba(0,0,0,0.08);
--shadow-modal: 0 8px 24px rgba(0,0,0,0.18);
```

**Note**: Qt QSS supports limited shadow effects. Use `QGraphicsDropShadowEffect` for panels, or border styling to simulate depth.

---

## 6) Component Specifications

### 6.1 Buttons

```css
/* Button heights */
--button-height-sm: 28px;
--button-height-md: 32px;
--button-height-lg: 40px;

/* Button padding */
--button-padding-x: 12px;
--button-padding-y:  6px;
```

**Button Types:**
- **Primary**: solid background, high contrast
- **Secondary**: outline style
- **Destructive**: red, requires confirmation dialog

**Button States:**
- default
- hover (slightly darker background)
- active (pressed inset effect)
- disabled (40% opacity)

### 6.2 Tables / File Lists

```css
--table-row-height:    28px;
--table-header-height: 30px;
--table-icon-size:     16px;
```

Columns:
- Type icon (16px)
- Name
- Size
- Modified date
- Optional: kind (PRG/SEQ/USR, etc.)

### 6.3 Panels

```css
--panel-padding:       16px;
--panel-header-height: 36px;
```

### 6.4 Toolbar

```css
--toolbar-height:  40px;
--toolbar-icon:    24px;
--toolbar-spacing:  4px;
```

### 6.5 Status Bar

```css
--statusbar-height:    24px;
--statusbar-font-size:  9px;
```

### 6.6 LED Component

Single reusable component for connection/power states:

```css
--led-size:      10px;
--led-ring:       2px;
--led-glow-on:  0 0 6px rgba(255,59,48,0.8);
--led-glow-off: none;
```

**LED Usage Rules:**
- Only for **connection / power / recording-style state**
- Never decorative
- Never inside badges or icons
- Position: top-right of container

---

## 7) Icon System

### 7.1 Icon Style

- **Pixel-art icons** on 16/24/32px grids
- **No anti-aliasing** — hard edges only
- All edges align to pixel grid
- VIC-II-inspired palette (or close equivalent)
- **Format**: PNG (not SVG — preserves pixel art edges)
- **High-DPI**: Include @2x versions for Retina displays

### 7.2 Icon Grid Specification

```css
--icon-grid:           16px;   /* base grid */
--icon-stroke:          1px;   /* outline thickness */
--icon-size-small:     16px;
--icon-size-medium:    24px;
--icon-size-large:     32px;
```

### 7.3 Icon Primitives

Icons are assembled from these primitives for consistency:

| Primitive | Variants |
|-----------|----------|
| `box` | filled, outline |
| `line` | horizontal, vertical, diagonal_45 |
| `corner` | tl, tr, bl, br |
| `notch` | small, medium |
| `circle` | filled, outline |
| `arrow` | up, down, left, right |

### 7.4 Icon Palette Rules

Icons use the **VIC-II Pepto palette** for authentic C64 aesthetics:

- Restrict to `vic2_pepto` colors only
- Maximum 4–5 colors per icon
- 1–2px dark outline for definition
- Must have good contrast against both light (beige) and dark backgrounds

**Recommended VIC-II colors for icons:**
- Outlines: `--vic2-0-black` or `--vic2-11-darkgrey`
- Fills: `--vic2-6-blue`, `--vic2-3-cyan`, `--vic2-5-green`
- Accents: `--vic2-8-orange`, `--vic2-9-brown`, `--vic2-2-red`
- Highlights: `--vic2-1-white`, `--vic2-15-lightgrey`

### 7.5 Icon Layers

```
Layer 1 - Base:    main shape (text.primary or inverted)
Layer 2 - Detail:  secondary interior marks (muted or accent)
Layer 3 - Overlay: status badge, optional (state color only)
```

### 7.5 Status Overlays

Small 6×6 corner glyphs:
- Green dot: connected
- Red dot: error
- Yellow triangle: warning

Overlay always lives **top-right** of icon.

---

## 8) Icon Inventory

### 8.1 Machine Control (7 icons)

| Icon | Purpose | Used In |
|------|---------|---------|
| `connect` | Connect to device | Main toolbar |
| `reset` | Reset C64 | Main toolbar |
| `reboot` | Reboot device | Main toolbar |
| `pause` | Pause emulation | Main toolbar |
| `resume` | Resume emulation | Main toolbar |
| `poweroff` | Power off device | Main toolbar |
| `menu` | Open device menu | Main toolbar |

### 8.2 File Actions (9 icons)

| Icon | Purpose | Used In |
|------|---------|---------|
| `play` | Play SID/music | Explore toolbar, context menu |
| `run` | Run program | Explore toolbar, context menu |
| `mount` | Mount disk image | Explore toolbar, context menu |
| `upload` | Upload to device | Transfer toolbar, context menu |
| `download` | Download from device | Transfer toolbar, context menu |
| `delete` | Delete file | Transfer toolbar, context menu |
| `rename` | Rename file | Transfer toolbar, context menu |
| `newfolder` | Create new folder | Transfer toolbar |
| `refresh` | Refresh file list | Multiple toolbars |

### 8.3 Navigation (3 icons)

| Icon | Purpose | Used In |
|------|---------|---------|
| `up` | Navigate to parent folder | File browsers |
| `eject` | Eject drive | Status bar drive buttons |
| `preferences` | Open preferences | Main toolbar |

### 8.4 Streaming (2 icons)

| Icon | Purpose | Used In |
|------|---------|---------|
| `stream-start` | Start video/audio stream | View toolbar |
| `stream-stop` | Stop stream | View toolbar |

### 8.5 Configuration (3 icons)

| Icon | Purpose | Used In |
|------|---------|---------|
| `save-flash` | Save config to flash | Config toolbar |
| `load-flash` | Load config from flash | Config toolbar |
| `reset-defaults` | Reset to defaults | Config toolbar |

### 8.6 File Types (9 icons)

| Icon | File Extensions | Description |
|------|-----------------|-------------|
| `filetype-prg` | .prg | Executable programs |
| `filetype-crt` | .crt | Cartridge images |
| `filetype-disk` | .d64, .d71, .d81, .g64 | Disk images |
| `filetype-tape` | .tap, .t64 | Tape images |
| `filetype-sid` | .sid | SID music files |
| `filetype-mod` | .mod | Tracker music |
| `filetype-rom` | .bin, .rom | ROM files |
| `filetype-config` | .cfg | Configuration files |
| `filetype-folder` | (directories) | Folder icon |

### 8.7 Status Indicators (3 icons)

| Icon | Purpose | Used In |
|------|---------|---------|
| `status-connected` | Device connected | Status bar |
| `status-disconnected` | Device disconnected | Status bar |
| `status-unsaved` | Unsaved config changes | Config panel |

### 8.8 Icon Count Summary

| Category | Count |
|----------|-------|
| Machine Control | 7 |
| File Actions | 9 |
| Navigation | 3 |
| Streaming | 2 |
| Configuration | 3 |
| File Types | 9 |
| Status Indicators | 3 |
| **Total** | **36** |

---

## 9) File Type Icon Recipes

### 9.1 PRG File

```yaml
icon:
  base: box.outline
  detail:
    - line.horizontal (label bar)
    - glyph: "PRG" (pixel text)
  colors: neutral + accent for label
```

### 9.2 Disk Image (D64/D71/D81/G64)

```yaml
icon:
  base: box.outline
  detail:
    - notch.small (right edge, like real floppy)
    - band: orange or brown (label stripe)
  overlay: optional (mounted indicator)
  colors: neutral base, warm accent for band
```

### 9.3 Tape Image

```yaml
icon:
  base: box.outline
  detail:
    - line.diagonal_45 (represents tape path)
    - circle.outline × 2 (tape reels)
  colors: neutral + brown accent
```

### 9.4 Cartridge (CRT)

```yaml
icon:
  base: box.outline
  detail:
    - tab at bottom edge (connector)
    - label area with glyph
  colors: neutral base, blue or purple accent
```

### 9.5 SID Music File

```yaml
icon:
  base: box.outline
  detail:
    - note symbol (diagonal + dot)
  colors: neutral + cyan or green accent
```

### 9.6 Action Icon Example: Mount

```yaml
icon:
  base: box.outline (disk shape)
  detail:
    - arrow.down (insert action)
  colors: neutral + state.info for arrow
```

---

## 10) App Icon & Logo

### 10.1 Logo Variants

1. **Square App Icon** (primary)
   - Beige plastic tile with `--radius-lg`
   - Subtle ridges top & bottom
   - Black rounded "sticker" badge
   - "R64U" mark inside badge
   - Single red LED in a black ring at extreme top-right

2. **Horizontal Lockup** (for website/header)
   - Left: small square icon
   - Right: wordmark "R64U" or "Remote 64 Ultimate"

3. **Monochrome** (for docs)
   - One-color version for printing

### 10.2 Logo Guidelines

**Do:**
- Keep the rainbow contained to the "64" bar
- Preserve generous margins
- Use consistent corner radius

**Don't:**
- Add gradients to the rainbow itself
- Put the LED on the badge—mount it in the "plastic"
- Scale below 32px (use simplified version)

---

## 11) Qt Implementation Notes

### 11.1 QSS Theme Structure

Prefer CSS variables (Qt 6.5+) or define as QSS properties:

```css
/* Example QSS for light theme */
QMainWindow {
    background-color: #E8DFC8;
}

QWidget#panel {
    background-color: #F4EEDD;
    border: 1px solid #CFC5AA;
    border-radius: 6px;
}

QPushButton {
    background-color: #F4EEDD;
    border: 1px solid #B7AE95;
    border-radius: 4px;
    padding: 6px 12px;
    min-height: 28px;
}

QPushButton:hover {
    background-color: #E8DFC8;
}

QPushButton:pressed {
    background-color: #E1D6BB;
}

QPushButton:disabled {
    opacity: 0.4;
}

QPushButton#destructive {
    background-color: #D6403A;
    color: #FFFFFF;
}
```

### 11.2 Theme Switching

Provide two complete QSS files:
- `theme-light.qss` (beige plastic / Breadbin) — **default**
- `theme-dark.qss` (charcoal desk mat)

Load dynamically at runtime:
```cpp
QFile styleFile(":/themes/theme-light.qss");
styleFile.open(QFile::ReadOnly);
qApp->setStyleSheet(styleFile.readAll());
```

### 11.3 Icon Loading

```cpp
// Load icons with automatic @2x selection
QIcon icon(":/icons/actions/play.png");

// Qt automatically selects @2x on high-DPI displays if available:
// :/icons/actions/play.png    (1x)
// :/icons/actions/play@2x.png (2x)
```

### 11.4 LED Effect (QGraphicsEffect)

```cpp
// For LED glow effect
QGraphicsDropShadowEffect *glow = new QGraphicsDropShadowEffect;
glow->setColor(QColor(255, 59, 48, 200));
glow->setBlurRadius(6);
glow->setOffset(0, 0);
ledWidget->setGraphicsEffect(glow);
```

---

## 12) Accessibility

- **Contrast**: Minimum 4.5:1 for all text
- **Keyboard navigation**: All primary actions accessible via keyboard
- **Focus ring**: Visible focus indicator on all interactive elements
- **Color independence**: Icons + labels together; never rely on color alone

---

## 13) File Structure

```
resources/
├── icons/
│   ├── app.png
│   ├── app@2x.png
│   ├── app.icns
│   ├── app.ico
│   ├── actions/
│   │   ├── connect.png
│   │   ├── connect@2x.png
│   │   ├── reset.png
│   │   ├── reboot.png
│   │   ├── pause.png
│   │   ├── resume.png
│   │   ├── poweroff.png
│   │   ├── menu.png
│   │   ├── play.png
│   │   ├── run.png
│   │   ├── mount.png
│   │   ├── upload.png
│   │   ├── download.png
│   │   ├── delete.png
│   │   ├── rename.png
│   │   ├── newfolder.png
│   │   ├── refresh.png
│   │   ├── up.png
│   │   ├── eject.png
│   │   ├── preferences.png
│   │   ├── stream-start.png
│   │   ├── stream-stop.png
│   │   ├── save-flash.png
│   │   ├── load-flash.png
│   │   └── reset-defaults.png
│   ├── filetypes/
│   │   ├── prg.png
│   │   ├── crt.png
│   │   ├── disk.png
│   │   ├── tape.png
│   │   ├── sid.png
│   │   ├── mod.png
│   │   ├── rom.png
│   │   ├── config.png
│   │   └── folder.png
│   └── status/
│       ├── connected.png
│       ├── disconnected.png
│       └── unsaved.png
├── themes/
│   ├── theme-light.qss
│   └── theme-dark.qss
├── fonts/
│   └── C64_Pro_Mono-STYLE.ttf
└── r64u.qrc
```

---

## 14) Deliverables Checklist

- [ ] Final palette values validated (hex)
- [ ] App icon: square 16/32/64/128/256/512px + .icns/.ico
- [ ] 36 icons in pixel style (1x/2x PNG)
- [ ] QSS theme files (light + dark)
- [ ] C64 Pro Mono font licensed and bundled
- [ ] Screen mockups: Connect, Explore, Transfer, Control, Config, View
- [ ] Interaction prototypes for Mount/Run and Config diff/apply
