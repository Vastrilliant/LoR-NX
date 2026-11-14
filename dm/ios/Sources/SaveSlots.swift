import SwiftUI
import UIKit

enum SaveSlot: String, CaseIterable, Identifiable {
    case burn, myth, legend, plague, nightmare, star, impuritas, end

    var id: String { rawValue }

    var title: String {
        switch self {
        case .burn: return "Burn Save"
        case .myth: return "Urban Myth"
        case .legend: return "Urban Legend"
        case .plague: return "Urban Plague"
        case .nightmare: return "Urban Nightmare"
        case .star: return "Star of the City"
        case .impuritas: return "Impuritas Civitatis"
        case .end: return "The End"
        }
    }

    var confirmation: String {
        self == .burn ? "Burn Save? All data will be wiped" : "Overwrite to \(title)? All data will be lost"
    }

    var confirmLabel: String { self == .burn ? "Burn" : "Overwrite" }

    /// The tile's picture, ready to draw: loaded and prepared once for all slots (never per render)
    var icon: UIImage? { SaveSlot.icons[self] }

    /// The icon files are hexagons on black. Each is turned into an image whose alpha is its brightest channel (colors stay as they are), which over black looks exactly like the file
    /// and over anything else lets the black corners of tiles that overlap show what is underneath: so the tiles need no blend mode, which was slow to animate.
    static let icons: [SaveSlot: UIImage] = {
        var made: [SaveSlot: UIImage] = [:]
        for slot in SaveSlot.allCases {
            guard let url = SaveStore.resource("icon_\(slot.rawValue)", "jpg"), let source = UIImage(contentsOfFile: url.path)?.cgImage else { continue }
            let w = source.width, h = source.height
            guard let context = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4, space: CGColorSpaceCreateDeviceRGB(),
                                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue), let base = context.data else { continue }
            context.draw(source, in: CGRect(x: 0, y: 0, width: w, height: h))
            let pixels = base.assumingMemoryBound(to: UInt8.self)
            for i in 0..<(w * h) {   // the colors are already at most the brightest channel, so the image stays valid premultiplied
                let o = i * 4
                pixels[o + 3] = max(pixels[o], pixels[o + 1], pixels[o + 2])
            }
            if let image = context.makeImage() { made[slot] = UIImage(cgImage: image, scale: 1, orientation: .up) }
        }
        return made
    }()

    var available: Bool {
        self == .burn || SaveStore.resource("save_\(rawValue)", "dat") != nil
    }
}

enum SaveStoreError: LocalizedError {
    case missing(String)
    case oversized(String)

    var errorDescription: String? {
        switch self {
        case .missing(let name): return "\(name) is not included in this build."
        case .oversized(let name): return "The \(name) save is larger than a Switch save file."
        }
    }
}

enum SaveStore {
    static let fileName = "MySaveData"
    static let fileSize = 16 * 1024 * 1024
    static let folder = "SaveSlots"

    static func resource(_ name: String, _ ext: String) -> URL? {
        Bundle.main.url(forResource: name, withExtension: ext, subdirectory: folder)
    }

    static var directory: URL {
        if let path = getenv("DM_SAVE") { return URL(fileURLWithPath: String(cString: path), isDirectory: true) }
        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return documents.appendingPathComponent("save", isDirectory: true)
    }

    private static func existing() -> [URL] {
        guard let walker = FileManager.default.enumerator(at: directory, includingPropertiesForKeys: nil) else { return [] }
        return walker.compactMap { $0 as? URL }.filter { $0.lastPathComponent == fileName }
    }

    static func apply(_ slot: SaveSlot) throws {
        let fm = FileManager.default
        let found = existing()
        if slot == .burn {
            for url in found { try fm.removeItem(at: url) }
            fputs("launcher: saves: burned \(found.count) \(fileName) file(s)\n", stderr)
            return
        }
        guard let source = resource("save_\(slot.rawValue)", "dat") else { throw SaveStoreError.missing(slot.title) }
        var data = try Data(contentsOf: source)
        guard data.count <= fileSize else { throw SaveStoreError.oversized(slot.title) }
        data.append(Data(count: fileSize - data.count))
        try fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let target = found.first ?? directory.appendingPathComponent(fileName)
        for extra in found.dropFirst() { try fm.removeItem(at: extra) }
        try data.write(to: target, options: .atomic)
        fputs("launcher: saves: \(slot.title) written to \(target.path)\n", stderr)
    }
}

