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

    var icon: UIImage? {
        guard let url = SaveStore.resource("icon_\(rawValue)", "jpg") else { return nil }
        return UIImage(contentsOfFile: url.path)
    }

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
/// `ripple` starts the hexagon ripple (HexRippleView), over the whole launcher, from a point in global coordinates.
/// Sounds (LauncherAudio.playEffect, files in Resources/SaveSlots): Popup_FloorLevelup_Start when the page opens, Ui_BookApply on every tile tap; Page_Turn on leaving is played by the launcher, which sees the page popped.
struct SaveSlotsPage: View {
    let ripple: (CGPoint) -> Void

    @State private var selected: SaveSlot?
    @State private var confirming = false
    @State private var working = false
    @State private var failure: String?
    @State private var started = false              // the opening sound and the fade-in ran (onAppear can fire again)
    @State private var shown: Set<SaveSlot> = []    // tiles faded in so far
    @State private var frames: [SaveSlot: CGRect] = [:]

    private let columns = 4
    private let rows = 2
    // The hexagon art fills 0.80 of a tile's width and 0.92 of its height, so tiles at a pitch of 1.0 / 0.87 left a gap of about 0.19 tile. A 2.5x smaller gap is a pitch of
    // 0.80 + 0.19 / 2.5 = 0.88 across; a regular honeycomb then steps 0.88 * sqrt(3) / 2 = 0.76 down a row.
    private let columnStep: CGFloat = 0.88
    private let rowStep: CGFloat = 0.76
    private let rowShift: CGFloat = 0.5
    private let headerHeight: CGFloat = 36
    private let footerHeight: CGFloat = 56
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
                Text(selected?.title ?? "")
                    .font(.title3.weight(.bold))
                    .foregroundStyle(.white)
                    .frame(maxWidth: .infinity)
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
        }
        .background(Color(.systemGroupedBackground).ignoresSafeArea())   // the same black as the mods pages, so the tiles' blend mode below still draws on black
        .navigationTitle("Manage saves")
        .navigationBarTitleDisplayMode(.inline)
        .animation(.easeInOut(duration: 0.15), value: selected)
        .onPreferenceChange(TileFramesKey.self) { frames = $0 }
        .onAppear(perform: appear)
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

    // The page opened: the opening sound, and the tiles fade in one after another in a new random order each time
    private func appear() {
        guard !started else { return }
        started = true
        LauncherAudio.shared.playEffect("Popup_FloorLevelup_Start")
        for (index, slot) in SaveSlot.allCases.shuffled().enumerated() {
            withAnimation(.easeOut(duration: fadeDuration).delay(Double(index) * fadeInterval)) { _ = shown.insert(slot) }
        }
    }

    private func grid(cell: CGFloat) -> some View {
        let slots = SaveSlot.allCases
        let width = ((CGFloat(columns - 1) + rowShift) * columnStep + 1) * cell
        let height = (1 + rowStep * CGFloat(rows - 1)) * cell
        return ZStack(alignment: .topLeading) {
            Color.black
            ForEach(Array(slots.enumerated()), id: \.element.id) { index, slot in
                let row = index / columns
                let column = index % columns
                tile(slot, cell: cell)
                    .offset(x: (CGFloat(column) + (row % 2 == 1 ? rowShift : 0)) * columnStep * cell, y: CGFloat(row) * rowStep * cell)
            }
        }
        .frame(width: width, height: height, alignment: .topLeading)
        .compositingGroup()
    }

    private func tile(_ slot: SaveSlot, cell: CGFloat) -> some View {
        let isSelected = selected == slot
        let visible = shown.contains(slot)
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
        .disabled(!slot.available || working || !visible)
        .opacity(visible ? (slot.available ? (isSelected ? 1.0 : 0.55) : 0.2) : 0)
        .scaleEffect(isSelected ? 1.04 : 1.0)
        .blendMode(.exclusion)
        .background(GeometryReader { proxy in
            Color.clear.preference(key: TileFramesKey.self, value: [slot: proxy.frame(in: .global)])
        })
        .accessibilityLabel(slot.title)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }

    private func commit() {
        guard let slot = selected, !working else { return }
        working = true
        if let frame = frames[slot] { ripple(CGPoint(x: frame.midX, y: frame.midY)) }
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

/// One ripple: where it starts (global coordinates) and when.
struct HexRipple: Identifiable, Equatable {
    let id = UUID()
    let origin: CGPoint
    let start = Date()
}

/// A honeycomb of hexagons that spreads outward from `origin` over the whole launcher and fades out behind its leading edge, in the buttons' amber. Drawn over everything and
/// never hit-tested (Launcher.swift). `finished` is called once it has faded away completely.
struct HexRippleView: View {
    let ripple: HexRipple
    let origin: CGPoint     // in this view's own coordinates
    let finished: () -> Void

    private static let duration = 1.5
    private static let radius: CGFloat = 24          // hexagon circumradius, points
    private static let band: CGFloat = 180           // how far behind the leading edge the hexagons take to fade out
    private static let lead: CGFloat = 70            // how far the hexagons take to grow to full size at the leading edge
    private static let color = Color(red: 0.88, green: 0.66, blue: 0.32)   // the save buttons' amber

    var body: some View {
        TimelineView(.animation) { context in
            Canvas { gc, size in
                let t = context.date.timeIntervalSince(ripple.start)
                guard t >= 0, t < Self.duration else { return }
                let corners = [CGPoint(x: 0, y: 0), CGPoint(x: size.width, y: 0), CGPoint(x: 0, y: size.height), CGPoint(x: size.width, y: size.height)]
                let reach = (corners.map { hypot($0.x - origin.x, $0.y - origin.y) }.max() ?? 0) + Self.band
                let x = CGFloat(t / Self.duration)
                let front = reach * (1 - (1 - x) * (1 - x))   // fast at first, slowing down
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
                        let grow = min(1, behind / Self.lead)
                        let trail = max(0, 1 - behind / Self.band)
                        if trail <= 0 { continue }
                        let edge = exp(-pow(Double((behind - 24) / 40), 2))   // brightest just behind the leading edge
                        let radius = r * 0.92 * (0.35 + 0.65 * grow)
                        var path = Path()
                        for i in 0..<6 {
                            let angle = CGFloat(i) * .pi / 3 - .pi / 2
                            let point = CGPoint(x: cx + radius * cos(angle), y: cy + radius * sin(angle))
                            if i == 0 { path.move(to: point) } else { path.addLine(to: point) }
                        }
                        path.closeSubpath()
                        gc.fill(path, with: .color(Self.color.opacity((0.08 + 0.30 * edge) * trail)))
                        gc.stroke(path, with: .color(Self.color.opacity((0.35 + 0.60 * edge) * trail)), lineWidth: 1.5)
                    }
                }
            }
        }
        .task(id: ripple.id) {
            try? await Task.sleep(for: .seconds(Self.duration))
            if !Task.isCancelled { finished() }
        }
    }
}
