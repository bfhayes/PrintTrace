import SwiftUI
import UniformTypeIdentifiers

// MARK: - SwiftUI Example App

@available(macOS 12.0, iOS 15.0, *)
struct ContentView: View {
    @StateObject private var outlineTool = OutlineTool()
    @State private var selectedImageURL: URL?
    @State private var outputURL: URL?
    @State private var showingImagePicker = false
    @State private var showingFileSaver = false
    @State private var showingParametersSheet = false
    @State private var processingParams = OutlineTool.ProcessingParams()
    @State private var showingAlert = false
    @State private var alertMessage = ""
    @State private var isSuccess = false
    
    var body: some View {
        NavigationView {
            VStack(spacing: 20) {
                
                // Header
                VStack(spacing: 8) {
                    Image(systemName: "doc.text.image")
                        .font(.system(size: 48))
                        .foregroundColor(.blue)
                    
                    Text("OutlineTool")
                        .font(.largeTitle)
                        .fontWeight(.bold)
                    
                    Text("Convert image outlines to DXF format")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .padding(.top)
                
                Spacer()
                
                // Main Content
                VStack(spacing: 16) {
                    
                    // Image Selection
                    VStack(spacing: 8) {
                        Button(action: {
                            showingImagePicker = true
                        }) {
                            HStack {
                                Image(systemName: "photo")
                                Text(selectedImageURL?.lastPathComponent ?? "Select Image")
                            }
                            .frame(maxWidth: .infinity)
                            .padding()
                            .background(selectedImageURL != nil ? Color.green.opacity(0.1) : Color.blue.opacity(0.1))
                            .foregroundColor(selectedImageURL != nil ? .green : .blue)
                            .cornerRadius(8)
                        }
                        
                        if let url = selectedImageURL {
                            Text("Selected: \(url.lastPathComponent)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                    
                    // Parameters
                    VStack(spacing: 8) {
                        Button(action: {
                            showingParametersSheet = true
                        }) {
                            HStack {
                                Image(systemName: "slider.horizontal.3")
                                Text("Processing Parameters")
                            }
                            .frame(maxWidth: .infinity)
                            .padding()
                            .background(Color.orange.opacity(0.1))
                            .foregroundColor(.orange)
                            .cornerRadius(8)
                        }
                        
                        Text("Warp: \(processingParams.warpSize)px, Threshold: \(processingParams.thresholdValue)")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    
                    // Processing Status
                    if outlineTool.isProcessing {
                        VStack(spacing: 12) {
                            ProgressView(value: outlineTool.progress)
                                .progressViewStyle(LinearProgressViewStyle())
                            
                            HStack {
                                Text(outlineTool.currentStage)
                                    .font(.subheadline)
                                Spacer()
                                Text("\(Int(outlineTool.progress * 100))%")
                                    .font(.subheadline)
                                    .foregroundColor(.secondary)
                            }
                        }
                        .padding()
                        .background(Color.blue.opacity(0.05))
                        .cornerRadius(8)
                    }
                    
                    // Process Button
                    Button(action: processImage) {
                        HStack {
                            if outlineTool.isProcessing {
                                ProgressView()
                                    .scaleEffect(0.8)
                                Text("Processing...")
                            } else {
                                Image(systemName: "play.circle")
                                Text("Process Image to DXF")
                            }
                        }
                        .frame(maxWidth: .infinity)
                        .padding()
                        .background(canProcess ? Color.blue : Color.gray.opacity(0.3))
                        .foregroundColor(canProcess ? .white : .gray)
                        .cornerRadius(8)
                    }
                    .disabled(!canProcess)
                }
                .padding(.horizontal)
                
                Spacer()
                
                // Info
                VStack(spacing: 4) {
                    Text("Supported formats: JPG, PNG, TIFF")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    if let estimatedTime = estimatedProcessingTime {
                        Text("Estimated processing time: \(estimatedTime, specifier: "%.1f")s")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    
                    if let error = outlineTool.lastError {
                        Text("Last error: \(error.localizedDescription)")
                            .font(.caption)
                            .foregroundColor(.red)
                    }
                }
                .padding(.bottom)
            }
            .navigationTitle("OutlineTool")
            .fileImporter(
                isPresented: $showingImagePicker,
                allowedContentTypes: [.image],
                allowsMultipleSelection: false
            ) { result in
                handleImageSelection(result)
            }
            .fileExporter(
                isPresented: $showingFileSaver,
                document: DXFDocument(),
                contentType: UTType(filenameExtension: "dxf") ?? .data,
                defaultFilename: defaultOutputFilename
            ) { result in
                handleFileSave(result)
            }
            .sheet(isPresented: $showingParametersSheet) {
                ParametersView(params: $processingParams)
            }
            .alert("Processing Result", isPresented: $showingAlert) {
                Button("OK") { }
            } message: {
                Text(alertMessage)
            }
        }
    }
    
    // MARK: - Computed Properties
    
    private var canProcess: Bool {
        selectedImageURL != nil && !outlineTool.isProcessing
    }
    
    private var defaultOutputFilename: String {
        guard let url = selectedImageURL else { return "outline.dxf" }
        let baseName = url.deletingPathExtension().lastPathComponent
        return "\(baseName).dxf"
    }
    
    private var estimatedProcessingTime: Double? {
        guard let url = selectedImageURL else { return nil }
        let time = OutlineTool.estimateProcessingTime(for: url.path)
        return time > 0 ? time : nil
    }
    
    // MARK: - Methods
    
    private func handleImageSelection(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            selectedImageURL = urls.first
        case .failure(let error):
            alertMessage = "Failed to select image: \(error.localizedDescription)"
            showingAlert = true
        }
    }
    
    private func handleFileSave(_ result: Result<URL, Error>) {
        switch result {
        case .success(let url):
            outputURL = url
            processImageToSelectedOutput()
        case .failure(let error):
            alertMessage = "Failed to save file: \(error.localizedDescription)"
            showingAlert = true
        }
    }
    
    private func processImage() {
        showingFileSaver = true
    }
    
    private func processImageToSelectedOutput() {
        guard let inputURL = selectedImageURL,
              let outputURL = outputURL else {
            alertMessage = "Please select input and output files"
            showingAlert = true
            return
        }
        
        Task {
            do {
                try await outlineTool.processImageFromURL(
                    inputURL,
                    outputURL: outputURL,
                    params: processingParams
                )
                
                await MainActor.run {
                    alertMessage = "Successfully processed image to DXF!\nOutput: \(outputURL.lastPathComponent)"
                    isSuccess = true
                    showingAlert = true
                }
            } catch {
                await MainActor.run {
                    alertMessage = "Processing failed: \(error.localizedDescription)"
                    isSuccess = false
                    showingAlert = true
                }
            }
        }
    }
}

// MARK: - Parameters View

@available(macOS 12.0, iOS 15.0, *)
struct ParametersView: View {
    @Binding var params: OutlineTool.ProcessingParams
    @Environment(\.dismiss) private var dismiss
    
    var body: some View {
        NavigationView {
            Form {
                Section("Image Processing") {
                    VStack(alignment: .leading) {
                        Text("Warp Size: \(params.warpSize)")
                        Slider(
                            value: Binding(
                                get: { Double(params.warpSize) },
                                set: { params.warpSize = Int32($0) }
                            ),
                            in: 1000...5000,
                            step: 100
                        )
                        Text("Target image size after perspective correction")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    
                    VStack(alignment: .leading) {
                        Text("Real World Size: \(params.realWorldSizeMM, specifier: "%.1f") mm")
                        Slider(
                            value: $params.realWorldSizeMM,
                            in: 50...500,
                            step: 1
                        )
                        Text("Real-world size corresponding to warp size")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    
                    VStack(alignment: .leading) {
                        Text("Threshold: \(params.thresholdValue)")
                        Slider(
                            value: Binding(
                                get: { Double(params.thresholdValue) },
                                set: { params.thresholdValue = Int32($0) }
                            ),
                            in: 0...255,
                            step: 1
                        )
                        Text("Binary threshold value (0-255)")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
                
                Section("Noise Reduction") {
                    VStack(alignment: .leading) {
                        Text("Kernel Size: \(params.noiseKernelSize)")
                        Slider(
                            value: Binding(
                                get: { Double(params.noiseKernelSize) },
                                set: { params.noiseKernelSize = Int32($0) | 1 } // Ensure odd
                            ),
                            in: 3...51,
                            step: 2
                        )
                        Text("Morphological operation kernel size")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    
                    VStack(alignment: .leading) {
                        Text("Blur Size: \(params.blurSize)")
                        Slider(
                            value: Binding(
                                get: { Double(params.blurSize) },
                                set: { params.blurSize = Int32($0) | 1 } // Ensure odd
                            ),
                            in: 3...201,
                            step: 2
                        )
                        Text("Gaussian blur kernel size")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
                
                Section("Advanced") {
                    VStack(alignment: .leading) {
                        Text("Polygon Epsilon: \(params.polygonEpsilonFactor, specifier: "%.3f")")
                        Slider(
                            value: $params.polygonEpsilonFactor,
                            in: 0.001...0.1,
                            step: 0.001
                        )
                        Text("Polygon approximation factor")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
                
                Section {
                    Button("Reset to Defaults") {
                        params = OutlineTool.ProcessingParams()
                    }
                    .foregroundColor(.red)
                }
            }
            .navigationTitle("Parameters")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") {
                        dismiss()
                    }
                }
            }
        }
    }
}

// MARK: - DXF Document for File Export

struct DXFDocument: FileDocument {
    static var readableContentTypes: [UTType] { [UTType(filenameExtension: "dxf") ?? .data] }
    
    init() {}
    
    init(configuration: ReadConfiguration) throws {}
    
    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        return FileWrapper(regularFileWithContents: Data())
    }
}

// MARK: - Preview

@available(macOS 12.0, iOS 15.0, *)
struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}