/// Where each tile is on screen (global coordinates), so the confirm ripple can start from the tile that was chosen.
private struct TileFramesKey: PreferenceKey {
    static var defaultValue: [SaveSlot: CGRect] = [:]
    static func reduce(value: inout [SaveSlot: CGRect], nextValue: () -> [SaveSlot: CGRect]) {
        value.merge(nextValue()) { $1 }
    }
}

/// The Manage saves page: pushed onto the launcher's navigation stack (Launcher.swift), so it fills the right-hand side like the mods pages do and the Play column stays on screen.
/// Its navigation bar is hidden, like the settings list's: a bar that slid in during the push shrank the page under the tiles and moved them mid-fade. The page has its own Back
/// button and takes a swipe from the left edge. `ripple` starts the hexagon ripple (HexRippleView) over the whole launcher.
/// Sounds (LauncherAudio.playEffect, files in Resources/SaveSlots): Popup_FloorLevelup_Start when the tiles start to fade in, Ui_BookApply on every tile tap; Page_Turn on leaving
/// is played by the launcher, which sees the page popped.
struct SaveSlotsPage: View {
    let ripple: (CGRect, UIImage?) -> Void

    @Environment(\.dismiss) private var dismiss
    @State private var selected: SaveSlot?
    @State private var confirming = false
    @State private var working = false
    @State private var failure: String?
    @State private var started = false              // the opening sound and the fade-in ran (onAppear can fire again)
    @State private var fadedIn = false              // one change, so the whole fade-in is one animation: each tile's own delay (fadeOrder) staggers it
    @State private var fadeOrder: [SaveSlot: Int] = Dictionary(uniqueKeysWithValues: SaveSlot.allCases.shuffled().enumerated().map { ($0.element, $0.offset) })   // new each time the page opens
    @State private var frames: [SaveSlot: CGRect] = [:]

    private let columns = 4
    private let rows = 2
    // The hexagon art fills 0.80 of a tile's width and 0.92 of its height, so tiles at a pitch of 1.0 / 0.87 left a gap of about 0.19 tile. A 2.5x smaller gap is a pitch of
    // 0.80 + 0.19 / 2.5 = 0.88 across; a regular honeycomb then steps 0.88 * sqrt(3) / 2 = 0.76 down a row.
    private let columnStep: CGFloat = 0.88
    private let rowStep: CGFloat = 0.76
    private let rowShift: CGFloat = 0.5
    private let headerHeight: CGFloat = 40
    private let footerHeight: CGFloat = 56
    private let fadeStart = 0.2        // seconds after the page appears: the push has finished by then
    private let fadeInterval = 0.09
    private let fadeDuration = 0.4

    private var failing: Binding<Bool> {
        Binding(get: { failure != nil }, set: { if !$0 { failure = nil } })
    }

