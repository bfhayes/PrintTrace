// swift-tools-version: 5.7
import PackageDescription

let package = Package(
    name: "OutlineToolSwift",
    platforms: [
        .macOS(.v10_15),
        .iOS(.v13)
    ],
    products: [
        .library(
            name: "OutlineToolSwift",
            targets: ["OutlineToolSwift"]
        ),
        .executable(
            name: "OutlineToolDemo",
            targets: ["OutlineToolDemo"]
        )
    ],
    targets: [
        .systemLibrary(
            name: "COutlineTool",
            pkgConfig: "liboutlinetool",
            providers: [
                .brew(["outlinetool"]),
                .apt(["liboutlinetool-dev"])
            ]
        ),
        .target(
            name: "OutlineToolSwift",
            dependencies: ["COutlineTool"],
            path: "Sources/OutlineToolSwift"
        ),
        .executableTarget(
            name: "OutlineToolDemo",
            dependencies: ["OutlineToolSwift"],
            path: "Sources/OutlineToolDemo"
        ),
        .testTarget(
            name: "OutlineToolSwiftTests",
            dependencies: ["OutlineToolSwift"],
            path: "Tests/OutlineToolSwiftTests"
        )
    ]
)