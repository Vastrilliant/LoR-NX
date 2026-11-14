import SwiftUI
import UIKit

enum SaveSlot: String, CaseIterable, Identifiable {
    case burn, canard, myth, legend, plague, nightmare, star, impuritas

    var id: String { rawValue }

    var title: String {
        switch self {
        case .burn: return "Burn Save"
        case .canard: return "Canard"
        case .myth: return "Urban Myth"
        case .legend: return "Urban Legend"
        case .plague: return "Urban Plague"
        case .nightmare: return "Urban Nightmare"
        case .star: return "Star of the City"
        case .impuritas: return "Impuritas Civitatis"
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

struct SaveSlotsPage: View {
    let close: () -> Void

    @State private var selected: SaveSlot?
    @State private var confirming = false
    @State private var working = false
    @State private var failure: String?

    private let columns = 4
    private let rows = 2
    private let headerHeight: CGFloat = 72
    private let footerHeight: CGFloat = 84

    private var failing: Binding<Bool> {
        Binding(get: { failure != nil }, set: { if !$0 { failure = nil } })
    }

    var body: some View {
        GeometryReader { geometry in
            let cell = min(geometry.size.width / CGFloat(columns), (geometry.size.height - headerHeight - footerHeight) / CGFloat(rows))
            ZStack(alignment: .topLeading) {
                Color.black.ignoresSafeArea()
                VStack(spacing: 0) {
                    Text(selected?.title ?? "")
                        .font(.title.weight(.bold))
                        .foregroundStyle(.white)
                        .frame(maxWidth: .infinity)
                        .frame(height: headerHeight)
                    Spacer(minLength: 0)
                    grid(cell: cell)
                    Spacer(minLength: 0)
                    ZStack {
                        if selected != nil {
                            Button { confirming = true } label: {
                                Text("Proceed").font(.title3.weight(.semibold)).frame(maxWidth: 220)
                            }
                            .buttonStyle(.borderedProminent)
                            .tint(.blue)
                            .controlSize(.large)
                            .disabled(working)
                            .transition(.opacity)
                        }
                    }
                    .frame(height: footerHeight)
                }
                Button(action: close) {
                    Label("Back", systemImage: "chevron.left").font(.body.weight(.semibold))
                }
                .padding(16)
            }
        }
        .animation(.easeInOut(duration: 0.15), value: selected)
        .preferredColorScheme(.dark)
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

    private func grid(cell: CGFloat) -> some View {
        let slots = SaveSlot.allCases
        return ZStack {
            Color.black
            VStack(spacing: 0) {
                ForEach(0..<rows, id: \.self) { row in
                    HStack(spacing: 0) {
                        ForEach(0..<columns, id: \.self) { column in
                            tile(slots[row * columns + column], cell: cell)
                        }
                    }
                }
            }
        }
        .frame(width: cell * CGFloat(columns), height: cell * CGFloat(rows))
        .compositingGroup()
    }

    private func tile(_ slot: SaveSlot, cell: CGFloat) -> some View {
        let isSelected = selected == slot
        return Button {
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
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(!slot.available || working)
        .opacity(slot.available ? (isSelected ? 1.0 : 0.55) : 0.2)
        .scaleEffect(isSelected ? 1.04 : 1.0)
        .blendMode(.exclusion)
        .accessibilityLabel(slot.title)
        .accessibilityAddTraits(isSelected ? .isSelected : [])
    }

    private func commit() {
        guard let slot = selected, !working else { return }
        working = true
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