    var body: some View {
        GeometryReader { geometry in
            let gridUnitsWide = (CGFloat(columns - 1) + rowShift) * columnStep + 1
            let gridUnitsHigh = 1 + rowStep * CGFloat(rows - 1)
            let cell = max(1, min((geometry.size.width - 24) / gridUnitsWide, (geometry.size.height - headerHeight - footerHeight) / gridUnitsHigh))
            VStack(spacing: 0) {
                ZStack {
                    Text(selected?.title ?? "Manage saves")
                        .font(.title3.weight(.bold))
                        .foregroundStyle(selected == nil ? Color.secondary : Color.white)
                    HStack {
                        Button { dismiss() } label: {
                            Label("Back", systemImage: "chevron.left").font(.body.weight(.semibold))
                        }
                        Spacer()
                    }
                }
                .padding(.horizontal, 12)
                .frame(height: headerHeight)
                Spacer(minLength: 0)
                grid(cell: cell)
                Spacer(minLength: 0)
                ZStack {
                    if selected != nil {
                        Button { confirming = true } label: {
                            Text("Proceed").font(.headline).frame(maxWidth: 180)
                        }
                        .buttonStyle(.borderedProminent)
                        .tint(.blue)
                        .controlSize(.regular)
                        .disabled(working)
                        .transition(.opacity)
                    }
                }
                .frame(height: footerHeight)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .hexRippleDistortion()
        }
        .background(Color(.systemGroupedBackground).ignoresSafeArea())   // the same black as the mods pages
        .toolbar(.hidden, for: .navigationBar)
        .navigationBarBackButtonHidden(true)
        .animation(.easeInOut(duration: 0.15), value: selected)
        .simultaneousGesture(DragGesture(minimumDistance: 24).onEnded { drag in   // a swipe in from the left edge goes back, as on the mods pages
            if drag.startLocation.x < 40 && drag.translation.width > 80 && abs(drag.translation.height) < 60 { dismiss() }
        })
        .onPreferenceChange(TileFramesKey.self) { frames = $0 }
        .task { await appear() }
        .alert(selected?.confirmation ?? "", isPresented: $confirming) {
            Button("Cancel", role: .cancel) {}
            Button(selected?.confirmLabel ?? "Confirm", role: .destructive) { commit() }
        }
        .alert("Save not changed", isPresented: failing) {
            Button("OK", role: .cancel) {}
        } message: {
            Text(failure ?? "")
        }
    }

    // The page opened: once the push is over, the opening sound, and all the tiles fade in at once, each after its own delay (a new random order every time)
    private func appear() async {
        guard !started else { return }
        started = true
        try? await Task.sleep(for: .seconds(fadeStart))
        guard !Task.isCancelled else { return }
        LauncherAudio.shared.playEffect("Popup_FloorLevelup_Start")
        fadedIn = true
    }

    private func grid(cell: CGFloat) -> some View {
        let slots = SaveSlot.allCases
        let width = ((CGFloat(columns - 1) + rowShift) * columnStep + 1) * cell
        let height = (1 + rowStep * CGFloat(rows - 1)) * cell
        return ZStack(alignment: .topLeading) {
            ForEach(Array(slots.enumerated()), id: \.element.id) { index, slot in
                let row = index / columns
                let column = index % columns
                tile(slot, cell: cell)
                    .offset(x: (CGFloat(column) + (row % 2 == 1 ? rowShift : 0)) * columnStep * cell, y: CGFloat(row) * rowStep * cell)
            }
        }
        .frame(width: width, height: height, alignment: .topLeading)
    }

    private func tile(_ slot: SaveSlot, cell: CGFloat) -> some View {
        let isSelected = selected == slot
        let target: Double = slot.available ? (isSelected ? 1.0 : 0.55) : 0.2
        return Button {
            LauncherAudio.shared.playEffect("Ui_BookApply")
            selected = isSelected ? nil : slot
        } label: {
            Group {
                if let icon = slot.icon {
                    Image(uiImage: icon).resizable().interpolation(.high).scaledToFit()
                } else {
                    Color.clear
                }
            }
            .frame(width: cell, height: cell)
            .contentShape(HexagonShape())
        }
        .buttonStyle(.plain)
        .disabled(!slot.available || working || !fadedIn)
        .opacity(fadedIn ? target : 0)
        .animation(.easeOut(duration: fadeDuration).delay(Double(fadeOrder[slot] ?? 0) * fadeInterval), value: fadedIn)
        .scaleEffect(isSelected ? 1.04 : 1.0)
        .background(GeometryReader { proxy in
            Color.clear.preference(key: TileFramesKey.self, value: [slot: proxy.frame(in: .global)])
        })
        .accessibilityLabel(slot.title)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }

    private func commit() {
        guard let slot = selected, !working else { return }
        working = true
        if let frame = frames[slot] { ripple(frame, slot.icon) }
        LauncherAudio.shared.playBookBurn()
        DispatchQueue.global(qos: .userInitiated).async {
            let result = Result { try SaveStore.apply(slot) }
            DispatchQueue.main.async {
                working = false
                switch result {
                case .success: selected = nil
                case .failure(let error): failure = error.localizedDescription
                }
            }
        }
    }
}

struct HexagonShape: Shape {
    func path(in rect: CGRect) -> Path {
        let w = rect.width, h = rect.height
        var path = Path()
        path.move(to: CGPoint(x: rect.minX + w * 0.5, y: rect.minY))
        path.addLine(to: CGPoint(x: rect.minX + w, y: rect.minY + h * 0.25))
        path.addLine(to: CGPoint(x: rect.minX + w, y: rect.minY + h * 0.75))
        path.addLine(to: CGPoint(x: rect.minX + w * 0.5, y: rect.minY + h))
        path.addLine(to: CGPoint(x: rect.minX, y: rect.minY + h * 0.75))
        path.addLine(to: CGPoint(x: rect.minX, y: rect.minY + h * 0.25))
        path.closeSubpath()
        return path
    }
}

/// One ripple on screen: when it started, and the tile it came from (global coordinates, with its picture, redrawn above the ripple).
struct HexRipple: Identifiable, Equatable {
    let id = UUID()
    let start = Date()
    let tile: CGRect
    let icon: UIImage?
    static func == (a: HexRipple, b: HexRipple) -> Bool { a.id == b.id }
}

private struct HexRippleKey: EnvironmentKey {
    static let defaultValue: HexRipple? = nil
}

extension EnvironmentValues {
    /// The ripple on screen, if any; the launcher sets it, so the parts that distort with the ripple (HexRippleDistortion) and the ripple itself share one clock.
    var hexRipple: HexRipple? {
        get { self[HexRippleKey.self] }
        set { self[HexRippleKey.self] = newValue }
    }
}

enum HexRippleTiming {
    static let duration = 0.8          // seconds
    static let band: CGFloat = 170     // how far behind the leading edge the hexagons take to fade out; also in HexRipple.metal
    /// The ripple's progress, 0...1, at a time; nil when it is over
    static func progress(of ripple: HexRipple, at date: Date) -> CGFloat? {
        let t = date.timeIntervalSince(ripple.start)
        return t >= 0 && t < duration ? CGFloat(t / duration) : nil
    }
    /// How far the leading edge has gone at a progress: fast at first, slowing down (the same curve is in HexRipple.metal)
    static func front(reach: CGFloat, progress: CGFloat) -> CGFloat { reach * (1 - (1 - progress) * (1 - progress)) }
}

/// Bends whatever it is on in a ring behind the ripple's leading edge (HexRipple.metal), for as long as a ripple is on screen. Only on plain SwiftUI content: a shader cannot read
/// a List or other UIKit-backed view. The clock runs only while a ripple is on.
struct HexRippleDistortion: ViewModifier {
    @Environment(\.hexRipple) private var ripple

