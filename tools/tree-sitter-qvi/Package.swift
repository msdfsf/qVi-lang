// swift-tools-version:5.6

import Foundation
import PackageDescription

let dir = Context.packageDirectory
var sources = ["src/parser.c"]
if FileManager.default.fileExists(atPath: "\(dir)/src/scanner.c") {
    sources.append("src/scanner.c")
}

let package = Package(
    name: "TreeSitterQvi",
    products: [
        .library(name: "TreeSitterQvi", targets: ["TreeSitterQvi"]),
    ],
    dependencies: [
        .package(url: "https://github.com/tree-sitter/swift-tree-sitter", from: "0.10.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterQvi",
            dependencies: [],
            path: ".",
            sources: sources,
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterQviTests",
            dependencies: [
                .product(name: "SwiftTreeSitter", package: "swift-tree-sitter"),
                "TreeSitterQvi",
            ],
            path: "bindings/swift/TreeSitterQviTests"
        )
    ],
    cLanguageStandard: .c11
)