    func body(content: Content) -> some View {
        TimelineView(.animation(paused: ripple == nil)) { context in
            let live = ripple.flatMap { r in HexRippleTiming.progress(of: r, at: context.date).map { (r, $0) } }
            content.visualEffect { view, proxy in
                let frame = proxy.frame(in: .global)
                let origin = live.map { CGPoint(x: $0.0.tile.midX - frame.minX, y: $0.0.tile.midY - frame.minY) } ?? .zero
                return view.distortionEffect(
                    ShaderLibrary.hexRippleDistort(.float2(origin), .float2(proxy.size), .float(live?.1 ?? 0), .float(HexRippleTiming.band)),
                    maxSampleOffset: CGSize(width: 16, height: 16), isEnabled: live != nil)
            }
        }
    }
}

extension View {
    func hexRippleDistortion() -> some View { modifier(HexRippleDistortion()) }
}

/// A honeycomb of hexagons that spreads outward over the whole launcher from the chosen tile and fades behind its leading edge, in the buttons' amber, with a glow around the hexagons
/// (wide soft strokes added on top of the picture), a bright shock ring at the leading edge and a flash where it starts. The chosen tile is drawn again above it, so the ripple
/// starts under the button. Drawn over everything and never hit-tested (Launcher.swift). `corner` is this view's origin in global coordinates.
struct HexRippleView: View {
    let ripple: HexRipple
    let corner: CGPoint

    private static let radius: CGFloat = 26          // hexagon circumradius, points
    private static let lead: CGFloat = 60            // how far the hexagons take to grow to full size at the leading edge
    private static let tileSettle = 0.25             // seconds for the redrawn tile to fall back to how an unselected tile looks

    var body: some View {
        TimelineView(.animation) { context in
            if let x = HexRippleTiming.progress(of: ripple, at: context.date) {
                let t = context.date.timeIntervalSince(ripple.start)
                let origin = CGPoint(x: ripple.tile.midX - corner.x, y: ripple.tile.midY - corner.y)
                ZStack {
                    Canvas(rendersAsynchronously: true) { gc, size in draw(gc, size: size, origin: origin, x: x) }
                    if let icon = ripple.icon {
                        let settle = min(1, t / Self.tileSettle)
                        Image(uiImage: icon).resizable().interpolation(.high).scaledToFit()
                            .frame(width: ripple.tile.width, height: ripple.tile.height)
                            .scaleEffect(1.04 - 0.04 * settle)
                            .opacity(1 - 0.45 * settle)
                            .position(origin)
                    }
                }
            }
        }
    }

    private func draw(_ gc: GraphicsContext, size: CGSize, origin: CGPoint, x: CGFloat) {
        let corners = [CGPoint(x: 0, y: 0), CGPoint(x: size.width, y: 0), CGPoint(x: 0, y: size.height), CGPoint(x: size.width, y: size.height)]
        let reach = (corners.map { hypot($0.x - origin.x, $0.y - origin.y) }.max() ?? 0) + HexRippleTiming.band
        let front = HexRippleTiming.front(reach: reach, progress: x)
        let fade = Double(1 - x)
        let amber = Color(red: 0.88, green: 0.66, blue: 0.32)   // the save buttons' amber
        var glow = gc
        glow.blendMode = .plusLighter   // the glow adds light to what is under it

        // the flash where it starts
        let flash = max(0, 1 - Double(x) * 2.5)
        if flash > 0 {
            let flashRadius = 90 + 140 * x
            let rect = CGRect(x: origin.x - flashRadius, y: origin.y - flashRadius, width: flashRadius * 2, height: flashRadius * 2)
            glow.fill(Path(ellipseIn: rect), with: .radialGradient(Gradient(colors: [amber.opacity(0.65 * flash), amber.opacity(0)]), center: origin, startRadius: 0, endRadius: flashRadius))
        }

        // the hexagons: a honeycomb with one hexagon centred on the tile
        let r = Self.radius
        let w = sqrt(3) * r
        let rowHeight = 1.5 * r
        let rowsEachWay = Int(max(origin.y, size.height - origin.y) / rowHeight) + 2
        let columnsEachWay = Int(max(origin.x, size.width - origin.x) / w) + 2
        for row in -rowsEachWay...rowsEachWay {
            let cy = origin.y + CGFloat(row) * rowHeight
            if cy < -r || cy > size.height + r { continue }
            for column in -columnsEachWay...columnsEachWay {
                let cx = origin.x + CGFloat(column) * w + (row & 1 != 0 ? w / 2 : 0)
                if cx < -r || cx > size.width + r { continue }
                let behind = front - hypot(cx - origin.x, cy - origin.y)   // how far the leading edge is past this hexagon
                if behind <= 0 { continue }
                let trail = max(0, 1 - behind / HexRippleTiming.band)
                if trail <= 0 { continue }
                let grow = min(1, behind / Self.lead)
                let edge = exp(-pow(Double((behind - 22) / 36), 2))   // 1 just behind the leading edge
                let radius = r * 0.92 * (0.35 + 0.65 * grow)
                var path = Path()
                for i in 0..<6 {
                    let angle = CGFloat(i) * .pi / 3 - .pi / 2
                    let point = CGPoint(x: cx + radius * cos(angle), y: cy + radius * sin(angle))
                    if i == 0 { path.move(to: point) } else { path.addLine(to: point) }
                }
                path.closeSubpath()
                let hot = Color(red: 0.88 + 0.12 * edge, green: 0.66 + 0.30 * edge, blue: 0.32 + 0.45 * edge)   // the edge burns toward white
                glow.stroke(path, with: .color(hot.opacity((0.05 + 0.20 * edge) * trail)), lineWidth: 10)
                glow.fill(path, with: .color(hot.opacity((0.06 + 0.28 * edge) * trail)))
                gc.stroke(path, with: .color(hot.opacity((0.35 + 0.60 * edge) * trail)), lineWidth: 1.5)
            }
        }

        // the shock ring on the leading edge
        if front > 0 && fade > 0 {
            let ring = Path(ellipseIn: CGRect(x: origin.x - front, y: origin.y - front, width: front * 2, height: front * 2))
            let strength = pow(fade, 1.5)
            glow.stroke(ring, with: .color(amber.opacity(0.10 * strength)), lineWidth: 26)
            glow.stroke(ring, with: .color(amber.opacity(0.22 * strength)), lineWidth: 10)
            glow.stroke(ring, with: .color(Color(red: 1, green: 0.93, blue: 0.78).opacity(0.55 * strength)), lineWidth: 2.5)
        }
    }
}
